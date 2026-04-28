// I/O stuff, such as Cairo and star catalog interactions.

#ifndef STREAM_H
#define STREAM_H

#include <cairo/cairo.h>

#include <random>
#include <vector>
#include <map>
#include <utility>
#include <string>
#include <sstream>
#include <iostream>
#include <memory>

#if defined(__linux__) // Or #if __linux__
#include <linux/videodev2.h>
#include <fcntl.h>
#endif

#ifndef CAIRO_HAS_PNG_FUNCTIONS
#error LOST requires Cairo to be compiled with PNG support
#endif

#include "centroiders.hpp"
#include "star-utils.hpp"
#include "star-id.hpp"
#include "camera.hpp"
#include "image.hpp"
#include "attitude-utils.hpp"
#include "attitude-estimators.hpp"
#include "databases.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

using boost::lexical_cast;
using boost::uuids::uuid;
using boost::uuids::random_generator;

namespace lost {

// use the environment variable LOST_BSC_PATH, or read from ./bright-star-catalog.tsv
const Catalog &CatalogRead();
// Convert a cairo surface to array of grayscale bytes
Image *ColorToGrayscaleImage(const Image* image);

// take an astrometry download from the bash script, and parse it into stuff.
// void v_astrometry_parse(std::string
//                         cairo_surface_t **pcairoSurface,   // image data
//                         Star      **ppx_centroids, // centroids according to astrometry
//                         int             *pi_centroids_length); // TODO: fov, actual angle, etc

// type for functions that create a centroid algorithm (by prompting the user usually)

////////////////////
// STREAM INPUT //
////////////////////

/// The command line options passed when running a stream
class StreamOptions {
public:
#define LOST_CLI_OPTION(name, type, prop, defaultVal, converter, defaultArg) \
    type prop = defaultVal;
#include "./stream-options.hpp"
#undef LOST_CLI_OPTION
};

/**
 * Represents the input and expected outputs of a pipeline run.
 * This is all the data about the pipeline we are about to run that can be gathered without actually running the pipeline. The "input" members return the things that will be fed into the pipeline. The "expected" methods return the "correct" outputs, i.e. what a perfect star tracking algorithm would output (this is only meaningful whe the image is generated, otherwise we don't know the correct output!). The "expected" methods are used to evaluate the quality of our algorithms. Some of the methods (both input and expected) may return NULL for certain subclasses.
 * By default, the "expected" methods return the corresponding inputs, which is reasonable behavior unless you are trying to intentionally introduce error into the inputs.
 */
class StreamInput {
public:
    StreamInput(Camera camera, const Catalog &catalog);
    ~StreamInput(){};

    Image *InputImage() { return NULL; };
    /// The catalog to which catalog indexes returned from other methods refer.
    const Catalog &GetCatalog() { return catalog; };
    Stars *InputStars() { return NULL; };
    /// The centroid indices in the StarIdentifiers returned from InputStarIds should be indices
    /// into InputStars(), not ExpectedStars(), when present, because otherwise it's useless.
    StarIdentifiers *InputStarIds() { return NULL; };
    /// Only used in tracking mode, in which case it is an estimate of the current attitude based on the last attitude, IMU info, etc.
    Attitude *InputAttitude() { return NULL; };
    Camera *InputCamera() { return &camera; };

    Stars *ExpectedStars() { return InputStars(); };
    /// Centroid indices in the StarIdentifiers returned from ExpectedStarIds should be indices into
    /// ExpectedStars(), /not/ InputStars(). This is in contrast to InputStarIds. If you need to
    /// compare ExpectedStarIds against the input stars, then you should use some function which
    /// uses simple heuristics to match the input stars and expected stars (eg based on distance).
    /// Cf how the star-ID comparator works for a reference implementation.
    StarIdentifiers *ExpectedStarIds() { return InputStarIds(); };
    Attitude *ExpectedAttitude();
    Image *ReadImageFromCamera();
    int id;

protected:
    std::string streamInput_id;

private:
    Image image;
    Camera camera;
    const Catalog &catalog;
};

typedef std::vector<std::unique_ptr<StreamInput>> StreamInputList;

StreamInputList GetStreamInput(const StreamOptions &values);

/////////////////////
// PIPELINE OUTPUT //
/////////////////////

/**
 * @brief The result of running a pipeline.
 * @details Also stores intermediate outputs, not just the final attitude.
 */



struct StreamOutput {
    std::unique_ptr<Stars> stars = nullptr;
    std::unique_ptr<StarIdentifiers> starIds = nullptr;
    std::unique_ptr<Attitude> attitude = nullptr;

    /// How many nanoseconds the centroiding stage of the pipeline took. Similarly for the other
    /// fields. If negative, the centroiding stage was not run.
    long long centroidingTimeNs = -1;
    long long starIdTimeNs = -1;
    long long attitudeEstimationTimeNs = -1;

    /**
     * @brief The catalog that the indices in starIds refer to
     * @todo Don't store it here
     */
    Catalog catalog;
};

//////////////
// PIPELINE //
//////////////

/**
 * @brief A set of algorithms that describes all or part of the star-tracking "pipeline"
 * @details A centroiding algorithm identifies the (x,y) pixel coordinates of each star detected in the raw image. The star id algorithm then determines which centroid corresponds to which catalog star. Finally, the attitude estimation algorithm determines the orientation of the camera based on the centroids and identified stars.
 */
class Stream {
    friend Stream SetStream(const StreamOptions &values);

public:
    Stream() = default;
    Stream(CentroidAlgorithm *, StarIdAlgorithm *, AttitudeEstimationAlgorithm *, unsigned char *);
//    StreamOutput Go(StreamInput &);
    StreamOutput SingleRun(StreamInput &);
    void Go(StreamInputList &);
    void StreamDataSend();
    void StreamDataReset();
    void FinalCompute();

protected:
    static std::atomic<int> stream_id;

private:
    std::unique_ptr<CentroidAlgorithm> centroidAlgorithm;

    // next two options are for magnitude filter:
    int centroidMinMagnitude = 0;
    int centroidMinStars = 0;

    std::unique_ptr<StarIdAlgorithm> starIdAlgorithm;
    std::unique_ptr<AttitudeEstimationAlgorithm> attitudeEstimationAlgorithm;
    std::unique_ptr<unsigned char[]> database;

    std::vector<StreamOutput> streamOutputs;
    std::vector<Image> rawImages;
    std::vector<Image> processedImages;
    std::vector<Attitude> attitudes;
    std::vector<Camera> camers;
    std::vector<int> testInteger;

    StreamOutput finalOutput;

    Catalog commonCatalog;

};

Stream SetStream(const StreamOptions &values);

// TODO: rename. Do something with the output
void StreamComparison(const StreamInputList &expected,
                        const std::vector<StreamOutput> &actual,
                        const StreamOptions &values);

/**
 * Compare expected and actual star identifications.
 * Useful for debugging and benchmarking.
 *
 * The following description is compatible with, but more actionable than, the definitions in the
 * documentation for StarIdComparison. A star-id is *correct* if the centroid is the closest
 * centroid to some expected centroid, and the referenced catalog star is the same one as in the
 * expected star-ids for that centroid. Also permissible is if the centroid is not the closest to
 * any expected centroid, but it has the same star-id as another star closer to the closest expected
 * centroid. All other star-ids are *incorrect* (because they are either identifying false stars, or
 * are incorrect identifications on true stars)
 *
 * The "total" in the result is just the number of input stars.
 */




}

#endif


