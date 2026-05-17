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
                #include "database-options.hpp"
                #undef LOST_CLI_OPTION
                    help
            };

            static struct option long_options[] = {
                #define LOST_CLI_OPTION(name, type, prop, defaultVal, converter, defaultArg) \
                {name,                                                      \
                     defaultArg == 0 ? required_argument : optional_argument, \
                     0,                                                         \
                     (int)DatabaseCliOption::prop},
                #include "database-options.hpp" // NOLINT
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
            #include "database-options.hpp" // NOLINT
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
                #include "pipeline-options.hpp"
                #undef LOST_CLI_OPTION
                    help
            };

            static struct option long_options[] = {
                #define LOST_CLI_OPTION(name, type, prop, defaultVal, converter, defaultArg) \
                {name,                                                      \
                 defaultArg == 0 ? required_argument : optional_argument, \
                 0,                                                         \
                 (int)PipelineCliOption::prop},
                #include "pipeline-options.hpp" // NOLINT
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
                    #include "pipeline-options.hpp" // NOLINT
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
                #include "stream-options.hpp"
                #undef LOST_CLI_OPTION
                help
            };

            static struct option long_options[] = {
                #define LOST_CLI_OPTION(name, type, prop, defaultVal, converter, defaultArg) \
                {name,                                                      \
                 defaultArg == 0 ? required_argument : optional_argument, \
                 0,                                                         \
                 (int)StreamCliOption::prop},
                #include "stream-options.hpp" // NOLINT
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
                #include "stream-options.hpp" // NOLINT
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
                image,
                indexDir,
                outputDir,
                scaleLow,
                scaleHigh,
                downsample,
                timeout,
                preprocess,
                preprocessMode,
                targetWidth,
                maxDim,
                tileWidth,
                tileHeight,
                tileOverlap,
                overwrite,
                ecefEnabled,
                visualization3D,
                visualizationOutput,
                ecefOutput,
                ecefConsoleOutput,
                help
            };

            static struct option long_options[] = {
                {"image", required_argument, 0, (int)BlindSolveCliOption::image},
                {"index-dir", required_argument, 0, (int)BlindSolveCliOption::indexDir},
                {"output-dir", required_argument, 0, (int)BlindSolveCliOption::outputDir},
                {"scale-low", required_argument, 0, (int)BlindSolveCliOption::scaleLow},
                {"scale-high", required_argument, 0, (int)BlindSolveCliOption::scaleHigh},
                {"downsample", required_argument, 0, (int)BlindSolveCliOption::downsample},
                {"timeout", required_argument, 0, (int)BlindSolveCliOption::timeout},
                {"preprocess", required_argument, 0, (int)BlindSolveCliOption::preprocess},
                {"preprocess-mode", required_argument, 0, (int)BlindSolveCliOption::preprocessMode},
                {"target-width", required_argument, 0, (int)BlindSolveCliOption::targetWidth},
                {"max-dim", required_argument, 0, (int)BlindSolveCliOption::maxDim},
                {"tile-width", required_argument, 0, (int)BlindSolveCliOption::tileWidth},
                {"tile-height", required_argument, 0, (int)BlindSolveCliOption::tileHeight},
                {"tile-overlap", required_argument, 0, (int)BlindSolveCliOption::tileOverlap},
                {"overwrite", no_argument, 0, (int)BlindSolveCliOption::overwrite},
                {"ecef", required_argument, 0, (int)BlindSolveCliOption::ecefEnabled},
                {"3d-viz", required_argument, 0, (int)BlindSolveCliOption::visualization3D},
                {"viz-output", required_argument, 0, (int)BlindSolveCliOption::visualizationOutput},
                {"ecef-output", required_argument, 0, (int)BlindSolveCliOption::ecefOutput},
                {"ecef-console", required_argument, 0, (int)BlindSolveCliOption::ecefConsoleOutput},
                {"help", no_argument, 0, (int)BlindSolveCliOption::help},
                {0, 0, 0, 0}
            };

            BlindSolveOptions blindSolveOptions;
            int index;
            int option;

            while ((option = getopt_long(argc, argv, "", long_options, &index)) != -1) {
                switch (option) {
                    case (int)BlindSolveCliOption::image:
                        blindSolveOptions.imagePath = optarg;
                        break;
                    case (int)BlindSolveCliOption::indexDir:
                        blindSolveOptions.indexDirectory = optarg;
                        break;
                    case (int)BlindSolveCliOption::outputDir:
                        blindSolveOptions.outputDirectory = optarg;
                        break;
                    case (int)BlindSolveCliOption::scaleLow:
                        blindSolveOptions.scaleLowArcsecPerPix = std::stof(optarg);
                        break;
                    case (int)BlindSolveCliOption::scaleHigh:
                        blindSolveOptions.scaleHighArcsecPerPix = std::stof(optarg);
                        break;
                    case (int)BlindSolveCliOption::downsample:
                        blindSolveOptions.downsample = std::stoi(optarg);
                        break;
                    case (int)BlindSolveCliOption::timeout:
                        blindSolveOptions.timeoutSeconds = std::stoi(optarg);
                        break;
                    case (int)BlindSolveCliOption::preprocess: {
                        std::string value(optarg);
                        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                            return static_cast<char>(std::tolower(c));
                        });
                        blindSolveOptions.preprocessEnabled = (value == "1" || value == "true" || value == "on" || value == "yes");
                        break;
                    }
                    case (int)BlindSolveCliOption::preprocessMode:
                        blindSolveOptions.preprocessMode = optarg;
                        break;
                    case (int)BlindSolveCliOption::targetWidth:
                        blindSolveOptions.preprocessTargetWidth = std::stoi(optarg);
                        break;
                    case (int)BlindSolveCliOption::maxDim:
                        blindSolveOptions.preprocessMaxDimension = std::stoi(optarg);
                        break;
                    case (int)BlindSolveCliOption::tileWidth:
                        blindSolveOptions.tileWidth = std::stoi(optarg);
                        break;
                    case (int)BlindSolveCliOption::tileHeight:
                        blindSolveOptions.tileHeight = std::stoi(optarg);
                        break;
                    case (int)BlindSolveCliOption::tileOverlap:
                        blindSolveOptions.tileOverlap = std::stoi(optarg);
                        break;
                    case (int)BlindSolveCliOption::overwrite:
                        blindSolveOptions.overwrite = true;
                        break;
                    case (int)BlindSolveCliOption::ecefEnabled: {
                        std::string value(optarg);
                        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                            return static_cast<char>(std::tolower(c));
                        });
                        blindSolveOptions.ecefEnabled = (value == "1" || value == "true" || value == "on" || value == "yes");
                        break;
                    }
                    case (int)BlindSolveCliOption::visualization3D: {
                        std::string value(optarg);
                        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                            return static_cast<char>(std::tolower(c));
                        });
                        blindSolveOptions.visualization3DEnabled = (value == "1" || value == "true" || value == "on" || value == "yes");
                        break;
                    }
                    case (int)BlindSolveCliOption::visualizationOutput:
                        blindSolveOptions.visualizationOutputPath = optarg;
                        break;
                    case (int)BlindSolveCliOption::ecefOutput:
                        blindSolveOptions.ecefOutputPath = optarg;
                        break;
                    case (int)BlindSolveCliOption::ecefConsoleOutput: {
                        std::string value(optarg);
                        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                            return static_cast<char>(std::tolower(c));
                        });
                        blindSolveOptions.ecefConsoleOutput = (value == "1" || value == "true" || value == "on" || value == "yes");
                        break;
                    }
                    case (int)BlindSolveCliOption::help:
                        std::cout
                            << "Usage: ./lost blind-solve --image <image-file> [options]\n"
                            << "Options:\n"
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

            lost::Lost::BlindSolveRun(blindSolveOptions);

        } else if (command == "blind-solve-batch") {

            enum class BlindSolveBatchCliOption {
                inputDir,
                indexDir,
                outputDir,
                scaleLow,
                scaleHigh,
                downsample,
                timeout,
                jobs,
                preprocess,
                preprocessMode,
                targetWidth,
                maxDim,
                tileWidth,
                tileHeight,
                tileOverlap,
                overwrite,
                help
            };

            static struct option long_options[] = {
                {"input-dir", required_argument, 0, (int)BlindSolveBatchCliOption::inputDir},
                {"index-dir", required_argument, 0, (int)BlindSolveBatchCliOption::indexDir},
                {"output-dir", required_argument, 0, (int)BlindSolveBatchCliOption::outputDir},
                {"scale-low", required_argument, 0, (int)BlindSolveBatchCliOption::scaleLow},
                {"scale-high", required_argument, 0, (int)BlindSolveBatchCliOption::scaleHigh},
                {"downsample", required_argument, 0, (int)BlindSolveBatchCliOption::downsample},
                {"timeout", required_argument, 0, (int)BlindSolveBatchCliOption::timeout},
                {"jobs", required_argument, 0, (int)BlindSolveBatchCliOption::jobs},
                {"preprocess", required_argument, 0, (int)BlindSolveBatchCliOption::preprocess},
                {"preprocess-mode", required_argument, 0, (int)BlindSolveBatchCliOption::preprocessMode},
                {"target-width", required_argument, 0, (int)BlindSolveBatchCliOption::targetWidth},
                {"max-dim", required_argument, 0, (int)BlindSolveBatchCliOption::maxDim},
                {"tile-width", required_argument, 0, (int)BlindSolveBatchCliOption::tileWidth},
                {"tile-height", required_argument, 0, (int)BlindSolveBatchCliOption::tileHeight},
                {"tile-overlap", required_argument, 0, (int)BlindSolveBatchCliOption::tileOverlap},
                {"overwrite", no_argument, 0, (int)BlindSolveBatchCliOption::overwrite},
                {"help", no_argument, 0, (int)BlindSolveBatchCliOption::help},
                {0, 0, 0, 0}
            };

            BlindSolveOptions batchDefaults;
            std::string inputDir;
            int jobs = std::max(1u, std::thread::hardware_concurrency());
            int index;
            int option;

            while ((option = getopt_long(argc, argv, "", long_options, &index)) != -1) {
                switch (option) {
                    case (int)BlindSolveBatchCliOption::inputDir:
                        inputDir = optarg;
                        break;
                    case (int)BlindSolveBatchCliOption::indexDir:
                        batchDefaults.indexDirectory = optarg;
                        break;
                    case (int)BlindSolveBatchCliOption::outputDir:
                        batchDefaults.outputDirectory = optarg;
                        break;
                    case (int)BlindSolveBatchCliOption::scaleLow:
                        batchDefaults.scaleLowArcsecPerPix = std::stof(optarg);
                        break;
                    case (int)BlindSolveBatchCliOption::scaleHigh:
                        batchDefaults.scaleHighArcsecPerPix = std::stof(optarg);
                        break;
                    case (int)BlindSolveBatchCliOption::downsample:
                        batchDefaults.downsample = std::stoi(optarg);
                        break;
                    case (int)BlindSolveBatchCliOption::timeout:
                        batchDefaults.timeoutSeconds = std::stoi(optarg);
                        break;
                    case (int)BlindSolveBatchCliOption::jobs:
                        jobs = std::max(1, std::stoi(optarg));
                        break;
                    case (int)BlindSolveBatchCliOption::preprocess: {
                        std::string value(optarg);
                        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                            return static_cast<char>(std::tolower(c));
                        });
                        batchDefaults.preprocessEnabled = (value == "1" || value == "true" || value == "on" || value == "yes");
                        break;
                    }
                    case (int)BlindSolveBatchCliOption::preprocessMode:
                        batchDefaults.preprocessMode = optarg;
                        break;
                    case (int)BlindSolveBatchCliOption::targetWidth:
                        batchDefaults.preprocessTargetWidth = std::stoi(optarg);
                        break;
                    case (int)BlindSolveBatchCliOption::maxDim:
                        batchDefaults.preprocessMaxDimension = std::stoi(optarg);
                        break;
                    case (int)BlindSolveBatchCliOption::tileWidth:
                        batchDefaults.tileWidth = std::stoi(optarg);
                        break;
                    case (int)BlindSolveBatchCliOption::tileHeight:
                        batchDefaults.tileHeight = std::stoi(optarg);
                        break;
                    case (int)BlindSolveBatchCliOption::tileOverlap:
                        batchDefaults.tileOverlap = std::stoi(optarg);
                        break;
                    case (int)BlindSolveBatchCliOption::overwrite:
                        batchDefaults.overwrite = true;
                        break;
                    case (int)BlindSolveBatchCliOption::help:
                        std::cout
                            << "Usage: ./lost blind-solve-batch --input-dir <dir> [options]\n"
                            << "Options:\n"
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
                            << "  --overwrite           Overwrite previous solve artifacts.\n";
                        return;
                    default:
                        std::cout << "Illegal flag" << std::endl;
                        exit(1);
                }
            }

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
                mode,
                cameraConfig,
                resultsDir,
                timingsTsv,
                python,
                runFusion,
                fusionOutput,
                help
            };

            static struct option long_options[] = {
                {"mode", required_argument, 0, (int)CameraYmlCliOption::mode},
                {"camera-config", required_argument, 0, (int)CameraYmlCliOption::cameraConfig},
                {"results-dir", required_argument, 0, (int)CameraYmlCliOption::resultsDir},
                {"timings-tsv", required_argument, 0, (int)CameraYmlCliOption::timingsTsv},
                {"python", required_argument, 0, (int)CameraYmlCliOption::python},
                {"run-fusion", no_argument, 0, (int)CameraYmlCliOption::runFusion},
                {"fusion-output", required_argument, 0, (int)CameraYmlCliOption::fusionOutput},
                {"help", no_argument, 0, (int)CameraYmlCliOption::help},
                {0, 0, 0, 0}
            };

            std::string mode = "single";
            std::string cameraConfig = "conf/camera.yml";
            std::string resultsDir = "logs/blind-solve-cameras";
            std::string timingsTsv = "";
            std::string pythonExe = "python3";
            bool runFusion = false;
            std::string fusionOutput = "logs/fusion_result.txt";

            int index;
            int option;
            while ((option = getopt_long(argc, argv, "", long_options, &index)) != -1) {
                switch (option) {
                    case (int)CameraYmlCliOption::mode:
                        mode = optarg;
                        break;
                    case (int)CameraYmlCliOption::cameraConfig:
                        cameraConfig = optarg;
                        break;
                    case (int)CameraYmlCliOption::resultsDir:
                        resultsDir = optarg;
                        break;
                    case (int)CameraYmlCliOption::timingsTsv:
                        timingsTsv = optarg;
                        break;
                    case (int)CameraYmlCliOption::python:
                        pythonExe = optarg;
                        break;
                    case (int)CameraYmlCliOption::runFusion:
                        runFusion = true;
                        break;
                    case (int)CameraYmlCliOption::fusionOutput:
                        fusionOutput = optarg;
                        break;
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
    if (str == "1" || str == "true") {
        return true;
    }
    if (str == "0" || str == "false") {
        return false;
    }
    assert(false);
}