// Central X-macro catalog for CLI options.
//
// Include this file only after defining exactly one selector:
//   LOST_OPTIONS_DATABASE
//   LOST_OPTIONS_PIPELINE
//   LOST_OPTIONS_STREAM
//   LOST_OPTIONS_BLIND_SOLVE
//   LOST_OPTIONS_BLIND_SOLVE_BATCH
//   LOST_OPTIONS_CAMERA_YML

#if defined(LOST_OPTIONS_DATABASE)

// DATABASE BUILD OPTIONS
// These control catalog narrowing, optional k-vector generation, byte order,
// and the output destination for serialized databases.
LOST_CLI_OPTION("min-mag"                , float      , minMag                , 100   , std::stof(optarg) , kNoDefaultArgument)
LOST_CLI_OPTION("max-stars"              , int        , maxStars              , 10000 , atoi(optarg)      , kNoDefaultArgument)
LOST_CLI_OPTION("min-separation"         , float      , minSeparation         , 0.08  , std::stof(optarg) , kNoDefaultArgument)
LOST_CLI_OPTION("kvector"                , bool       , kvector               , false , atobool(optarg)   , true)
LOST_CLI_OPTION("kvector-min-distance"   , float      , kvectorMinDistance    , 0.5   , std::stof(optarg) , kNoDefaultArgument)
LOST_CLI_OPTION("kvector-max-distance"   , float      , kvectorMaxDistance    , 15    , std::stof(optarg) , kNoDefaultArgument)
LOST_CLI_OPTION("kvector-distance-bins"  , long       , kvectorNumDistanceBins, 10000 , atol(optarg)      , kNoDefaultArgument)
LOST_CLI_OPTION("swap-integer-endianness", bool       , swapIntegerEndianness , false , atobool(optarg)   , true)
LOST_CLI_OPTION("swap-decimal-endianness", bool       , swapDecimalEndianness , false , atobool(optarg)   , true)
LOST_CLI_OPTION("output"                 , std::string, outputPath            , "-"   , optarg            , kNoDefaultArgument)

#elif defined(LOST_OPTIONS_PIPELINE) || defined(LOST_OPTIONS_STREAM)

// PIPELINE AND STREAM TRACKING OPTIONS
// Shared tracking and comparison knobs used by both `pipeline` and `stream`.

// MULTI-CAMERA CONFIG INPUT
LOST_CLI_OPTION("config-file"               , std::string, configFile                  , ""   , optarg             , kNoDefaultArgument)

