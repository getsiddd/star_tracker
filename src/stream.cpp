#include "stream.hpp"

#include <cairo/cairo.h>
#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>

#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <memory>
#include <cstring>
#include <cmath>
#include <random>
#include <algorithm>
#include <map>
#include <chrono>

#include <boost/log/trivial.hpp>
#include <boost/thread.hpp>

//thred library
#include <tbb/parallel_for.h>
#include <tbb/concurrent_queue.h>

#include "attitude-estimators.hpp"
#include "attitude-utils.hpp"
#include "databases.hpp"

#include "star-id.hpp"
#include "star-utils.hpp"
#include "agent.hpp"

#include "config.pb.h"

namespace lost {

/// Convert a colored Cairo image surface into a row-major array of grayscale pixels.
/// Result is allocated with new[]

/**
 * Plot information about an image onto the image using Cairo.
 * Puts a box around each centroid, writes the attitude in the top left, etc.
 * @param red,green,blue The color to use when annotating the image. 0 represents none of that color, 1 represents that color in full.
 * @param alpha The transparency of annotations. 0 is completely transparent, 1 is completely opaque.
 * @param rawStarIndexes If true, print the catalog index. This is in contrast with the default behavior, which is to print the name of the catalog star.
 */
void StreamPlot(std::string description,
                 cairo_surface_t *cairoSurface,
                 const Stars &stars,
                 const StarIdentifiers *starIds,
                 const Catalog *catalog,
                 const Attitude *attitude,
                 double red,
                 double green,
                 double blue,
                 double alpha,
                 // if true, don't use catalog name
                 bool rawStarIndexes = false) {
    cairo_t *cairoCtx;
    std::string metadata = description + " ";

    cairoCtx = cairo_create(cairoSurface);
    cairo_set_source_rgba(cairoCtx, red, green, blue, alpha);
    cairo_set_line_width(cairoCtx, 1.0);
    cairo_set_antialias(cairoCtx, CAIRO_ANTIALIAS_NONE);
    cairo_font_options_t *cairoFontOptions = cairo_font_options_create();
    cairo_font_options_set_antialias(cairoFontOptions, CAIRO_ANTIALIAS_NONE);
    cairo_set_font_options(cairoCtx, cairoFontOptions);
    cairo_text_extents_t cairoTextExtents;
    cairo_text_extents(cairoCtx, "1234567890", &cairoTextExtents);
    float textHeight = cairoTextExtents.height;

    for (const Star &centroid : stars) {
        // plot the box around the star
        if (centroid.radiusX > 0.0) {
            float radiusX = centroid.radiusX;
            float radiusY = centroid.radiusY > 0.0 ?
                centroid.radiusY : radiusX;

            // Rectangles should be entirely /outside/ the radius of the star, so the star is
            // fully visible.
            cairo_rectangle(cairoCtx,
                            centroid.position.x - radiusX,
                            centroid.position.y - radiusY,
                            radiusX * 2,
                            radiusY * 2);
            cairo_stroke(cairoCtx);
        } else {
            cairo_rectangle(cairoCtx,
                            std::floor(centroid.position.x),
                            std::floor(centroid.position.y),
                            1, 1);
            cairo_fill(cairoCtx);
        }
    }

    metadata += std::to_string(stars.size()) + " centroids   ";

    if (starIds != NULL) {
        assert(catalog != NULL);

        for (const StarIdentifier &starId : *starIds) {
            const Star &centroid = stars[starId.starIndex];
            cairo_move_to(cairoCtx,

                          centroid.radiusX > 0.0
                          ? centroid.position.x + centroid.radiusX + 3
                          : centroid.position.x + 8,

                          centroid.radiusY > 0.0
                          ? centroid.position.y - centroid.radiusY + textHeight
                          : centroid.position.y + 10);

            int plotName = rawStarIndexes ? starId.catalogIndex : (*catalog)[starId.catalogIndex].name;
            cairo_show_text(cairoCtx, std::to_string(plotName).c_str());
        }
        metadata += std::to_string(starIds->size()) + " identified   ";
    }

    if (attitude != NULL && attitude->IsKnown()) {
        std::cout << attitude->IsKnown() << std::endl;
        EulerAngles spherical = attitude->ToSpherical();
        metadata +=
            "RA: " + std::to_string(RadToDeg(spherical.ra)) + "  " +
            "DE: " + std::to_string(RadToDeg(spherical.de)) + "  " +
            "Roll: " + std::to_string(RadToDeg(spherical.roll)) + "   ";
    }

    // plot metadata
    cairo_move_to(cairoCtx, 3, 3 + textHeight);
    cairo_show_text(cairoCtx, metadata.c_str());

    cairo_font_options_destroy(cairoFontOptions);
    cairo_destroy(cairoCtx);
}


// PIPELINE INPUT STUFF

/**
 * Calculate the focal length, in pixels, based on the given command line options.
 * This function exists because there are two ways to specify how "zoomed-in" the camera is. One way is using just FOV, which is useful when generating false images. Another is a combination of pixel size and focal length, which is useful for physical cameras.
 */

Image *ColorToGrayscaleImage(const Image* image) {
    BOOST_LOG_TRIVIAL(info) << "Converting color image to grayscale image";
    int width = image->width;
    int height = image->height;
    int channels = image->size > 0 ? image->size : 3;

    Image *grayscale = new Image();
    grayscale->image = new unsigned char[width * height];
    grayscale->width = width;
    grayscale->height = height;
    grayscale->size = 1;

    for (int i = 0; i < height * width; ++i) {
        if (channels >= 3) {
            const int src = i * channels;
            uint32_t pixel_r = image->image[src];
            uint32_t pixel_g = image->image[src + 1];
            uint32_t pixel_b = image->image[src + 2];
            // Use luminosity method for RGB input.
            grayscale->image[i] = static_cast<unsigned char>(std::round(pixel_r * 0.21 + pixel_g * 0.71 + pixel_b * 0.07));
        } else {
            grayscale->image[i] = image->image[i];
        }
    }

    return grayscale;
}

/**
 * A pipeline input coming from an image with no extra metadata. Only InputImage will be available.
 * No references to the surface are kept in the class and it may be freed after construction.
 * @param cairoSurface A cairo surface from the image file.
 * @todo should rename, not specific to PNG.
 */

StreamInput::StreamInput(Camera camera, const Catalog &catalog): camera(camera), catalog(catalog) {
    streamInput_id = lexical_cast<std::string>((random_generator())());
};

Image *StreamInput::ReadImageFromCamera(){
    while(1){
        if (camera.open()) {
            Image *image = camera.readImageFromV4L();
            Image *grayscale = ColorToGrayscaleImage(image);
            BOOST_LOG_TRIVIAL(info) << "Read Custom Image-2";
            return grayscale;
        }
        else {
            BOOST_LOG_TRIVIAL(error) << "Camera Not Detected: Will try again after 5 seconds";
            Agent::getDefaultAgent()->setStatusOutput("Camera Not Detected: Will try again after 5 seconds");
            Agent::getDefaultAgent()->setStarCatalogOutput(GetCatalog());
            sleep(5);
            return NULL;
        }
    }
}

/// Create a StreamInput using command line options.
StreamInputList GetStreamInput(const StreamOptions &values) {
    // I'm not sure why, but i can't get an initializer list to work here. Probably something to do
    // with copying unique ptrs
    StreamInputList result;
    std::string camera_id = values.configFile;

    systemConfiguration system;
    std::fstream in(values.configFile, std::ios::in | std::ios::binary);
    system.ParseFromIstream(&in);
    for (int i=0; i < system.cameras_size();i++){
        auto camera = system.cameras(i);
        std::string deviceID = camera.deviceid();
        int xResolution = camera.xresolution();
        int yResolution = camera.yresolution();
        Camera cam = Camera(camera.focallength(), xResolution, yResolution, deviceID);
        result.push_back(std::unique_ptr<StreamInput>(new StreamInput(cam, CatalogRead())));
    }
    BOOST_LOG_TRIVIAL(info) << "Number of Input Cameras: " << result.size() << std::endl;
    return result;
}

/// A star used in simulated image generation. Contains extra data about how to simulate the star.
class GeneratedStar : public Star {
public:
    GeneratedStar(Star star, float peakBrightness, Vec2 motionBlurDelta)
        : Star(star), peakBrightness(peakBrightness), delta(motionBlurDelta) { };

