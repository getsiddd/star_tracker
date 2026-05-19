#include "lost.hpp"
#include "ecef-integration.hpp"

#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>

#include <bitset>
#include <algorithm>
#include <string>
#include <iostream>
#include <fstream>
#include <chrono>
#include <cstring>
#include <cctype>
#include <map>
#include <filesystem>
#include <atomic>
#include <thread>
#include <vector>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>

#include <boost/thread.hpp>

// Protobuf Libraries
#include "src/config.pb.h"

#include <man-database.h>
#include <man-pipeline.h>
#include <man-stream.h>

namespace lost {

    Lost* _pDefaultLost = NULL;

    static std::string ShellEscape(const std::string &value) {
        std::string escaped = "'";
        for (char c : value) {
            if (c == '\'') {
                escaped += "'\\''";
            } else {
                escaped += c;
            }
        }
        escaped += "'";
        return escaped;
    }

    struct BlindSolveBatchCliValues {
        #define LOST_CLI_OPTION(name, type, prop, defaultVal, converter, defaultArg) type prop = defaultVal;
        #define LOST_OPTIONS_BLIND_SOLVE_BATCH
        #include "options.hpp"
        #undef LOST_OPTIONS_BLIND_SOLVE_BATCH
        #undef LOST_CLI_OPTION
    };

    struct CameraYmlCliValues {
        #define LOST_CLI_OPTION(name, type, prop, defaultVal, converter, defaultArg) type prop = defaultVal;
        #define LOST_OPTIONS_CAMERA_YML
        #include "options.hpp"
        #undef LOST_OPTIONS_CAMERA_YML
        #undef LOST_CLI_OPTION
    };

