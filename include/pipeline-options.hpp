// this file uses the "X" pattern.

// Arguments to LOST_CLI_OPTION:
// 1. String used as the command line option.
// 2. Type of the option value.
// 3. Property name
// 4. Default value
// 5. Code to convert optarg into the value.
// 6. The default value if the option is specified with no argument, or kNoDefaultArgument

// To properly align these fields, I recommend using an editor plugin. In Vim, try `vim-lion`; in
// Emacs, try `evil-lion`. With your cursor inside any of the blocks, type `glip,` to aLign the
// Inside of the current Paragraph to comma.

#include <string>



// OPERATION MODE
LOST_CLI_OPTION("mode"         , std::string  , mode        , "camera-snap" , optarg    , kNoDefaultArgument)

// Many modes available to operate this star tracker software
// 1. camera-snap - program will take a snap from available camera and will compute the attitude information
// 2. generated - program will generate a image and then and will compute the attitude information on the generated image
// 3. continous - program will compute the attitude information continously from the available single camera feed.
// 4. continous-multi - program will compute the attitude information continously from the available multiple camera feeds.
// 4. file - program will compute the attitude information from an image file. Only png files are supported

// CAMERA OPTIONS for camera-snap, generated, continous and file mode (when a single camera information is required)
LOST_CLI_OPTION("focal-length" , float      , focalLength , 0  , atof(optarg) , kNoDefaultArgument)
LOST_CLI_OPTION("pixel-size"   , float      , pixelSize   , -1 , atof(optarg) , kNoDefaultArgument)
LOST_CLI_OPTION("fov"          , float      , fov         , 20 , atof(optarg) , kNoDefaultArgument)