    /// the brightness density per time unit at the center of the star. 0.0 is black, 1.0 is white.
    float peakBrightness;

    /// (only meaningful with motion blur) Where the star will appear one time unit in the future.
    Vec2 delta;
};

// In the equations for pixel brightness both with motion blur enabled and disabled, we don't need
// any constant scaling factor outside the integral because when d0=0, the brightness at the center
// will be zero without any scaling. The scaling factor you usually see on a Normal distribution is
// so that the Normal distribution integrates to one over the real line, making it a probability
// distribution. But we want the /peak/ to be one, not the integral. motion blur enabled

/**
 * Calculates the indefinite integral of brightness density at a point due to a single star.
 * When oversampling is disabled, this is called only at pixel centers. When oversampling is enabled, it's called at multiple points in each pixel and then averaged. If multiple stars are near each other, the brightnesses can just be added then clamped.
 * See https://wiki.huskysat.org/wiki/index.php/Motion_Blur_and_Rolling_Shutter#Motion_Blur_Math to learn how these equations were derived.
 * @param pixel The point to calculate brightness density at. If only calculating per-pixel, should be the center of the pixel. ie, Vec2(10.5,5.5) would be appropriate for the pixel (10,5)
 * @param generatedStar the star to calculate brightness based on.
 * @param t Since this function computes the indefinite integral, this is the value it's evaluated at. To calculate the definite integral, which is what you want, call this function twice with different values for the `t` parameter then find the difference.
 * @param stddev The standard deviation of spread of the star. Higher values make stars more spread out. See command line documentation.
 * @return Indefinite integral of brightness density.
 */
static float MotionBlurredPixelBrightness(const Vec2 &pixel, const GeneratedStar &generatedStar,
                                          float t, float stddev) {
    const Vec2 &p0 = generatedStar.position;
    const Vec2 &delta = generatedStar.delta;
    const Vec2 d0 = p0 - pixel;
    return generatedStar.peakBrightness
        * stddev*std::sqrt(M_PI) / (std::sqrt(2)*delta.Magnitude())
        * std::exp(std::pow(d0.x*delta.x + d0.y*delta.y, 2) / (2*stddev*stddev*delta.MagnitudeSq())
              - d0.MagnitudeSq() / (2*stddev*stddev))
        * std::erf((t*delta.MagnitudeSq() + d0.x*delta.x + d0.y*delta.y) / (stddev*std::sqrt(2)*delta.Magnitude()));
}

/// Like motionBlurredPixelBrightness, but for when motion blur is disabled.
static float StaticPixelBrightness(const Vec2 &pixel, const GeneratedStar &generatedStar,
                                   float t, float stddev) {
    const Vec2 d0 = generatedStar.position - pixel;
    return generatedStar.peakBrightness * t * std::exp(-d0.MagnitudeSq() / (2 * stddev * stddev));
}

/**
 * Compute how likely a star is to be imaged, given a "cutoff" magnitude that the camera can see half of.
 *
 * The theory is that there's a threshold of total light energy that must be received to image a
 * star. The main random factors are shot noise and read noise, but to simplify things so we don't
 * need to think about photons, we only focus on read noise, which we assume has a standard
 * deviation 1/5th of the cutoff brightness. We compute the probability that, taking read noise into
 * account, the observed energy would be less than the cutoff energy.
 */
static float CentroidImagingProbability(float mag, float cutoffMag) {
    float brightness = MagToBrightness(mag);
    float cutoffBrightness = MagToBrightness(cutoffMag);
    float stddev = cutoffBrightness/(5.0);
    // CDF of Normal distribution with given mean and stddev
    return 1 - ((0.5) * (1 + std::erf((cutoffBrightness-brightness)/(stddev*std::sqrt(2.0)))));
}

const int kMaxBrightness = 255;


typedef StreamInputList (*StreamInputFactory)();

/**
 * Construct a pipeline using the given algorithms, some of which may be null.
 * @param database A pointer to the raw bytes of the database the star ID algorithm expects. If the database is NULL or not the type of database the star ID algorithm expects (almost always a multi-database), you'll get an error trying to identify stars later.
 */
Stream::Stream(CentroidAlgorithm *centroidAlgorithm,
                   StarIdAlgorithm *starIdAlgorithm,
                   AttitudeEstimationAlgorithm *attitudeEstimationAlgorithm,
                   unsigned char *database)
    : Stream() {
    if (centroidAlgorithm) {
        this->centroidAlgorithm = std::unique_ptr<CentroidAlgorithm>(centroidAlgorithm);
    }
    if (starIdAlgorithm) {
        this->starIdAlgorithm = std::unique_ptr<StarIdAlgorithm>(starIdAlgorithm);
    }
    if (attitudeEstimationAlgorithm) {
        this->attitudeEstimationAlgorithm = std::unique_ptr<AttitudeEstimationAlgorithm>(attitudeEstimationAlgorithm);
    }
    if (database) {
        this->database = std::unique_ptr<unsigned char[]>(database);
    }
}


/// Create a pipeline from command line options.
Stream SetStream(const StreamOptions &values) {
    Stream result;

    // TODO: more flexible or sth
    // TODO: don't allow setting star-id until database is set, and perhaps limit the star-id
    // choices to those compatible with the database?
    //

    // centroid algorithm stage
    if (values.centroidAlgo == "dummy") {
        result.centroidAlgorithm = std::unique_ptr<CentroidAlgorithm>(new DummyCentroidAlgorithm(values.centroidDummyNumStars));
    } else if (values.centroidAlgo == "cog") {
        result.centroidAlgorithm = std::unique_ptr<CentroidAlgorithm>(new CenterOfGravityAlgorithm());
    } else if (values.centroidAlgo == "iwcog") {
        result.centroidAlgorithm = std::unique_ptr<CentroidAlgorithm>(new IterativeWeightedCenterOfGravityAlgorithm());
    } else if (values.centroidAlgo != "") {
        BOOST_LOG_TRIVIAL(info) << "Illegal centroid algorithm.";
    }

    // centroid magnitude filter stage
    if (values.centroidMagFilter > 0) result.centroidMinMagnitude = values.centroidMagFilter;
    if (values.centroidFilterBrightest > 0) result.centroidMinStars = values.centroidFilterBrightest;

    // database stage
    if (values.databasePath != "") {
        std::fstream fs;
        fs.open(values.databasePath, std::fstream::in | std::fstream::binary);
        fs.seekg(0, fs.end);
        long length = fs.tellg();
        fs.seekg(0, fs.beg);

        BOOST_LOG_TRIVIAL(info) << "Reading " << length << " bytes of database.";

        result.database = std::unique_ptr<unsigned char[]>(new unsigned char[length]);
        fs.read((char *)result.database.get(), length);
        BOOST_LOG_TRIVIAL(info) << "Done";
    }

    if (values.idAlgo == "dummy") {
        result.starIdAlgorithm = std::unique_ptr<StarIdAlgorithm>(new DummyStarIdAlgorithm());
    } else if (values.idAlgo == "gv") {
        result.starIdAlgorithm = std::unique_ptr<StarIdAlgorithm>(new GeometricVotingStarIdAlgorithm(DegToRad(values.angularTolerance)));
    } else if (values.idAlgo == "py") {
        result.starIdAlgorithm = std::unique_ptr<StarIdAlgorithm>(new PyramidStarIdAlgorithm(DegToRad(values.angularTolerance), values.estimatedNumFalseStars, values.maxMismatchProb, 1000));
    } else if (values.idAlgo != "") {
        BOOST_LOG_TRIVIAL(info) << "Illegal id algorithm.";
        exit(1);
    }

    if (values.attitudeAlgo == "dqm") {
        result.attitudeEstimationAlgorithm = std::unique_ptr<AttitudeEstimationAlgorithm>(new DavenportQAlgorithm());
    } else if (values.attitudeAlgo == "triad") {
        result.attitudeEstimationAlgorithm = std::unique_ptr<AttitudeEstimationAlgorithm>(new TriadAlgorithm());
    } else if (values.attitudeAlgo == "quest") {
        result.attitudeEstimationAlgorithm = std::unique_ptr<AttitudeEstimationAlgorithm>(new QuestAlgorithm());
    } else if (values.attitudeAlgo != "") {
        BOOST_LOG_TRIVIAL(info) << "Illegal attitude algorithm.";
        exit(1);
    }
    BOOST_LOG_TRIVIAL(info) << "Stream Parameters Set.";
    return result;
}

static void PrintAttitude(std::ostream &os, const std::string &prefix, const Attitude &attitude) {
    if (attitude.IsKnown()) {
        os << prefix << "attitude_known 1" << std::endl;

        EulerAngles spherical = attitude.ToSpherical();
        os << prefix << "attitude_ra " << RadToDeg(spherical.ra) << std::endl;
        os << prefix << "attitude_de " << RadToDeg(spherical.de) << std::endl;
        os << prefix << "attitude_roll " << RadToDeg(spherical.roll) << std::endl;

        Quaternion q = attitude.GetQuaternion();
        os << prefix << "attitude_i " << q.i << std::endl;
        os << prefix << "attitude_j " << q.j << std::endl;
        os << prefix << "attitude_k " << q.k << std::endl;
        os << prefix << "attitude_real " << q.real << std::endl;

    } else {
        os << prefix << "attitude_known 0" << std::endl;
    }
}

/**
 * Run all stages of a pipeline. This is the "main" method for pipelines.
 * In space (or when using an image file as input), the StreamInput will contain only an InputImage. In this case, `Go` runs each star tracking algorithm in turn, passing the result of each step into the next one.
 * When running on a generated image (or any pipeline input where methods other than InputImage are available), or using a Stream where some algorithms are not set, the behavior is more nuanced. Each algorithm will be run on the return value of the corresponding input method from the StreamInput object, unless an earlier algorithm in the Stream returned a result, in which case that intermediate value is used instead of the value from the StreamInput.
 */
StreamOutput Stream::SingleRun(StreamInput &input) {
    // Start executing the pipeline at the first stage that has both input and an algorithm. From
    // there, execute each successive stage of the pipeline using the output of the last stage
    // (human centipede) until there are no more stages set.
    StreamOutput result;
    Image *inputImage = input.ReadImageFromCamera();
    if (inputImage == NULL){
        testInteger.push_back(rand() % 100);
        Attitude ts;
        attitudes.push_back(ts);
//        streamOutputs.push_back(result);
        return result;
    }
    rawImages.push_back(*inputImage);
    camers.push_back(*input.InputCamera());
    Stars *inputStars = input.InputStars();
    StarIdentifiers *inputStarIds = input.InputStarIds();

    // if database is provided, that's where we get catalog from.
    if (database) {
        MultiDatabase multiDatabase(database.get());
        const unsigned char *catalogBuffer = multiDatabase.SubDatabasePointer(kStarCatalogMagicValue);
        if (catalogBuffer != NULL) {
            DeserializeContext des(catalogBuffer);
            result.catalog = DeserializeCatalog(&des, NULL, NULL);
        } else {
            BOOST_LOG_TRIVIAL(error) << "WARNING: That database does not include a catalog. Proceeding with the full catalog.";
            result.catalog = input.GetCatalog();
        }
    } else {
        result.catalog = input.GetCatalog();
    }

    if (centroidAlgorithm && inputImage) {
        std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::steady_clock::now();

        // TODO: we should probably modify Go to just take an image argument
        Stars unfilteredStars = centroidAlgorithm->Go(inputImage->image, inputImage->width, inputImage->height);

        std::chrono::time_point<std::chrono::steady_clock> end = std::chrono::steady_clock::now();
        result.centroidingTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        // MAGNITUDE FILTERING
        int minMagnitude = centroidMinMagnitude;
        if (centroidMinStars > 0
            // don't need to filter if we don't even have that many stars
            && centroidMinStars < (int)unfilteredStars.size()) {

            Stars magSortedStars = unfilteredStars;
            // sort descending
            std::sort(magSortedStars.begin(), magSortedStars.end(), [](const Star &a, const Star &b) { return a.magnitude > b.magnitude; });
            minMagnitude = std::max(minMagnitude, magSortedStars[centroidMinStars - 1].magnitude);
        }
        // determine the minimum magnitude according to sorted stars
        Stars *filteredStars = new std::vector<Star>();
        for (const Star &star : unfilteredStars) {
            assert(star.magnitude >= 0); // catalog stars can have negative magnitude, but by our
                                         // conventions, centroids shouldn't.
            if (star.magnitude >= minMagnitude) {
                filteredStars->push_back(star);
            }
        }
        result.stars = std::unique_ptr<Stars>(filteredStars);
        inputStars = filteredStars;

        // any starid set up to this point needs to be discarded, because it's based on input
        // centroids instead of our new centroids.
        inputStarIds = NULL;
        result.starIds = NULL;
    } else if (centroidAlgorithm) {
        BOOST_LOG_TRIVIAL(error) << "ERROR: Centroid algorithm specified, but no input image to run it on.";
        exit(1);
    }

    if (starIdAlgorithm && database && inputStars && input.InputCamera()) {
        // TODO: don't copy the vector!
        std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::steady_clock::now();

        result.starIds = std::unique_ptr<StarIdentifiers>(new std::vector<StarIdentifier>(
            starIdAlgorithm->Go(database.get(), *inputStars, result.catalog, *input.InputCamera())));

        std::chrono::time_point<std::chrono::steady_clock> end = std::chrono::steady_clock::now();
        result.starIdTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        inputStarIds = result.starIds.get();
    } else if (starIdAlgorithm) {
        BOOST_LOG_TRIVIAL(error) << "ERROR: Star ID algorithm specified but cannot run because database, centroids, or camera are missing.";
        exit(1);
    }

    if (attitudeEstimationAlgorithm && inputStarIds && input.InputCamera()) {
        assert(inputStars); // ensure that starIds doesn't exist without stars
        std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::steady_clock::now();

        result.attitude = std::unique_ptr<Attitude>(new Attitude(attitudeEstimationAlgorithm->Go(*input.InputCamera(), *inputStars, result.catalog, *inputStarIds)));
        attitudes.push_back(*result.attitude);

        std::chrono::time_point<std::chrono::steady_clock> end = std::chrono::steady_clock::now();
        result.attitudeEstimationTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    } else if (attitudeEstimationAlgorithm) {
        BOOST_LOG_TRIVIAL(error) << "ERROR: Attitude estimation algorithm set, but either star IDs or camera are missing. One reason this can happen: Setting a centroid algorithm and attitude algorithm, but no star-id algorithm -- that can't work because the input star-ids won't properly correspond to the output centroids!";
        exit(1);
    }

    PrintAttitude(std::cout, "", *result.attitude);

    Agent::getDefaultAgent()->setStarCatalogOutput(result.catalog);
    Agent::getDefaultAgent()->setStatusOutput("Hello World");

    //streamOutputs.push_back(result);

    return result;
}

void Stream::StreamDataReset(){
    streamOutputs.clear();
    rawImages.clear();
    processedImages.clear();
    attitudes.clear();
    camers.clear();
    testInteger.clear();
}

void Stream::StreamDataSend(){
    Agent::getDefaultAgent()->setRawImagesOutput(rawImages);
    Agent::getDefaultAgent()->setProcessedImagesOutput(processedImages);
    Agent::getDefaultAgent()->setAttitudesOutput(attitudes);
}

void Stream::FinalCompute(){
    std::cout << "Final Computation" << std::endl;
}

/// Convenience function to run the main `Stream::Go` function on each input
void Stream::Go(StreamInputList &inputs) {
    std::vector<StreamOutput> results;
    while(1){
        tbb::parallel_for(size_t(0), inputs.size(), [&inputs, this](size_t i){
            this->SingleRun(*inputs[i]);
        });
        FinalCompute();
        StreamDataSend();
        StreamDataReset();
    }
}

/////////////////////
// PIPELINE OUTPUT //
/////////////////////

typedef void (*StreamComparator)(std::ostream &os,
                                   const StreamInputList &,
                                   const std::vector<StreamOutput> &,
                                   const StreamOptions &);

/// Plotter suitable for `cairo_surface_write_to_png_stream` which simply writes to an std::ostream
static cairo_status_t OstreamPlotter(void *closure, const unsigned char *data, unsigned int length) {
    std::ostream *os = (std::ostream *)closure;
    os->write((const char *)data, length);
    return CAIRO_STATUS_SUCCESS;
}

} // namespace lost