    template <typename T>
    bool BuildBlindSolveProfileDefaults(const std::string &requestedProfile,
                                        T &profileDefaults,
                                        std::string &errorOut) {
        std::string normalizedProfile = requestedProfile;
        std::transform(normalizedProfile.begin(), normalizedProfile.end(), normalizedProfile.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        if (normalizedProfile.empty() || normalizedProfile == "default" || normalizedProfile == "generic" || normalizedProfile == "auto") {
            profileDefaults.profile = "default";
            return true;
        }

        if (normalizedProfile == "wide" || normalizedProfile == "wide-field") {
            profileDefaults.profile = "wide-field";
            profileDefaults.scaleLowArcsecPerPix = 20.0f;
            profileDefaults.scaleHighArcsecPerPix = 400.0f;
            profileDefaults.downsample = 2;
            profileDefaults.timeoutSeconds = 180;
            profileDefaults.preprocessEnabled = true;
            profileDefaults.preprocessMode = "global";
            profileDefaults.preprocessTargetWidth = 720;
            profileDefaults.preprocessMaxDimension = 2200;
            profileDefaults.tileWidth = 1800;
            profileDefaults.tileHeight = 1800;
            profileDefaults.tileOverlap = 300;
            profileDefaults.maxStarCount = 40;
            profileDefaults.minStarSeparation = 0.15f;
            return true;
        }

        if (normalizedProfile == "narrow" || normalizedProfile == "narrow-field") {
            profileDefaults.profile = "narrow-field";
            profileDefaults.scaleLowArcsecPerPix = 15.0f;
            profileDefaults.scaleHighArcsecPerPix = 90.0f;
            profileDefaults.downsample = 1;
            profileDefaults.timeoutSeconds = 240;
            profileDefaults.preprocessEnabled = true;
            profileDefaults.preprocessMode = "global";
            profileDefaults.preprocessTargetWidth = 1280;
            profileDefaults.preprocessMaxDimension = 2600;
            profileDefaults.tileWidth = 1800;
            profileDefaults.tileHeight = 1800;
            profileDefaults.tileOverlap = 300;
            profileDefaults.maxStarCount = 60;
            profileDefaults.minStarSeparation = 0.05f;
            return true;
        }

        errorOut = "Unknown blind solve profile: " + requestedProfile + " (expected default, wide-field, or narrow-field).";
        return false;
    }

    template <typename T>
    bool ApplyBlindSolveProfile(T &values,
                                const std::set<std::string> &explicitProps,
                                std::string &errorOut) {
        T profileDefaults;
        if (!BuildBlindSolveProfileDefaults(values.profile, profileDefaults, errorOut)) {
            return false;
        }

        values.profile = profileDefaults.profile;

        auto assignIfImplicit = [&](const char *propName, auto &target, const auto &profileValue) {
            if (explicitProps.count(propName) == 0) {
                target = profileValue;
            }
        };

        assignIfImplicit("scaleLowArcsecPerPix", values.scaleLowArcsecPerPix, profileDefaults.scaleLowArcsecPerPix);
        assignIfImplicit("scaleHighArcsecPerPix", values.scaleHighArcsecPerPix, profileDefaults.scaleHighArcsecPerPix);
        assignIfImplicit("downsample", values.downsample, profileDefaults.downsample);
        assignIfImplicit("timeoutSeconds", values.timeoutSeconds, profileDefaults.timeoutSeconds);
        assignIfImplicit("preprocessEnabled", values.preprocessEnabled, profileDefaults.preprocessEnabled);
        assignIfImplicit("preprocessMode", values.preprocessMode, profileDefaults.preprocessMode);
        assignIfImplicit("preprocessTargetWidth", values.preprocessTargetWidth, profileDefaults.preprocessTargetWidth);
        assignIfImplicit("preprocessMaxDimension", values.preprocessMaxDimension, profileDefaults.preprocessMaxDimension);
        assignIfImplicit("tileWidth", values.tileWidth, profileDefaults.tileWidth);
        assignIfImplicit("tileHeight", values.tileHeight, profileDefaults.tileHeight);
        assignIfImplicit("tileOverlap", values.tileOverlap, profileDefaults.tileOverlap);
        assignIfImplicit("maxStarCount", values.maxStarCount, profileDefaults.maxStarCount);
        assignIfImplicit("minStarSeparation", values.minStarSeparation, profileDefaults.minStarSeparation);
        return true;
    }

    Lost* Lost::getInstance()
    {
        if(!_pDefaultLost)
        {
            // _pDefaultLogger = new TempLogger();
            _pDefaultLost = new lost::Lost();
        }
        return _pDefaultLost;
    }

        /// Create a database and write it to a file based on the command line options in \p values
    void Lost::DatabaseBuild(const DatabaseOptions &values) {

        BOOST_LOG_TRIVIAL(info) << "Generating HealPix Tles of the Space";

        HealPix healPix = lost::generateHealPixTiles(8);
        std::cout << "Total Tiles generated : " << healPix.size() << std::endl;

        Catalog narrowedCatalog = NarrowCatalog(lost::CatalogRead(), (int) (values.minMag * 100), values.maxStars, DegToRad(values.minSeparation));
        BOOST_LOG_TRIVIAL(info) << "Narrowed catalog has " << narrowedCatalog.size() << " stars.";

        healPix = lost::assignStarsToTiles(narrowedCatalog, healPix);

        Catalog result;

        // Testing Quad Generation Start---
        
        BOOST_LOG_TRIVIAL(info) << "Generating Quads from HealPix Tles of the Space";

        std::vector<CatalogQuad> quads, narrowQuads;
        quads = lost::HealPix2QuadParse(healPix);
        BOOST_LOG_TRIVIAL(info) << "Total Quads generated : " << quads.size();        
        
        narrowQuads = lost::removeDuplicateQuads(quads);
        BOOST_LOG_TRIVIAL(info) << "Total Narrowed Quads generated : " << narrowQuads.size();
        // Testing Quad Generation End---

        MultiDatabaseDescriptor dbEntriesQuads = GenerateQuadsDatabases(quads, values);
        SerializeContext serQuads = serFromDbValues(values);

        // Create & Set Flags.
        uint32_t dbFlags = 0;
        dbFlags |= typeid(float) == typeid(float) ? MULTI_DB_FLOAT_FLAG : 0;

        BOOST_LOG_TRIVIAL(info) << "Total Quads generated : " << quads.size();  
        // Serialize Flags
        SerializeMultiDatabase(&serQuads, dbEntriesQuads, dbFlags);

        BOOST_LOG_TRIVIAL(info) << "Generated database with " << serQuads.buffer.size() << " bytes";
        BOOST_LOG_TRIVIAL(info) << "Database flagged with " << std::bitset<8*sizeof(dbFlags)>(dbFlags);

        UserSpecifiedOutputStream pos = UserSpecifiedOutputStream(values.outputPath, true, true);
        pos.Stream().write((char *) serQuads.buffer.data(), serQuads.buffer.size());

        BOOST_LOG_TRIVIAL(info) << "Quads Database Serialization Complete";

    }

    /// Run a star-tracking pipeline (possibly including generating inputs and analyzing outputs) based on command line options in \p values.
    void Lost::PipelineRun(const PipelineOptions &values) {
        PipelineInputList input = GetPipelineInput(values);
        Pipeline pipeline = SetPipeline(values);
        std::vector<PipelineOutput> outputs = pipeline.Go(input);
        PipelineComparison(input, outputs, values);
    }

    /// Run a continous star-tracking pipeline (possibly including generating inputs and analyzing outputs) based on command line options in \p values.
    void Lost::StreamRun(const StreamOptions &values) {
        StreamInputList input = GetStreamInput(values);
        Stream stream = SetStream(values);
        stream.Go(input);
    }

    void Lost::BlindSolveRun(const BlindSolveOptions &values) {
        BlindSolveResult result = BlindSolve(values);
        if (!result.success) {
            BOOST_LOG_TRIVIAL(error) << "Blind solve failed.";
            if (!result.stderrText.empty()) {
                BOOST_LOG_TRIVIAL(error) << result.stderrText;
            }
            exit(1);
        }

        std::cout << "blind_solve_success 1" << std::endl;
        std::cout << "blind_solve_ra_deg " << result.raDeg << std::endl;
        std::cout << "blind_solve_dec_deg " << result.decDeg << std::endl;
        std::cout << "blind_solve_ra_hms " << result.raHMS << std::endl;
        std::cout << "blind_solve_dec_dms " << result.decDMS << std::endl;
        std::cout << "blind_solve_center_ra_hms " << result.raHMS << std::endl;
        std::cout << "blind_solve_center_dec_dms " << result.decDMS << std::endl;
        std::cout << "blind_solve_rotation_deg " << result.rotationDeg << std::endl;
        std::cout << "blind_solve_plate_scale_arcsec_per_pix " << result.plateScaleArcsecPerPix << std::endl;
        std::cout << "blind_solve_pixel_scale_arcsec_per_pix " << result.plateScaleArcsecPerPix << std::endl;
        std::cout << "blind_solve_size_w_arcmin " << (result.fieldWidthDeg * 60.0) << std::endl;
        std::cout << "blind_solve_size_h_arcmin " << (result.fieldHeightDeg * 60.0) << std::endl;
        std::cout << "blind_solve_radius_deg " << result.fieldRadiusDeg << std::endl;
        std::cout << "blind_solve_wcs_file " << result.wcsFilePath << std::endl;
        
        // Process ECEF transformation if enabled (default: enabled)
        if (values.ecefEnabled) {
            std::string ecefOutput = ProcessBlindSolveToECEF(result, values);
            if (!ecefOutput.empty()) {
                std::cout << ecefOutput;
            }
        }
    }

        // This is separate from `main` just because it's in the `lost` namespace
    void Lost::Run(int argc, char **argv) {

        if (argc == 1) {
            BOOST_LOG_TRIVIAL(info) << "Usage: ./lost database or ./lost pipeline";
            BOOST_LOG_TRIVIAL(info) << "Use --help flag on those commands for further help";

            //return 0;
        }

        std::string command(argv[1]);
        optind = 2;

        if (command == "database") {

            enum class DatabaseCliOption {
                #define LOST_CLI_OPTION(name, type, prop, defaultVal, converter, defaultArg) prop,
                #define LOST_OPTIONS_DATABASE
                #include "options.hpp"
                #undef LOST_OPTIONS_DATABASE
                #undef LOST_CLI_OPTION
                    help
            };

            static struct option long_options[] = {
                #define LOST_CLI_OPTION(name, type, prop, defaultVal, converter, defaultArg) \
                {name,                                                      \
                     defaultArg == 0 ? required_argument : optional_argument, \
                     0,                                                         \
                     (int)DatabaseCliOption::prop},
                 #define LOST_OPTIONS_DATABASE
                 #include "options.hpp" // NOLINT
                 #undef LOST_OPTIONS_DATABASE
                #undef LOST_CLI_OPTION
                    {"help", no_argument, 0, (int) DatabaseCliOption::help},
                    {0}
            };

            DatabaseOptions databaseOptions;
            int index;
            int option;

            while ((option = getopt_long(argc, argv, "", long_options, &index)) != -1) {
                switch (option) {
            #define LOST_CLI_OPTION(name, type, prop, defaultVal, converter, defaultArg) \
                    case (int)DatabaseCliOption::prop :                     \
                        if (defaultArg == 0) {     \
                            databaseOptions.prop = converter;       \
                        } else {                                    \
                            if (LOST_OPTIONAL_OPTARG()) {           \
                                databaseOptions.prop = converter;   \
                            } else {                                \
                                databaseOptions.prop = defaultArg;  \
                            }                                       \
                        }                                           \
                break;
            #define LOST_OPTIONS_DATABASE
            #include "options.hpp" // NOLINT
            #undef LOST_OPTIONS_DATABASE
            #undef LOST_CLI_OPTION
                    case (int) DatabaseCliOption::help :std::cout << documentation_database_txt << std::endl;
                        //return 0;
                        break;
                    default :std::cout << "Illegal flag" << std::endl;
                        exit(1);
                }
            }

            lost::Lost::DatabaseBuild(databaseOptions);

        }
        else if (command == "pipeline") {

            enum class PipelineCliOption {
                #define LOST_CLI_OPTION(name, type, prop, defaultVal, converter, defaultArg) prop,
                #define LOST_OPTIONS_PIPELINE
                #include "options.hpp"
                #undef LOST_OPTIONS_PIPELINE
                #undef LOST_CLI_OPTION
                    help
            };

            static struct option long_options[] = {
                #define LOST_CLI_OPTION(name, type, prop, defaultVal, converter, defaultArg) \
                {name,                                                      \
                 defaultArg == 0 ? required_argument : optional_argument, \
                 0,                                                         \
                 (int)PipelineCliOption::prop},
                #define LOST_OPTIONS_PIPELINE
                #include "options.hpp" // NOLINT
                #undef LOST_OPTIONS_PIPELINE
                #undef LOST_CLI_OPTION

                    // DATABASES
                    {"help", no_argument, 0, (int) PipelineCliOption::help},
                    {0, 0, 0, 0}
            };

            lost::PipelineOptions pipelineOptions;
            int index;
            int option;

            while ((option = getopt_long(argc, argv, "", long_options, &index)) != -1) {
                switch (option) {
                    #define LOST_CLI_OPTION(name, type, prop, defaultVal, converter, defaultArg) \
                    case (int)PipelineCliOption::prop :                         \
                        if (defaultArg == 0) {    \
                            pipelineOptions.prop = converter;       \
                        } else {                                    \
                            if (LOST_OPTIONAL_OPTARG()) {           \
                                pipelineOptions.prop = converter;   \
                            } else {                                \
                                pipelineOptions.prop = defaultArg;  \
                            }                                       \
                        }                                           \
                break;
                    #define LOST_OPTIONS_PIPELINE
                    #include "options.hpp" // NOLINT
                    #undef LOST_OPTIONS_PIPELINE
                    #undef LOST_CLI_OPTION
                    case (int) PipelineCliOption::help :std::cout << documentation_pipeline_txt << std::endl;
                        //return 0;
                        break;
                    default :std::cout << "Illegal flag" << std::endl;
                        exit(1);
                }
            }

            lost::Lost::PipelineRun(pipelineOptions);

        } else if (command == "stream") {

            enum class StreamCliOption {
                #define LOST_CLI_OPTION(name, type, prop, defaultVal, converter, defaultArg) prop,
                #define LOST_OPTIONS_STREAM
                #include "options.hpp"
                #undef LOST_OPTIONS_STREAM
                #undef LOST_CLI_OPTION
                help
            };

            static struct option long_options[] = {
                #define LOST_CLI_OPTION(name, type, prop, defaultVal, converter, defaultArg) \
                {name,                                                      \
                 defaultArg == 0 ? required_argument : optional_argument, \
                 0,                                                         \
                 (int)StreamCliOption::prop},
                #define LOST_OPTIONS_STREAM
                #include "options.hpp" // NOLINT
                #undef LOST_OPTIONS_STREAM
                #undef LOST_CLI_OPTION

                    // DATABASES
                    {"help", no_argument, 0, (int) StreamCliOption::help},
                    {0, 0, 0, 0}
            };

            lost::StreamOptions streamOptions;
            int index;
            int option;

            while ((option = getopt_long(argc, argv, "", long_options, &index)) != -1) {
                switch (option) {
                    #define LOST_CLI_OPTION(name, type, prop, defaultVal, converter, defaultArg) \
                    case (int)StreamCliOption::prop :                         \
                        if (defaultArg == 0) {    \
                            streamOptions.prop = converter;       \
                        } else {                                    \
                            if (LOST_OPTIONAL_OPTARG()) {           \
                                streamOptions.prop = converter;   \
                            } else {                                \
                                streamOptions.prop = defaultArg;  \
                            }                                       \
                        }                                           \
                break;
                #define LOST_OPTIONS_STREAM
                #include "options.hpp" // NOLINT
                #undef LOST_OPTIONS_STREAM
                #undef LOST_CLI_OPTION
                    case (int) StreamCliOption::help :std::cout << documentation_stream_txt << std::endl;
                        //return 0;
                        break;
                    default :std::cout << "Illegal flag" << std::endl;
                        exit(1);
                }
            }

            lost::Lost::StreamRun(streamOptions);

        } else if (command == "blind-solve") {

            enum class BlindSolveCliOption {
                #define LOST_CLI_OPTION(name, type, prop, defaultVal, converter, defaultArg) prop,
                #define LOST_OPTIONS_BLIND_SOLVE
                #include "options.hpp"
                #undef LOST_OPTIONS_BLIND_SOLVE
                #undef LOST_CLI_OPTION
                help
            };

            static struct option long_options[] = {
                #define LOST_CLI_OPTION(name, type, prop, defaultVal, converter, defaultArg) \
                {name, defaultArg == 0 ? required_argument : optional_argument, 0, (int)BlindSolveCliOption::prop},
                #define LOST_OPTIONS_BLIND_SOLVE
                #include "options.hpp"
                #undef LOST_OPTIONS_BLIND_SOLVE
                #undef LOST_CLI_OPTION
                {"help", no_argument, 0, (int)BlindSolveCliOption::help},
                {0, 0, 0, 0}
            };

            BlindSolveOptions blindSolveOptions;
            std::set<std::string> explicitBlindSolveProps;
            int index;
            int option;

            while ((option = getopt_long(argc, argv, "", long_options, &index)) != -1) {
                switch (option) {
                    #define LOST_CLI_OPTION(name, type, prop, defaultVal, converter, defaultArg) \
                    case (int)BlindSolveCliOption::prop:                        \
                        explicitBlindSolveProps.insert(#prop);                  \
                        if (defaultArg == 0) {                                  \
                            blindSolveOptions.prop = converter;                 \
                        } else {                                                \
                            if (LOST_OPTIONAL_OPTARG()) {                       \
                                blindSolveOptions.prop = converter;             \
                            } else {                                            \
                                blindSolveOptions.prop = defaultArg;            \
                            }                                                   \
                        }                                                       \
                        break;
                    #define LOST_OPTIONS_BLIND_SOLVE
                    #include "options.hpp"
                    #undef LOST_OPTIONS_BLIND_SOLVE
                    #undef LOST_CLI_OPTION
                    case (int)BlindSolveCliOption::help:
                        std::cout
                            << "Usage: ./lost blind-solve --image <image-file> [options]\n"
                            << "Options:\n"
                            << "  --profile <name>      Preset defaults: default, wide-field, narrow-field.\n"
                            << "  --index-dir <dir>     Path to astrometry index files.\n"
                            << "  --output-dir <dir>    Output directory for solve artifacts.\n"
                            << "  --scale-low <v>       Lower plate scale in arcsec/pixel.\n"
                            << "  --scale-high <v>      Upper plate scale in arcsec/pixel.\n"
                            << "  --downsample <n>      Downsample factor for source extraction.\n"
                            << "  --timeout <seconds>   Solver CPU timeout.\n"
                            << "  --preprocess <on|off> Enable preprocessing (default: on).\n"
                            << "  --preprocess-mode <global|tiles> Preprocessing mode (default: global).\n"
                            << "  --target-width <px>   Global mode target width with proportional height (default: 720).\n"
                            << "  --max-dim <px>        Global mode max longest side (default: 2200).\n"
                            << "  --tile-width <px>     Tile width for tiles mode (default: 1800).\n"
                            << "  --tile-height <px>    Tile height for tiles mode (default: 1800).\n"
                            << "  --tile-overlap <px>   Tile overlap for tiles mode (default: 300).\n"
                            << "  --max-star-count <n>  Limit the extracted source list to the brightest N stars (default: 50).\n"
                            << "  --min-star-separation <deg> Minimum angular separation between selected sources.\n"
                            << "  --overwrite           Overwrite previous solve artifacts.\n"
                            << "\nECEF & Visualization Options (defaults enabled):\n"
                            << "  --ecef <on|off>       Enable ECEF transformation (default: on).\n"
                            << "  --3d-viz <on|off>     Enable 3D visualization (default: on).\n"
                            << "  --viz-output <file>   Save 3D visualization to PNG file.\n"
                            << "  --ecef-output <file>  Save ECEF attitude to JSON file.\n"
                            << "  --ecef-console <on|off> Print ECEF attitude to console (default: on).\n"
                            << "\n"
                            << "Outputs:\n"
                            << "  Writes <output-dir>/summary.tsv automatically after each batch run.\n";
                        return;
                    default:
                        std::cout << "Illegal flag" << std::endl;
                        exit(1);
                }
            }

            if (blindSolveOptions.imagePath.empty()) {
                BOOST_LOG_TRIVIAL(error) << "Missing required option: --image";
                exit(1);
            }

            if (blindSolveOptions.indexDirectory.empty()) {
                const char *envIndexDir = getenv("ASTROMETRY_INDEX_DIR");
                if (envIndexDir != nullptr) {
                    blindSolveOptions.indexDirectory = envIndexDir;
                }
            }

            if (blindSolveOptions.outputDirectory.empty()) {
                blindSolveOptions.outputDirectory = "logs/blind-solve";
            }

            std::string blindSolveProfileError;
            if (!ApplyBlindSolveProfile(blindSolveOptions, explicitBlindSolveProps, blindSolveProfileError)) {
                BOOST_LOG_TRIVIAL(error) << blindSolveProfileError;
                exit(1);
            }

            lost::Lost::BlindSolveRun(blindSolveOptions);

        } else if (command == "blind-solve-batch") {

            enum class BlindSolveBatchCliOption {
                #define LOST_CLI_OPTION(name, type, prop, defaultVal, converter, defaultArg) prop,
                #define LOST_OPTIONS_BLIND_SOLVE_BATCH
                #include "options.hpp"
                #undef LOST_OPTIONS_BLIND_SOLVE_BATCH
                #undef LOST_CLI_OPTION
                help
            };

            static struct option long_options[] = {
                #define LOST_CLI_OPTION(name, type, prop, defaultVal, converter, defaultArg) \
                {name, defaultArg == 0 ? required_argument : optional_argument, 0, (int)BlindSolveBatchCliOption::prop},
                #define LOST_OPTIONS_BLIND_SOLVE_BATCH
                #include "options.hpp"
                #undef LOST_OPTIONS_BLIND_SOLVE_BATCH
                #undef LOST_CLI_OPTION
                {"help", no_argument, 0, (int)BlindSolveBatchCliOption::help},
                {0, 0, 0, 0}
            };

            BlindSolveBatchCliValues batchCliValues;
            std::set<std::string> explicitBlindSolveBatchProps;
            int index;
            int option;

            while ((option = getopt_long(argc, argv, "", long_options, &index)) != -1) {
                switch (option) {
                    #define LOST_CLI_OPTION(name, type, prop, defaultVal, converter, defaultArg) \
                    case (int)BlindSolveBatchCliOption::prop:                   \
                        explicitBlindSolveBatchProps.insert(#prop);             \
                        if (defaultArg == 0) {                                  \
                            batchCliValues.prop = converter;                    \
                        } else {                                                \
                            if (LOST_OPTIONAL_OPTARG()) {                       \
                                batchCliValues.prop = converter;                \
                            } else {                                            \
                                batchCliValues.prop = defaultArg;               \
                            }                                                   \
                        }                                                       \
                        break;
                    #define LOST_OPTIONS_BLIND_SOLVE_BATCH
                    #include "options.hpp"
                    #undef LOST_OPTIONS_BLIND_SOLVE_BATCH
                    #undef LOST_CLI_OPTION
                    case (int)BlindSolveBatchCliOption::help:
                        std::cout
                            << "Usage: ./lost blind-solve-batch --input-dir <dir> [options]\n"
                            << "Options:\n"
                            << "  --profile <name>      Preset defaults: default, wide-field, narrow-field.\n"
                            << "  --index-dir <dir>     Path to astrometry index files.\n"
                            << "  --output-dir <dir>    Output directory for solve artifacts.\n"
                            << "  --scale-low <v>       Lower plate scale in arcsec/pixel.\n"
                            << "  --scale-high <v>      Upper plate scale in arcsec/pixel.\n"
                            << "  --downsample <n>      Downsample factor for source extraction.\n"
                            << "  --timeout <seconds>   Solver CPU timeout per image.\n"
                            << "  --jobs <n>            Number of parallel workers.\n"
                            << "  --preprocess <on|off> Enable preprocessing (default: on).\n"
                            << "  --preprocess-mode <global|tiles> Preprocessing mode (default: global).\n"
                            << "  --target-width <px>   Global mode target width with proportional height (default: 720).\n"
                            << "  --max-dim <px>        Global mode max longest side (default: 2200).\n"
                            << "  --tile-width <px>     Tile width for tiles mode (default: 1800).\n"
                            << "  --tile-height <px>    Tile height for tiles mode (default: 1800).\n"
                            << "  --tile-overlap <px>   Tile overlap for tiles mode (default: 300).\n"
                            << "  --max-star-count <n>  Limit the extracted source list to the brightest N stars (default: 50).\n"
                            << "  --min-star-separation <deg> Minimum angular separation between selected sources.\n"
                            << "  --overwrite           Overwrite previous solve artifacts.\n";
                        return;
                    default:
                        std::cout << "Illegal flag" << std::endl;
                        exit(1);
                }
            }

            std::string blindSolveBatchProfileError;
            if (!ApplyBlindSolveProfile(batchCliValues, explicitBlindSolveBatchProps, blindSolveBatchProfileError)) {
                BOOST_LOG_TRIVIAL(error) << blindSolveBatchProfileError;
                exit(1);
            }

            BlindSolveOptions batchDefaults;
            batchDefaults.profile = batchCliValues.profile;
            batchDefaults.indexDirectory = batchCliValues.indexDirectory;
            batchDefaults.outputDirectory = batchCliValues.outputDirectory;
            batchDefaults.scaleLowArcsecPerPix = batchCliValues.scaleLowArcsecPerPix;
            batchDefaults.scaleHighArcsecPerPix = batchCliValues.scaleHighArcsecPerPix;
            batchDefaults.downsample = batchCliValues.downsample;
            batchDefaults.timeoutSeconds = batchCliValues.timeoutSeconds;
            batchDefaults.preprocessEnabled = batchCliValues.preprocessEnabled;
            batchDefaults.preprocessMode = batchCliValues.preprocessMode;
            batchDefaults.preprocessTargetWidth = batchCliValues.preprocessTargetWidth;
            batchDefaults.preprocessMaxDimension = batchCliValues.preprocessMaxDimension;
            batchDefaults.tileWidth = batchCliValues.tileWidth;
            batchDefaults.tileHeight = batchCliValues.tileHeight;
            batchDefaults.tileOverlap = batchCliValues.tileOverlap;
            batchDefaults.overwrite = batchCliValues.overwrite;
            batchDefaults.maxStarCount = batchCliValues.maxStarCount;
            batchDefaults.minStarSeparation = batchCliValues.minStarSeparation;

            const std::string &inputDir = batchCliValues.inputDir;
            const int jobs = std::max(1, batchCliValues.jobs);

            if (inputDir.empty()) {
                BOOST_LOG_TRIVIAL(error) << "Missing required option: --input-dir";
                exit(1);
            }

            if (batchDefaults.indexDirectory.empty()) {
                const char *envIndexDir = getenv("ASTROMETRY_INDEX_DIR");
                if (envIndexDir != nullptr) {
                    batchDefaults.indexDirectory = envIndexDir;
                }
            }

            if (batchDefaults.indexDirectory.empty()) {
                BOOST_LOG_TRIVIAL(error) << "Missing astrometry index directory. Use --index-dir or ASTROMETRY_INDEX_DIR.";
                exit(1);
            }

            namespace fs = std::filesystem;
            fs::path inputPath(inputDir);
            if (!fs::exists(inputPath) || !fs::is_directory(inputPath)) {
                BOOST_LOG_TRIVIAL(error) << "Invalid input directory: " << inputDir;
                exit(1);
            }

            if (batchDefaults.outputDirectory.empty()) {
                batchDefaults.outputDirectory = "logs/blind-solve-batch";
            }
            fs::create_directories(batchDefaults.outputDirectory);

            std::vector<fs::path> images;
            for (const auto &entry : fs::directory_iterator(inputPath)) {
                if (!entry.is_regular_file()) {
                    continue;
                }
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".fits") {
                    images.push_back(entry.path());
                }
            }

            if (images.empty()) {
                BOOST_LOG_TRIVIAL(error) << "No images found in input directory: " << inputDir;
                exit(1);
            }

            std::sort(images.begin(), images.end());

            struct BatchSolveRow {
                std::string image;
                bool success = false;
                double timeSec = 0.0;
                double raDeg = 0.0;
                double decDeg = 0.0;
                std::string raHMS;
                std::string decDMS;
                double rollDeg = 0.0;
                double scaleArcsecPerPix = 0.0;
                double sizeWArcmin = 0.0;
                double sizeHArcmin = 0.0;
                double radiusDeg = 0.0;
                std::string reason;
            };

            std::vector<BatchSolveRow> summaryRows(images.size());

            std::atomic<size_t> nextIndex{0};
            std::atomic<int> solvedCount{0};
            std::atomic<int> failedCount{0};
            boost::mutex outputMutex;
            auto startAll = std::chrono::steady_clock::now();

            auto worker = [&]() {
                while (true) {
                    const size_t idx = nextIndex.fetch_add(1);
                    if (idx >= images.size()) {
                        break;
                    }

                    BlindSolveOptions opts = batchDefaults;
                    opts.imagePath = images[idx].string();
                    opts.outputDirectory = (fs::path(batchDefaults.outputDirectory) / images[idx].stem()).string();

                    auto startOne = std::chrono::steady_clock::now();
                    BlindSolveResult result = BlindSolve(opts);
                    const double elapsedSec = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - startOne).count() / 1000.0;

                    boost::lock_guard<boost::mutex> lock(outputMutex);
                    BatchSolveRow &row = summaryRows[idx];
                    row.image = images[idx].filename().string();
                    row.success = result.success;
                    row.timeSec = elapsedSec;

                    if (result.success) {
                        ++solvedCount;
                        row.raDeg = result.raDeg;
                        row.decDeg = result.decDeg;
                        row.raHMS = result.raHMS;
                        row.decDMS = result.decDMS;
                        row.rollDeg = result.rotationDeg;
                        row.scaleArcsecPerPix = result.plateScaleArcsecPerPix;
                        row.sizeWArcmin = result.fieldWidthDeg * 60.0;
                        row.sizeHArcmin = result.fieldHeightDeg * 60.0;
                        row.radiusDeg = result.fieldRadiusDeg;

                        std::cout
                            << "batch_done success image=" << images[idx].filename().string()
                            << " time_sec=" << elapsedSec
                            << " ra_deg=" << result.raDeg
                            << " dec_deg=" << result.decDeg
                            << " ra_hms=" << result.raHMS
                            << " dec_dms=" << result.decDMS
                            << " roll_deg=" << result.rotationDeg
                            << " size_w_arcmin=" << (result.fieldWidthDeg * 60.0)
                            << " size_h_arcmin=" << (result.fieldHeightDeg * 60.0)
                            << " radius_deg=" << result.fieldRadiusDeg
                            << " scale_arcsec_per_pix=" << result.plateScaleArcsecPerPix
                            << std::endl;
                    } else {
                        ++failedCount;
                        std::string reason = result.stderrText.empty() ? "unknown" : result.stderrText;
                        std::replace(reason.begin(), reason.end(), '\n', ' ');
                        std::replace(reason.begin(), reason.end(), '\t', ' ');
                        row.reason = reason;

                        std::cout
                            << "batch_done failure image=" << images[idx].filename().string()
                            << " time_sec=" << elapsedSec
                            << " reason=" << reason
                            << std::endl;
                    }
                }
            };

            const int threadCount = std::min<int>(jobs, static_cast<int>(images.size()));
            boost::thread_group group;
            for (int i = 0; i < threadCount; ++i) {
                group.create_thread(worker);
            }
            group.join_all();

            const double totalSec = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startAll).count() / 1000.0;

            std::cout
                << "batch_summary total=" << images.size()
                << " solved=" << solvedCount.load()
                << " failed=" << failedCount.load()
                << " workers=" << threadCount
                << " total_time_sec=" << totalSec
                << std::endl;

            const fs::path summaryPath = fs::path(batchDefaults.outputDirectory) / "summary.tsv";
            std::ofstream summary(summaryPath);
            if (!summary.good()) {
                BOOST_LOG_TRIVIAL(error) << "Failed to write batch summary table: " << summaryPath.string();
            } else {
                auto formatMachine = [](double value) {
                    std::ostringstream ss;
                    ss << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
                    return ss.str();
                };

                summary << "image\tresult\ttime_sec\tra_deg\tdec_deg\tra_hms\tdec_dms\troll_deg\tsize_w_arcmin\tsize_h_arcmin\tradius_deg\tpixel_scale_arcsec_per_pix\treason\n";

                for (const BatchSolveRow &row : summaryRows) {
                    summary << row.image << "\t";
                    summary << (row.success ? "solved" : "failure") << "\t";
                    summary << formatMachine(row.timeSec) << "\t";

                    if (row.success) {
                        summary << formatMachine(row.raDeg) << "\t";
                        summary << formatMachine(row.decDeg) << "\t";
                        summary << row.raHMS << "\t";
                        summary << row.decDMS << "\t";
                        summary << formatMachine(row.rollDeg) << "\t";
                        summary << formatMachine(row.sizeWArcmin) << "\t";
                        summary << formatMachine(row.sizeHArcmin) << "\t";
                        summary << formatMachine(row.radiusDeg) << "\t";
                        summary << formatMachine(row.scaleArcsecPerPix) << "\t";
                        summary << "NA";
                    } else {
                        summary << "NA\tNA\tNA\tNA\tNA\tNA\tNA\tNA\tNA\t";
                        summary << row.reason;
                    }

                    summary << "\n";
                }

                summary.close();
                std::cout << "batch_summary_file " << summaryPath.string() << std::endl;

                const fs::path prettySummaryPath = fs::path(batchDefaults.outputDirectory) / "summary_pretty.tsv";
                std::ofstream pretty(prettySummaryPath);
                if (!pretty.good()) {
                    BOOST_LOG_TRIVIAL(error) << "Failed to write pretty batch summary table: " << prettySummaryPath.string();
                } else {
                    auto formatPretty = [](double value) {
                        std::ostringstream ss;
                        ss << std::fixed << std::setprecision(2) << value;
                        return ss.str();
                    };

                    pretty << "image\tresult\ttime_sec\tra_deg\tdec_deg\tra_hms\tdec_dms\troll_deg\tsize_w_arcmin\tsize_h_arcmin\tradius_deg\tpixel_scale_arcsec_per_pix\treason\n";

                    for (const BatchSolveRow &row : summaryRows) {
                        pretty << row.image << "\t";
                        pretty << (row.success ? "solved" : "failure") << "\t";
                        pretty << formatPretty(row.timeSec) << "\t";

                        if (row.success) {
                            pretty << formatPretty(row.raDeg) << "\t";
                            pretty << formatPretty(row.decDeg) << "\t";
                            pretty << row.raHMS << "\t";
                            pretty << row.decDMS << "\t";
                            pretty << formatPretty(row.rollDeg) << "\t";
                            pretty << formatPretty(row.sizeWArcmin) << "\t";
                            pretty << formatPretty(row.sizeHArcmin) << "\t";
                            pretty << formatPretty(row.radiusDeg) << "\t";
                            pretty << formatPretty(row.scaleArcsecPerPix) << "\t";
                            pretty << "NA";
                        } else {
                            pretty << "NA\tNA\tNA\tNA\tNA\tNA\tNA\tNA\tNA\t";
                            pretty << row.reason;
                        }

                        pretty << "\n";
                    }

                    pretty.close();
                    std::cout << "batch_summary_pretty_file " << prettySummaryPath.string() << std::endl;
                }
            }

        } else if (command == "camera-yml") {

            enum class CameraYmlCliOption {
                #define LOST_CLI_OPTION(name, type, prop, defaultVal, converter, defaultArg) prop,
                #define LOST_OPTIONS_CAMERA_YML
                #include "options.hpp"
                #undef LOST_OPTIONS_CAMERA_YML
                #undef LOST_CLI_OPTION
                help
            };

            static struct option long_options[] = {
                #define LOST_CLI_OPTION(name, type, prop, defaultVal, converter, defaultArg) \
                {name, defaultArg == 0 ? required_argument : optional_argument, 0, (int)CameraYmlCliOption::prop},
                #define LOST_OPTIONS_CAMERA_YML
                #include "options.hpp"
                #undef LOST_OPTIONS_CAMERA_YML
                #undef LOST_CLI_OPTION
                {"help", no_argument, 0, (int)CameraYmlCliOption::help},
                {0, 0, 0, 0}
            };

            CameraYmlCliValues cameraYmlValues;

            int index;
            int option;
            while ((option = getopt_long(argc, argv, "", long_options, &index)) != -1) {
                switch (option) {
                    #define LOST_CLI_OPTION(name, type, prop, defaultVal, converter, defaultArg) \
                    case (int)CameraYmlCliOption::prop:                         \
                        if (defaultArg == 0) {                                  \
                            cameraYmlValues.prop = converter;                   \
                        } else {                                                \
                            if (LOST_OPTIONAL_OPTARG()) {                       \
                                cameraYmlValues.prop = converter;               \
                            } else {                                            \
                                cameraYmlValues.prop = defaultArg;              \
                            }                                                   \
                        }                                                       \
                        break;
                    #define LOST_OPTIONS_CAMERA_YML
                    #include "options.hpp"
                    #undef LOST_OPTIONS_CAMERA_YML
                    #undef LOST_CLI_OPTION
                    case (int)CameraYmlCliOption::help:
                        std::cout
                            << "Usage: ./lost camera-yml [options]\n"
                            << "Options:\n"
                            << "  --mode <single|folder|live>   camera.yml mode (default: single).\n"
                            << "  --camera-config <file>        Path to camera.yml (default: conf/camera.yml).\n"
                            << "  --results-dir <dir>           Output root for mode run (default: logs/blind-solve-cameras).\n"
                            << "  --timings-tsv <file>          Optional timings table path.\n"
                            << "  --python <exe>                Python executable (default: python3).\n"
                            << "  --run-fusion                  Run fusion script after mode execution.\n"
                            << "  --fusion-output <file>        Fusion output file (default: logs/fusion_result.txt).\n"
                            << "\nExamples:\n"
                            << "  ./bin/lost camera-yml --mode single --camera-config conf/camera.yml\n"
                            << "  ./bin/lost camera-yml --mode folder --camera-config conf/camera.yml\n"
                            << "  ./bin/lost camera-yml --mode live --camera-config conf/camera.yml\n"
                            << "  ./bin/lost camera-yml --mode folder --camera-config conf/camera.yml --run-fusion\n";
                        return;
                    default:
                        std::cout << "Illegal flag" << std::endl;
                        exit(1);
                }
            }

            const std::string &mode = cameraYmlValues.mode;
            const std::string &cameraConfig = cameraYmlValues.cameraConfig;
            const std::string &resultsDir = cameraYmlValues.resultsDir;
            const std::string &timingsTsv = cameraYmlValues.timingsTsv;
            const std::string &pythonExe = cameraYmlValues.pythonExe;
            const bool runFusion = cameraYmlValues.runFusion;
            const std::string &fusionOutput = cameraYmlValues.fusionOutput;

            if (mode != "single" && mode != "folder" && mode != "live") {
                BOOST_LOG_TRIVIAL(error) << "Invalid --mode. Expected single, folder, or live.";
                exit(1);
            }

            std::string binaryPath = argv[0];
            std::string runCmd =
                ShellEscape(pythonExe) + " " +
                ShellEscape("tools/blind_solve_from_camera_yml.py") + " " +
                "--mode " + ShellEscape(mode) + " " +
                "--camera-config " + ShellEscape(cameraConfig) + " " +
                "--binary " + ShellEscape(binaryPath) + " " +
                "--results-dir " + ShellEscape(resultsDir);

            if (!timingsTsv.empty()) {
                runCmd += " --timings-tsv " + ShellEscape(timingsTsv);
            }

            BOOST_LOG_TRIVIAL(info) << "Running camera.yml mode: " << mode;
            int runStatus = std::system(runCmd.c_str());
            if (runStatus != 0) {
                BOOST_LOG_TRIVIAL(error) << "camera-yml execution failed with status " << runStatus;
                exit(1);
            }

            if (runFusion) {
                std::string fusionCmd =
                    ShellEscape(pythonExe) + " " +
                    ShellEscape("tools/fuse_multi_star_tracker.py") + " " +
                    "--camera-config " + ShellEscape(cameraConfig) + " " +
                    "--output " + ShellEscape(fusionOutput);

                BOOST_LOG_TRIVIAL(info) << "Running fusion from camera.yml";
                int fusionStatus = std::system(fusionCmd.c_str());
                if (fusionStatus != 0) {
                    BOOST_LOG_TRIVIAL(error) << "Fusion execution failed with status " << fusionStatus;
                    exit(1);
                }
            }

        } else {
            BOOST_LOG_TRIVIAL(info) << "Usage: ./lost database, ./lost pipeline, ./lost stream, ./lost blind-solve, ./lost blind-solve-batch or ./lost camera-yml";
            BOOST_LOG_TRIVIAL(info) << "Use --help flag on those commands for further help";
        }
        //return 0;
    }

}

bool atobool(const char *cstr) {
    std::string str(cstr);
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (str == "1" || str == "true" || str == "on" || str == "yes") {
        return true;
    }
    if (str == "0" || str == "false" || str == "off" || str == "no") {
        return false;
    }
    assert(false);
}