// PIPELINE STAGES
LOST_CLI_OPTION("centroid-algo"            , std::string, centroidAlgo                  , ""  , optarg                  , "cog")
LOST_CLI_OPTION("centroid-dummy-stars"     , int        , centroidDummyNumStars         , 5   , atoi(optarg)            , kNoDefaultArgument)
LOST_CLI_OPTION("centroid-mag-filter"      , float    , centroidMagFilter             , -1  , std::stof(optarg)  , 5)
LOST_CLI_OPTION("centroid-filter-brightest", int        , centroidFilterBrightest       , -1  , atoi(optarg)            , 10)
LOST_CLI_OPTION("database"                 , std::string, databasePath                  , ""  , optarg                  , kNoDefaultArgument)
LOST_CLI_OPTION("star-id-algo"             , std::string, idAlgo                        , ""  , optarg                  , "pyramid")
LOST_CLI_OPTION("angular-tolerance"        , float    , angularTolerance              , .04 , std::stof(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("false-stars-estimate"     , int        , estimatedNumFalseStars        , 500 , atoi(optarg)            , kNoDefaultArgument)
LOST_CLI_OPTION("max-mismatch-probability" , float    , maxMismatchProb               , .001, std::stof(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("attitude-algo"            , std::string, attitudeAlgo                  , ""  , optarg                  , "dqm")

// OUTPUT COMPARISON
LOST_CLI_OPTION("centroid-compare-threshold", float    , centroidCompareThreshold, 2 , std::stof(optarg) , kNoDefaultArgument)
LOST_CLI_OPTION("attitude-compare-threshold", float    , attitudeCompareThreshold, 1 , std::stof(optarg) , kNoDefaultArgument)
LOST_CLI_OPTION("plot-raw-input"            , std::string, plotRawInput            , "", optarg                 , "-")
LOST_CLI_OPTION("plot-input"                , std::string, plotInput               , "", optarg                 , "-")
LOST_CLI_OPTION("plot-expected"             , std::string, plotExpected            , "", optarg                 , "-")
LOST_CLI_OPTION("plot-centroid-indices"     , std::string, plotCentroidIndices     , "", optarg                 , "-")
LOST_CLI_OPTION("plot-output"               , std::string, plotOutput              , "", optarg                 , "-")
LOST_CLI_OPTION("print-expected-centroids"  , std::string, printExpectedCentroids  , "", optarg                 , "-")
LOST_CLI_OPTION("print-input-centroids"     , std::string, printInputCentroids     , "", optarg                 , "-")
LOST_CLI_OPTION("print-actual-centroids"    , std::string, printActualCentroids    , "", optarg                 , "-")
LOST_CLI_OPTION("print-attitude"            , std::string, printAttitude           , "", optarg                 , "-")
LOST_CLI_OPTION("print-expected-attitude"   , std::string, printExpectedAttitude   , "", optarg                 , "-")
LOST_CLI_OPTION("print-speed"               , std::string, printSpeed              , "", optarg                 , "-")
LOST_CLI_OPTION("compare-centroids"         , std::string, compareCentroids        , "", optarg                 , "-")
LOST_CLI_OPTION("compare-star-ids"          , std::string, compareStarIds          , "", optarg                 , "-")
LOST_CLI_OPTION("compare-attitudes"         , std::string, compareAttitudes        , "", optarg                 , "-")

// IMAGE GENERATION generated mode
LOST_CLI_OPTION("generate"                    , int     , generate                  , 0     , atoi(optarg)    , 1)
LOST_CLI_OPTION("generate-x-resolution"       , int     , generateXRes              , 1024  , atoi(optarg)    , kNoDefaultArgument)
LOST_CLI_OPTION("generate-y-resolution"       , int     , generateYRes              , 1024  , atoi(optarg)    , kNoDefaultArgument)
LOST_CLI_OPTION("generate-centroids-only"     , bool    , generateCentroidsOnly     , false , atobool(optarg) , true)
LOST_CLI_OPTION("generate-zero-mag-photons"   , float , generateZeroMagPhotons    , 20000 , std::stof(optarg)    , kNoDefaultArgument)
LOST_CLI_OPTION("generate-saturation-photons" , float , generateSaturationPhotons , 150   , std::stof(optarg)    , kNoDefaultArgument)
LOST_CLI_OPTION("generate-spread-stddev"      , float , generateSpreadStdDev      , 1     , std::stof(optarg)    , kNoDefaultArgument)
LOST_CLI_OPTION("generate-shot-noise"         , bool    , generateShotNoise         , true  , atobool(optarg) , kNoDefaultArgument)
LOST_CLI_OPTION("generate-dark-current"       , float , generateDarkCurrent       , 0.1   , std::stof(optarg)    , kNoDefaultArgument)
LOST_CLI_OPTION("generate-read-noise-stddev"  , float , generateReadNoiseStdDev   , .05   , std::stof(optarg)    , kNoDefaultArgument)
LOST_CLI_OPTION("generate-ra"                 , float , generateRa                , 88    , std::stof(optarg)    , kNoDefaultArgument)
LOST_CLI_OPTION("generate-de"                 , float , generateDe                , 7     , std::stof(optarg)    , kNoDefaultArgument)
LOST_CLI_OPTION("generate-roll"               , float , generateRoll              , 0     , std::stof(optarg)    , kNoDefaultArgument)
LOST_CLI_OPTION("generate-random-attitudes"   , bool    , generateRandomAttitudes   , false , atobool(optarg) , true)
LOST_CLI_OPTION("generate-blur-ra"            , float , generateBlurRa            , 0     , std::stof(optarg)    , kNoDefaultArgument)
LOST_CLI_OPTION("generate-blur-de"            , float , generateBlurDe            , 0     , std::stof(optarg)    , kNoDefaultArgument)
LOST_CLI_OPTION("generate-blur-roll"          , float , generateBlurRoll          , 0     , std::stof(optarg)    , kNoDefaultArgument)
LOST_CLI_OPTION("generate-exposure"           , float , generateExposure          , 0.2   , std::stof(optarg)    , 0.1)
LOST_CLI_OPTION("generate-readout-time"       , float , generateReadoutTime       , 0     , std::stof(optarg)    , 0.01)
LOST_CLI_OPTION("generate-oversampling"       , int     , generateOversampling      , 4     , atoi(optarg)    , kNoDefaultArgument)
LOST_CLI_OPTION("generate-false-stars"        , int     , generateNumFalseStars     , 0     , atoi(optarg)    , 50)
LOST_CLI_OPTION("generate-false-min-mag"      , float , generateFalseMinMag       , 8     , std::stof(optarg)    , kNoDefaultArgument)
LOST_CLI_OPTION("generate-false-max-mag"      , float , generateFalseMaxMag       , 1     , std::stof(optarg)    , kNoDefaultArgument)
LOST_CLI_OPTION("generate-perturb-centroids"  , float , generatePerturbationStddev, 0     , std::stof(optarg)    , 0.2)
LOST_CLI_OPTION("generate-cutoff-mag"         , float , generateCutoffMag         , 6.0   , std::stof(optarg)    , kNoDefaultArgument)
LOST_CLI_OPTION("generate-seed"               , int     , generateSeed              , 394859, atoi(optarg)    , kNoDefaultArgument)
LOST_CLI_OPTION("generate-time-based-seed"    , bool    , timeSeed                  , false , atobool(optarg) , true)

// CAMERA SNAP camera-snap
LOST_CLI_OPTION("real"         , bool         , real        , false , atobool(optarg), kNoDefaultArgument)
LOST_CLI_OPTION("camera_id"    , std::string  , camera_id   , ""     , optarg         , kNoDefaultArgument)

// CONTINOUS condinous camera feed
// only camera_id information is required which has been defined above.

// CONTINOUS MULTI CAMERAS continous-multi
LOST_CLI_OPTION("config-file"   , std::string  , configFile    , "", optarg  , kNoDefaultArgument)

// IMAGE FILE (ony png files are supported)
LOST_CLI_OPTION("file"          , std::string  , file          , "", optarg  , kNoDefaultArgument)