// PROCESSING STAGES
LOST_CLI_OPTION("centroid-algo"             , std::string, centroidAlgo                , ""   , optarg             , "cog")
LOST_CLI_OPTION("centroid-dummy-stars"      , int        , centroidDummyNumStars       , 5    , atoi(optarg)       , kNoDefaultArgument)
LOST_CLI_OPTION("centroid-mag-filter"       , float      , centroidMagFilter           , -1   , std::stof(optarg)  , 5)
LOST_CLI_OPTION("centroid-filter-brightest" , int        , centroidFilterBrightest     , -1   , atoi(optarg)       , 10)
LOST_CLI_OPTION("database"                  , std::string, databasePath                , ""   , optarg             , kNoDefaultArgument)
LOST_CLI_OPTION("star-id-algo"              , std::string, idAlgo                      , ""   , optarg             , "pyramid")
LOST_CLI_OPTION("angular-tolerance"         , float      , angularTolerance            , .04  , std::stof(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("false-stars-estimate"      , int        , estimatedNumFalseStars      , 500  , atoi(optarg)       , kNoDefaultArgument)
LOST_CLI_OPTION("max-mismatch-probability"  , float      , maxMismatchProb             , .001 , std::stof(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("attitude-algo"             , std::string, attitudeAlgo                , ""   , optarg             , "dqm")

// OUTPUT COMPARISON AND DEBUG EXPORTS
LOST_CLI_OPTION("centroid-compare-threshold" , float      , centroidCompareThreshold    , 2    , std::stof(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("attitude-compare-threshold" , float      , attitudeCompareThreshold    , 1    , std::stof(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("plot-raw-input"             , std::string, plotRawInput               , ""   , optarg             , "-")
LOST_CLI_OPTION("plot-input"                 , std::string, plotInput                  , ""   , optarg             , "-")
LOST_CLI_OPTION("plot-expected"              , std::string, plotExpected               , ""   , optarg             , "-")
LOST_CLI_OPTION("plot-centroid-indices"      , std::string, plotCentroidIndices        , ""   , optarg             , "-")
LOST_CLI_OPTION("plot-output"                , std::string, plotOutput                 , ""   , optarg             , "-")
LOST_CLI_OPTION("print-expected-centroids"   , std::string, printExpectedCentroids     , ""   , optarg             , "-")
LOST_CLI_OPTION("print-input-centroids"      , std::string, printInputCentroids        , ""   , optarg             , "-")
LOST_CLI_OPTION("print-actual-centroids"     , std::string, printActualCentroids       , ""   , optarg             , "-")
LOST_CLI_OPTION("print-attitude"             , std::string, printAttitude              , ""   , optarg             , "-")
LOST_CLI_OPTION("print-expected-attitude"    , std::string, printExpectedAttitude      , ""   , optarg             , "-")
LOST_CLI_OPTION("print-speed"                , std::string, printSpeed                 , ""   , optarg             , "-")
LOST_CLI_OPTION("compare-centroids"          , std::string, compareCentroids           , ""   , optarg             , "-")
LOST_CLI_OPTION("compare-star-ids"           , std::string, compareStarIds             , ""   , optarg             , "-")
LOST_CLI_OPTION("compare-attitudes"          , std::string, compareAttitudes           , ""   , optarg             , "-")

#if defined(LOST_OPTIONS_PIPELINE)

// PIPELINE EXECUTION MODE
// Supported modes include camera-snap, generated, continous, continous-multi,
// and file.
LOST_CLI_OPTION("mode"         , std::string, mode        , "camera-snap" , optarg       , kNoDefaultArgument)

// CAMERA MODEL INPUTS
LOST_CLI_OPTION("focal-length" , float      , focalLength , 0             , atof(optarg) , kNoDefaultArgument)
LOST_CLI_OPTION("pixel-size"   , float      , pixelSize   , -1            , atof(optarg) , kNoDefaultArgument)
LOST_CLI_OPTION("fov"          , float      , fov         , 20            , atof(optarg) , kNoDefaultArgument)

// GENERATED-IMAGE MODE
LOST_CLI_OPTION("generate"                    , int        , generate                  , 0      , atoi(optarg)       , 1)
LOST_CLI_OPTION("generate-x-resolution"       , int        , generateXRes              , 1024   , atoi(optarg)       , kNoDefaultArgument)
LOST_CLI_OPTION("generate-y-resolution"       , int        , generateYRes              , 1024   , atoi(optarg)       , kNoDefaultArgument)
LOST_CLI_OPTION("generate-centroids-only"     , bool       , generateCentroidsOnly     , false  , atobool(optarg)    , true)
LOST_CLI_OPTION("generate-zero-mag-photons"   , float      , generateZeroMagPhotons    , 20000  , std::stof(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("generate-saturation-photons" , float      , generateSaturationPhotons , 150    , std::stof(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("generate-spread-stddev"      , float      , generateSpreadStdDev      , 1      , std::stof(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("generate-shot-noise"         , bool       , generateShotNoise         , true   , atobool(optarg)    , kNoDefaultArgument)
LOST_CLI_OPTION("generate-dark-current"       , float      , generateDarkCurrent       , 0.1    , std::stof(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("generate-read-noise-stddev"  , float      , generateReadNoiseStdDev   , .05    , std::stof(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("generate-ra"                 , float      , generateRa                , 88     , std::stof(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("generate-de"                 , float      , generateDe                , 7      , std::stof(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("generate-roll"               , float      , generateRoll              , 0      , std::stof(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("generate-random-attitudes"   , bool       , generateRandomAttitudes   , false  , atobool(optarg)    , true)
LOST_CLI_OPTION("generate-blur-ra"            , float      , generateBlurRa            , 0      , std::stof(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("generate-blur-de"            , float      , generateBlurDe            , 0      , std::stof(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("generate-blur-roll"          , float      , generateBlurRoll          , 0      , std::stof(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("generate-exposure"           , float      , generateExposure          , 0.2    , std::stof(optarg)  , 0.1)
LOST_CLI_OPTION("generate-readout-time"       , float      , generateReadoutTime       , 0      , std::stof(optarg)  , 0.01)
LOST_CLI_OPTION("generate-oversampling"       , int        , generateOversampling      , 4      , atoi(optarg)       , kNoDefaultArgument)
LOST_CLI_OPTION("generate-false-stars"        , int        , generateNumFalseStars     , 0      , atoi(optarg)       , 50)
LOST_CLI_OPTION("generate-false-min-mag"      , float      , generateFalseMinMag       , 8      , std::stof(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("generate-false-max-mag"      , float      , generateFalseMaxMag       , 1      , std::stof(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("generate-perturb-centroids"  , float      , generatePerturbationStddev, 0      , std::stof(optarg)  , 0.2)
LOST_CLI_OPTION("generate-cutoff-mag"         , float      , generateCutoffMag         , 6.0    , std::stof(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("generate-seed"               , int        , generateSeed              , 394859 , atoi(optarg)       , kNoDefaultArgument)
LOST_CLI_OPTION("generate-time-based-seed"    , bool       , timeSeed                  , false  , atobool(optarg)    , true)

// LIVE CAMERA INPUTS
LOST_CLI_OPTION("real"         , bool       , real       , false , atobool(optarg), kNoDefaultArgument)
LOST_CLI_OPTION("camera_id"    , std::string, camera_id  , ""    , optarg         , kNoDefaultArgument)

// FILE INPUT MODE
LOST_CLI_OPTION("file"         , std::string, file       , ""    , optarg         , kNoDefaultArgument)

#endif

#elif defined(LOST_OPTIONS_BLIND_SOLVE)

// SINGLE IMAGE BLIND-SOLVE OPTIONS
// Profile presets let wide-field and narrow-field images start from different
// solve defaults while still allowing explicit flags to override each knob.
LOST_CLI_OPTION("profile"             , std::string, profile                 , "default"         , optarg            , kNoDefaultArgument)
LOST_CLI_OPTION("image"               , std::string, imagePath               , "logs/blind-solve" , optarg            , kNoDefaultArgument)
LOST_CLI_OPTION("index-dir"           , std::string, indexDirectory          , ""                , optarg            , kNoDefaultArgument)
LOST_CLI_OPTION("output-dir"          , std::string, outputDirectory         , "logs/blind-solve", optarg            , kNoDefaultArgument)
LOST_CLI_OPTION("scale-low"           , float      , scaleLowArcsecPerPix     , 0.1f              , std::stof(optarg) , kNoDefaultArgument)
LOST_CLI_OPTION("scale-high"          , float      , scaleHighArcsecPerPix    , 120.0f            , std::stof(optarg) , kNoDefaultArgument)
LOST_CLI_OPTION("downsample"          , int        , downsample               , 2                 , std::stoi(optarg) , kNoDefaultArgument)
LOST_CLI_OPTION("timeout"             , int        , timeoutSeconds           , 120               , std::stoi(optarg) , kNoDefaultArgument)
LOST_CLI_OPTION("preprocess"          , bool       , preprocessEnabled        , true              , atobool(optarg)   , kNoDefaultArgument)
LOST_CLI_OPTION("preprocess-mode"     , std::string, preprocessMode           , "global"          , optarg            , kNoDefaultArgument)
LOST_CLI_OPTION("target-width"        , int        , preprocessTargetWidth    , 720               , std::stoi(optarg) , kNoDefaultArgument)
LOST_CLI_OPTION("max-dim"             , int        , preprocessMaxDimension   , 2200              , std::stoi(optarg) , kNoDefaultArgument)
LOST_CLI_OPTION("tile-width"          , int        , tileWidth                , 1800              , std::stoi(optarg) , kNoDefaultArgument)
LOST_CLI_OPTION("tile-height"         , int        , tileHeight               , 1800              , std::stoi(optarg) , kNoDefaultArgument)
LOST_CLI_OPTION("tile-overlap"        , int        , tileOverlap              , 300               , std::stoi(optarg) , kNoDefaultArgument)
LOST_CLI_OPTION("overwrite"           , bool       , overwrite                , false             , atobool(optarg)   , true)
LOST_CLI_OPTION("ecef"                , bool       , ecefEnabled              , true              , atobool(optarg)   , kNoDefaultArgument)
LOST_CLI_OPTION("3d-viz"              , bool       , visualization3DEnabled   , true              , atobool(optarg)   , kNoDefaultArgument)
LOST_CLI_OPTION("viz-output"          , std::string, visualizationOutputPath  , ""                , optarg            , kNoDefaultArgument)
LOST_CLI_OPTION("ecef-output"         , std::string, ecefOutputPath           , ""                , optarg            , kNoDefaultArgument)
LOST_CLI_OPTION("ecef-console"        , bool       , ecefConsoleOutput        , true              , atobool(optarg)   , kNoDefaultArgument)
LOST_CLI_OPTION("max-star-count"      , int        , maxStarCount             , 50                , std::stoi(optarg) , kNoDefaultArgument)
LOST_CLI_OPTION("min-star-separation" , float      , minStarSeparation        , 0.1f              , std::stof(optarg) , kNoDefaultArgument)

#elif defined(LOST_OPTIONS_BLIND_SOLVE_BATCH)

// BATCH BLIND-SOLVE OPTIONS
LOST_CLI_OPTION("profile"              , std::string, profile                 , "default"                                   , optarg             , kNoDefaultArgument)
LOST_CLI_OPTION("input-dir"            , std::string, inputDir                , ""                                          , optarg             , kNoDefaultArgument)
LOST_CLI_OPTION("index-dir"            , std::string, indexDirectory          , ""                                          , optarg             , kNoDefaultArgument)
LOST_CLI_OPTION("output-dir"           , std::string, outputDirectory         , "logs/blind-solve-batch"                    , optarg             , kNoDefaultArgument)
LOST_CLI_OPTION("scale-low"            , float      , scaleLowArcsecPerPix     , 0.1f                                        , std::stof(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("scale-high"           , float      , scaleHighArcsecPerPix    , 120.0f                                      , std::stof(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("downsample"           , int        , downsample               , 2                                           , std::stoi(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("timeout"              , int        , timeoutSeconds           , 120                                         , std::stoi(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("jobs"                 , int        , jobs                     , static_cast<int>(std::max(1u, std::thread::hardware_concurrency())), std::stoi(optarg), kNoDefaultArgument)
LOST_CLI_OPTION("preprocess"           , bool       , preprocessEnabled        , true                                        , atobool(optarg)    , kNoDefaultArgument)
LOST_CLI_OPTION("preprocess-mode"      , std::string, preprocessMode           , "global"                                    , optarg             , kNoDefaultArgument)
LOST_CLI_OPTION("target-width"         , int        , preprocessTargetWidth    , 720                                         , std::stoi(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("max-dim"              , int        , preprocessMaxDimension   , 2200                                        , std::stoi(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("tile-width"           , int        , tileWidth                , 1800                                        , std::stoi(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("tile-height"          , int        , tileHeight               , 1800                                        , std::stoi(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("tile-overlap"         , int        , tileOverlap              , 300                                         , std::stoi(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("overwrite"            , bool       , overwrite                , false                                       , atobool(optarg)    , true)
LOST_CLI_OPTION("max-star-count"       , int        , maxStarCount             , 50                                          , std::stoi(optarg)  , kNoDefaultArgument)
LOST_CLI_OPTION("min-star-separation"  , float      , minStarSeparation        , 0.1f                                        , std::stof(optarg)  , kNoDefaultArgument)

#elif defined(LOST_OPTIONS_CAMERA_YML)

// CAMERA YML DRIVER OPTIONS
LOST_CLI_OPTION("mode"          , std::string, mode         , "single"                   , optarg          , kNoDefaultArgument)
LOST_CLI_OPTION("camera-config" , std::string, cameraConfig , "conf/camera.yml"          , optarg          , kNoDefaultArgument)
LOST_CLI_OPTION("results-dir"   , std::string, resultsDir   , "logs/blind-solve-cameras" , optarg          , kNoDefaultArgument)
LOST_CLI_OPTION("timings-tsv"   , std::string, timingsTsv   , ""                         , optarg          , kNoDefaultArgument)
LOST_CLI_OPTION("python"        , std::string, pythonExe    , "python3"                  , optarg          , kNoDefaultArgument)
LOST_CLI_OPTION("run-fusion"    , bool       , runFusion    , false                      , atobool(optarg) , true)
LOST_CLI_OPTION("fusion-output" , std::string, fusionOutput , "logs/fusion_result.txt"   , optarg          , kNoDefaultArgument)

#else

#error "Define a LOST_OPTIONS_* selector before including options.hpp"

#endif
