#include "blind-solve.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
#include <sys/wait.h>

#include <boost/log/trivial.hpp>

namespace lost {

namespace {

struct ExtractedSource {
    double x = 0.0;
    double y = 0.0;
    double flux = 0.0;
    double background = 0.0;
};

std::string DegreesToHMS(double raDeg) {
    // Convert RA in degrees to hours:minutes:seconds format
    double raHours = raDeg / 15.0;
    int hours = static_cast<int>(raHours);
    double minutesFrac = (raHours - hours) * 60.0;
    int minutes = static_cast<int>(minutesFrac);
    double seconds = (minutesFrac - minutes) * 60.0;
    
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%02dh%02dm%06.2fs", hours, minutes, seconds);
    return std::string(buffer);
}

std::string DegreesToDMS(double decDeg) {
    // Convert Dec in degrees to degrees:arcminutes:arcseconds format
    char sign = (decDeg >= 0.0) ? '+' : '-';
    double decAbs = fabs(decDeg);
    int degrees = static_cast<int>(decAbs);
    double minutesFrac = (decAbs - degrees) * 60.0;
    int minutes = static_cast<int>(minutesFrac);
    double seconds = (minutesFrac - minutes) * 60.0;
    
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%c%02d°%02d'%05.2f\"", sign, degrees, minutes, seconds);
    return std::string(buffer);
}

std::string ShellEscape(const std::string &value) {
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

std::string ReadFileToString(const std::string &path) {
    std::ifstream stream(path);
    if (!stream.good()) {
        return "";
    }
    std::stringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

double ParseKeyFromWcsInfo(const std::string &text, const std::string &key, double fallback) {
    std::regex keyRegex("^" + key + "\\s+([+-]?[0-9]*\\.?[0-9]+(?:[eE][+-]?[0-9]+)?)$", std::regex::icase | std::regex::multiline);
    std::smatch match;
    if (std::regex_search(text, match, keyRegex)) {
        return std::stod(match[1].str());
    }
    return fallback;
}

double ParseSolveFieldValue(const std::string &text, const std::string &pattern, double fallback) {
    std::regex expr(pattern, std::regex::icase);
    std::smatch match;
    if (std::regex_search(text, match, expr)) {
        return std::stod(match[1].str());
    }
    return fallback;
}

std::string ParseStringKeyFromWcsInfo(const std::string &text, const std::string &key, const std::string &fallback) {
    std::regex keyRegex("^" + key + "\\s+(.+)$", std::regex::icase | std::regex::multiline);
    std::smatch match;
    if (std::regex_search(text, match, keyRegex)) {
        return match[1].str();
    }
    return fallback;
}

bool GetImageDimensionsSips(const std::filesystem::path &imagePath, int &width, int &height) {
    std::string command =
        "sips -g pixelWidth -g pixelHeight " + ShellEscape(imagePath.string()) +
        " 2>/dev/null";

    FILE *pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        return false;
    }

    std::string output;
    std::array<char, 256> buffer{};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    int status = pclose(pipe);
    if (status == -1) {
        return false;
    }

    std::smatch match;
    std::regex widthRegex("pixelWidth:\\s*([0-9]+)", std::regex::icase);
    std::regex heightRegex("pixelHeight:\\s*([0-9]+)", std::regex::icase);

    if (!std::regex_search(output, match, widthRegex)) {
        return false;
    }
    width = std::stoi(match[1].str());

    if (!std::regex_search(output, match, heightRegex)) {
        return false;
    }
    height = std::stoi(match[1].str());

    return width > 0 && height > 0;
}

bool BuildAstrometryConfig(const std::filesystem::path &indexDirectory,
                          const std::filesystem::path &configPath,
                          std::string &errorOut) {
    std::error_code ec;
    std::ofstream cfg(configPath);
    if (!cfg.good()) {
        errorOut = "Unable to create astrometry config file: " + configPath.string();
        return false;
    }

    cfg << "add_path " << indexDirectory.string() << "\n";

    std::vector<std::string> indexFilenames;
    const std::regex singleSeriesPattern("^index-[0-9]{4}\\.fits$", std::regex::icase);
    const std::regex multiPartSeriesPattern("^index-[0-9]{4}-[0-9]{2}\\.fits$", std::regex::icase);

    for (const auto &entry : std::filesystem::directory_iterator(indexDirectory, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file()) {
            continue;
        }

        const std::string filename = entry.path().filename().string();
        if (!std::regex_match(filename, singleSeriesPattern) && !std::regex_match(filename, multiPartSeriesPattern)) {
            continue;
        }

        indexFilenames.push_back(filename);
    }

    std::sort(indexFilenames.begin(), indexFilenames.end());
    for (const auto &name : indexFilenames) {
        cfg << "index " << name << "\n";
    }
    cfg.close();

    if (indexFilenames.empty()) {
        errorOut = "No astrometry index files found in: " + indexDirectory.string();
        return false;
    }

    return true;
}

bool BuildPreprocessedInputs(const std::filesystem::path &imagePath,
                            const std::filesystem::path &outputDirectory,
                            const BlindSolveOptions &values,
                            std::vector<std::filesystem::path> &inputs,
                            std::string &errorOut) {
    inputs.clear();

    if (!values.preprocessEnabled) {
        inputs.push_back(imagePath);
        return true;
    }

    std::string mode = values.preprocessMode;
    std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (mode != "global" && mode != "tiles") {
        errorOut = "Invalid preprocess mode: " + values.preprocessMode + " (expected 'global' or 'tiles').";
        return false;
    }

    int width = 0;
    int height = 0;
    if (!GetImageDimensionsSips(imagePath, width, height)) {
        errorOut = "Failed to read image dimensions via sips for: " + imagePath.string();
        return false;
    }

    if (mode == "global") {
        if (values.preprocessTargetWidth > 0 && width > values.preprocessTargetWidth) {
            std::filesystem::path resizedPath = outputDirectory / "preprocessed_global.png";
            std::string resizeCommand =
                "sips --resampleWidth " + std::to_string(values.preprocessTargetWidth) + " " +
                ShellEscape(imagePath.string()) + " --out " + ShellEscape(resizedPath.string()) +
                " >/dev/null 2>&1";

            int status = std::system(resizeCommand.c_str());
            if (status == -1 || !std::filesystem::exists(resizedPath)) {
                errorOut = "Failed to resize image to target width with sips: " + imagePath.string();
                return false;
            }

            inputs.push_back(resizedPath);
            return true;
        }

        const int longestSide = std::max(width, height);
        if (values.preprocessMaxDimension <= 0 || longestSide <= values.preprocessMaxDimension) {
            inputs.push_back(imagePath);
            return true;
        }

        std::filesystem::path resizedPath = outputDirectory / "preprocessed_global.png";
        std::string resizeCommand =
            "sips -Z " + std::to_string(values.preprocessMaxDimension) + " " +
            ShellEscape(imagePath.string()) + " --out " + ShellEscape(resizedPath.string()) +
            " >/dev/null 2>&1";

        int status = std::system(resizeCommand.c_str());
        if (status == -1 || !std::filesystem::exists(resizedPath)) {
            errorOut = "Failed to resize image with sips: " + imagePath.string();
            return false;
        }

        inputs.push_back(resizedPath);
        return true;
    }

    if (values.tileWidth <= 0 || values.tileHeight <= 0) {
        errorOut = "Tile dimensions must be positive.";
        return false;
    }

    std::filesystem::path tilesDir = outputDirectory / "preprocessed_tiles";
    std::error_code ec;
    std::filesystem::create_directories(tilesDir, ec);
    if (ec) {
        errorOut = "Unable to create tiles directory: " + tilesDir.string();
        return false;
    }

    const int stepX = std::max(1, values.tileWidth - values.tileOverlap);
    const int stepY = std::max(1, values.tileHeight - values.tileOverlap);

    for (int y = 0; y < height; y += stepY) {
        for (int x = 0; x < width; x += stepX) {
            const int tileW = std::min(values.tileWidth, width - x);
            const int tileH = std::min(values.tileHeight, height - y);
            if (tileW <= 0 || tileH <= 0) {
                continue;
            }

            const int tileCenterY = y + (tileH / 2);
            const int tileCenterX = x + (tileW / 2);
            const int offsetY = tileCenterY - (height / 2);
            const int offsetX = tileCenterX - (width / 2);

            std::filesystem::path tilePath =
                tilesDir / ("tile_y" + std::to_string(y) + "_x" + std::to_string(x) + ".png");

            std::string cropCommand =
                "sips -c " + std::to_string(tileH) + " " + std::to_string(tileW) +
                " --cropOffset " + std::to_string(offsetY) + " " + std::to_string(offsetX) + " " +
                ShellEscape(imagePath.string()) + " --out " + ShellEscape(tilePath.string()) +
                " >/dev/null 2>&1";

            int status = std::system(cropCommand.c_str());
            if (status != -1 && std::filesystem::exists(tilePath)) {
                inputs.push_back(tilePath);
            }
        }
    }

    if (inputs.empty()) {
        errorOut = "No tiles were produced for image: " + imagePath.string();
        return false;
    }

    return true;
}

double EstimateMinSeparationPixels(const BlindSolveOptions &values) {
    if (values.minStarSeparation <= 0.0f) {
        return 0.0;
    }

    const double scaleLow = std::max(0.001, static_cast<double>(values.scaleLowArcsecPerPix));
    const double scaleHigh = std::max(scaleLow, static_cast<double>(values.scaleHighArcsecPerPix));
    const double estimatedScaleArcsecPerPix = std::sqrt(scaleLow * scaleHigh);
    return (static_cast<double>(values.minStarSeparation) * 3600.0) / estimatedScaleArcsecPerPix;
}

bool ParseTablistSources(const std::string &text,
                         std::vector<ExtractedSource> &sources,
                         std::string &errorOut) {
    sources.clear();

    std::stringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        std::istringstream row(line);
        int index = 0;
        ExtractedSource source;
        if (!(row >> index >> source.x >> source.y >> source.flux >> source.background)) {
            continue;
        }
        sources.push_back(source);
    }

    if (sources.empty()) {
        errorOut = "No extracted sources were parsed from tablist output.";
        return false;
    }

    return true;
}

std::vector<ExtractedSource> FilterSources(const std::vector<ExtractedSource> &sources,
                                           const BlindSolveOptions &values) {
    std::vector<ExtractedSource> sortedSources = sources;
    std::sort(sortedSources.begin(), sortedSources.end(), [](const ExtractedSource &left, const ExtractedSource &right) {
        if (left.flux != right.flux) {
            return left.flux > right.flux;
        }
        return left.background < right.background;
    });

    const double minSeparationPixels = EstimateMinSeparationPixels(values);
    const double minSeparationSq = minSeparationPixels * minSeparationPixels;
    const int maxStars = values.maxStarCount;

    std::vector<ExtractedSource> filtered;
    filtered.reserve(sortedSources.size());
    for (const auto &candidate : sortedSources) {
        bool tooClose = false;
        if (minSeparationSq > 0.0) {
            for (const auto &accepted : filtered) {
                const double dx = candidate.x - accepted.x;
                const double dy = candidate.y - accepted.y;
                if ((dx * dx) + (dy * dy) < minSeparationSq) {
                    tooClose = true;
                    break;
                }
            }
        }
        if (tooClose) {
            continue;
        }

        filtered.push_back(candidate);
        if (maxStars > 0 && static_cast<int>(filtered.size()) >= maxStars) {
            break;
        }
    }

    return filtered;
}

bool BuildFilteredSourceCatalog(const std::filesystem::path &solveImagePath,
                                const std::filesystem::path &attemptOutputDirectory,
                                const BlindSolveOptions &values,
                                std::filesystem::path &catalogPath,
                                int &imageWidth,
                                int &imageHeight,
                                std::string &errorOut) {
    catalogPath.clear();
    imageWidth = 0;
    imageHeight = 0;

    if (!GetImageDimensionsSips(solveImagePath, imageWidth, imageHeight)) {
        errorOut = "Failed to read filtered-source image dimensions for: " + solveImagePath.string();
        return false;
    }

    const bool needsFiltering = values.maxStarCount > 0 || values.minStarSeparation > 0.0f;
    if (!needsFiltering) {
        return false;
    }

    namespace fs = std::filesystem;
    const std::string stem = solveImagePath.stem().string();
    const fs::path rawXyPath = attemptOutputDirectory / (stem + ".raw.xy.fits");
    const fs::path rawTextPath = attemptOutputDirectory / (stem + ".raw.xy.txt");
    const fs::path filteredTextPath = attemptOutputDirectory / (stem + ".filtered.xy.txt");
    const fs::path filteredFitsPath = attemptOutputDirectory / (stem + ".filtered.xy.fits");
    const fs::path extractStdoutPath = attemptOutputDirectory / "extract-sources.stdout.log";
    const fs::path extractStderrPath = attemptOutputDirectory / "extract-sources.stderr.log";

    std::string extractCommand =
        "solve-field " + ShellEscape(solveImagePath.string()) +
        " --dir " + ShellEscape(attemptOutputDirectory.string()) +
        " --no-plots --no-verify --dont-augment" +
        " --downsample " + std::to_string(std::max(1, values.downsample)) +
        " --keep-xylist " + ShellEscape(rawXyPath.string());
    if (values.overwrite) {
        extractCommand += " --overwrite";
    }
    extractCommand +=
        " > " + ShellEscape(extractStdoutPath.string()) +
        " 2> " + ShellEscape(extractStderrPath.string());

    int status = std::system(extractCommand.c_str());
    if (status == -1 || !fs::exists(rawXyPath)) {
        errorOut = ReadFileToString(extractStderrPath.string());
        if (errorOut.empty()) {
            errorOut = "Failed to extract raw source list for: " + solveImagePath.string();
        }
        return false;
    }

    std::string tablistCommand =
        "tablist " + ShellEscape(rawXyPath.string()) +
        " > " + ShellEscape(rawTextPath.string()) +
        " 2>> " + ShellEscape(extractStderrPath.string());
    status = std::system(tablistCommand.c_str());
    if (status == -1 || !fs::exists(rawTextPath)) {
        errorOut = ReadFileToString(extractStderrPath.string());
        if (errorOut.empty()) {
            errorOut = "Failed to dump extracted source list for: " + solveImagePath.string();
        }
        return false;
    }

    std::vector<ExtractedSource> rawSources;
    if (!ParseTablistSources(ReadFileToString(rawTextPath.string()), rawSources, errorOut)) {
        return false;
    }

    std::vector<ExtractedSource> filteredSources = FilterSources(rawSources, values);
    if (filteredSources.size() < 4) {
        std::stringstream message;
        message << "Filtered source list is too small to solve (kept " << filteredSources.size()
                << " of " << rawSources.size() << " sources).";
        errorOut = message.str();
        return false;
    }

    std::ofstream filteredText(filteredTextPath);
    if (!filteredText.good()) {
        errorOut = "Unable to write filtered source list: " + filteredTextPath.string();
        return false;
    }
    filteredText << std::fixed << std::setprecision(6);
    for (const auto &source : filteredSources) {
        filteredText << source.x << ' ' << source.y << ' ' << source.flux << ' ' << source.background << "\n";
    }
    filteredText.close();

    std::string text2fitsCommand =
        "text2fits -H 'X Y FLUX BACKGROUND' -f dddd " +
        ShellEscape(filteredTextPath.string()) + " " + ShellEscape(filteredFitsPath.string()) +
        " >> " + ShellEscape(extractStdoutPath.string()) +
        " 2>> " + ShellEscape(extractStderrPath.string());
    status = std::system(text2fitsCommand.c_str());
    if (status == -1 || !fs::exists(filteredFitsPath)) {
        errorOut = ReadFileToString(extractStderrPath.string());
        if (errorOut.empty()) {
            errorOut = "Failed to build filtered FITS source list for: " + solveImagePath.string();
        }
        return false;
    }

    BOOST_LOG_TRIVIAL(info)
        << "Filtered sources for " << solveImagePath.filename().string()
        << ": kept " << filteredSources.size() << " of " << rawSources.size()
        << " with minimum separation " << EstimateMinSeparationPixels(values) << " px.";

    catalogPath = filteredFitsPath;
    return true;
}

bool SolveOneInput(const std::filesystem::path &solveImagePath,
                   const std::filesystem::path &attemptOutputDirectory,
                   const BlindSolveOptions &values,
                   const std::string &configArg,
                   BlindSolveResult &result) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(attemptOutputDirectory, ec);
    if (ec) {
        result.stderrText = "Unable to create attempt output directory: " + attemptOutputDirectory.string();
        return false;
    }

    fs::path stdoutPath = attemptOutputDirectory / "blind-solve.stdout.log";
    fs::path stderrPath = attemptOutputDirectory / "blind-solve.stderr.log";
    std::string fullPassCommand;
    bool hasFullPassCommand = false;
    bool fullPassRan = false;
    const int maxObjects = std::max(0, values.maxStarCount);
    fs::path filteredCatalogPath;
    fs::path commandInputPath = solveImagePath;
    int imageWidth = 0;
    int imageHeight = 0;

    std::string filterError;
    if (BuildFilteredSourceCatalog(solveImagePath,
                                   attemptOutputDirectory,
                                   values,
                                   filteredCatalogPath,
                                   imageWidth,
                                   imageHeight,
                                   filterError)) {
        commandInputPath = filteredCatalogPath;
    } else if (!filterError.empty() && (values.maxStarCount > 0 || values.minStarSeparation > 0.0f)) {
        BOOST_LOG_TRIVIAL(warning) << "Falling back to unfiltered blind solve input for "
                                   << solveImagePath.filename().string() << ": " << filterError;
    }

    const bool usingFilteredCatalog = !filteredCatalogPath.empty();

    auto BuildSolveCommand = [&](int downsample, int timeoutSeconds, bool fastPass) {
        std::string command =
            "solve-field " + ShellEscape(commandInputPath.string()) +
            " --dir " + ShellEscape(attemptOutputDirectory.string()) +
            " --no-plots --no-verify" +
            " --scale-units arcsecperpix" +
            " --scale-low " + std::to_string(values.scaleLowArcsecPerPix) +
            " --scale-high " + std::to_string(values.scaleHighArcsecPerPix) +
            " --cpulimit " + std::to_string(timeoutSeconds);

        if (usingFilteredCatalog) {
            command += " --x-column X --y-column Y --sort-column FLUX";
            command += " --width " + std::to_string(imageWidth);
            command += " --height " + std::to_string(imageHeight);
        } else {
            command += " --downsample " + std::to_string(downsample);
        }

        if (fastPass && !usingFilteredCatalog) {
            command += " --no-tweak";
            if (maxObjects > 0) {
                command += " --objs " + std::to_string(maxObjects);
                command += " --depth 1-" + std::to_string(maxObjects);
            }
        }
        if (!configArg.empty()) {
            command += configArg;
        }
        if (values.overwrite) {
            command += " --overwrite";
        }
        command += " > " + ShellEscape(stdoutPath.string()) + " 2> " + ShellEscape(stderrPath.string());
        return command;
    };

    const int fastDownsample = std::max(values.downsample, 4);
    const int fastTimeout = std::min(values.timeoutSeconds, 90);
    fullPassCommand = BuildSolveCommand(values.downsample, values.timeoutSeconds, false);
    hasFullPassCommand = true;
    std::string command = BuildSolveCommand(fastDownsample, fastTimeout, true);

    BOOST_LOG_TRIVIAL(info) << "Running fast blind solve pass: " << command;
    int status = std::system(command.c_str());
    result.stdoutText = ReadFileToString(stdoutPath.string());
    result.stderrText = ReadFileToString(stderrPath.string());

    bool fastPassFailed =
        status == -1 ||
        !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0;

    if (fastPassFailed) {
        const std::string fastPassError = result.stderrText;
        BOOST_LOG_TRIVIAL(info) << "Fast pass failed, retrying with full blind solve pass.";
        BOOST_LOG_TRIVIAL(info) << "Running full blind solve pass: " << fullPassCommand;

        status = std::system(fullPassCommand.c_str());
        fullPassRan = true;
        result.stdoutText = ReadFileToString(stdoutPath.string());
        result.stderrText = ReadFileToString(stderrPath.string());

        if (status == -1) {
            result.stderrText += "\nFailed to execute solve-field.";
            return false;
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            std::stringstream error;
            error << "Fast pass failed then full solve-field exited with status " << WEXITSTATUS(status);
            if (!fastPassError.empty()) {
                error << "\n--- fast pass stderr ---\n" << fastPassError;
            }
            if (!result.stderrText.empty()) {
                error << "\n--- full pass stderr ---\n" << result.stderrText;
            }
            result.stderrText = error.str();
            return false;
        }
    }

    fs::path wcsPath = attemptOutputDirectory / (commandInputPath.stem().string() + ".wcs");
    if (!fs::exists(wcsPath) && hasFullPassCommand && !fullPassRan) {
        BOOST_LOG_TRIVIAL(info) << "Fast pass produced no WCS output, retrying full blind solve pass.";
        BOOST_LOG_TRIVIAL(info) << "Running full blind solve pass: " << fullPassCommand;

        status = std::system(fullPassCommand.c_str());
        fullPassRan = true;
        result.stdoutText = ReadFileToString(stdoutPath.string());
        result.stderrText = ReadFileToString(stderrPath.string());
        if (status == -1) {
            result.stderrText += "\nFailed to execute solve-field.";
            return false;
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            std::stringstream error;
            error << "Full solve-field exited with status " << WEXITSTATUS(status);
            if (!result.stderrText.empty()) {
                error << "\n" << result.stderrText;
            }
            result.stderrText = error.str();
            return false;
        }
    }

    if (!fs::exists(wcsPath)) {
        result.stderrText += "\nsolve-field completed but no WCS file was produced: " + wcsPath.string();
        return false;
    }

    result.wcsFilePath = wcsPath.string();

    fs::path wcsInfoPath = attemptOutputDirectory / "blind-solve.wcsinfo.log";
    std::string wcsInfoCmd =
        "wcsinfo " + ShellEscape(wcsPath.string()) +
        " > " + ShellEscape(wcsInfoPath.string()) +
        " 2>> " + ShellEscape(stderrPath.string());

    int wcsInfoStatus = std::system(wcsInfoCmd.c_str());
    std::string wcsInfoText;
    if (wcsInfoStatus != -1 && WIFEXITED(wcsInfoStatus) && WEXITSTATUS(wcsInfoStatus) == 0) {
        wcsInfoText = ReadFileToString(wcsInfoPath.string());
    }

    result.raDeg = ParseKeyFromWcsInfo(wcsInfoText, "ra_center", ParseSolveFieldValue(result.stdoutText, "RA,Dec\\) = \\(([+-]?[0-9]*\\.?[0-9]+),", 0.0));
    result.decDeg = ParseKeyFromWcsInfo(wcsInfoText, "dec_center", ParseSolveFieldValue(result.stdoutText, "RA,Dec\\) = \\([+-]?[0-9]*\\.?[0-9]+, ([+-]?[0-9]*\\.?[0-9]+)\\)", 0.0));
    result.rotationDeg = ParseKeyFromWcsInfo(wcsInfoText, "orientation", ParseSolveFieldValue(result.stdoutText, "Field rotation angle: up is ([+-]?[0-9]*\\.?[0-9]+)", 0.0));
    result.plateScaleArcsecPerPix = ParseKeyFromWcsInfo(wcsInfoText, "pixscale", ParseSolveFieldValue(result.stdoutText, "pixel scale ([+-]?[0-9]*\\.?[0-9]+)", 0.0));
    result.raHMS = ParseStringKeyFromWcsInfo(wcsInfoText, "ra_center_hms", DegreesToHMS(result.raDeg));
    result.decDMS = ParseStringKeyFromWcsInfo(wcsInfoText, "dec_center_dms", DegreesToDMS(result.decDeg));
    result.fieldWidthDeg = ParseKeyFromWcsInfo(wcsInfoText, "fieldw", 0.0);
    result.fieldHeightDeg = ParseKeyFromWcsInfo(wcsInfoText, "fieldh", 0.0);
    if (result.fieldWidthDeg > 0.0 && result.fieldHeightDeg > 0.0) {
        const double halfWidth = result.fieldWidthDeg * 0.5;
        const double halfHeight = result.fieldHeightDeg * 0.5;
        result.fieldRadiusDeg = std::sqrt((halfWidth * halfWidth) + (halfHeight * halfHeight));
    }
    result.success = true;
    return true;
}

}

BlindSolveResult BlindSolve(const BlindSolveOptions &values) {
    BlindSolveResult result;

    if (values.imagePath.empty()) {
        result.stderrText = "Blind solve requires an image path.";
        return result;
    }

    namespace fs = std::filesystem;
    fs::path imagePath(values.imagePath);
    if (!fs::exists(imagePath)) {
        result.stderrText = "Input image not found: " + values.imagePath;
        return result;
    }

    fs::path outputDirectory(values.outputDirectory.empty() ? "logs/blind-solve" : values.outputDirectory);
    std::error_code ec;
    fs::create_directories(outputDirectory, ec);
    if (ec) {
        result.stderrText = "Unable to create output directory: " + outputDirectory.string();
        return result;
    }

    std::string configArg;
    if (!values.indexDirectory.empty()) {
        fs::path indexDirectory = fs::absolute(fs::path(values.indexDirectory), ec);
        if (ec) {
            result.stderrText = "Unable to resolve index directory path: " + values.indexDirectory;
            return result;
        }

        fs::path astrometryCfgPath = outputDirectory / "astrometry.cfg";
        std::string cfgError;
        if (!BuildAstrometryConfig(indexDirectory, astrometryCfgPath, cfgError)) {
            result.stderrText = cfgError;
            return result;
        }
        configArg = " --config " + ShellEscape(astrometryCfgPath.string());
    }

    std::vector<fs::path> solveInputs;
    std::string preprocessError;
    if (!BuildPreprocessedInputs(imagePath, outputDirectory, values, solveInputs, preprocessError)) {
        result.stderrText = preprocessError;
        return result;
    }

    BOOST_LOG_TRIVIAL(info)
        << "Blind solve profile=" << values.profile
        << " scale=[" << values.scaleLowArcsecPerPix << ", " << values.scaleHighArcsecPerPix << "]"
        << " downsample=" << values.downsample
        << " preprocess=" << (values.preprocessEnabled ? "on" : "off")
        << " mode=" << values.preprocessMode
        << " target_width=" << values.preprocessTargetWidth
        << " max_dim=" << values.preprocessMaxDimension
        << " max_star_count=" << values.maxStarCount
        << " min_star_separation=" << values.minStarSeparation;

    std::vector<std::string> attemptErrors;
    for (size_t i = 0; i < solveInputs.size(); ++i) {
        fs::path attemptDir = outputDirectory / ("attempt_" + std::to_string(i + 1));
        BlindSolveResult attemptResult;
        if (SolveOneInput(solveInputs[i], attemptDir, values, configArg, attemptResult)) {
            return attemptResult;
        }

        std::stringstream error;
        error << "Attempt " << (i + 1) << " failed for input " << solveInputs[i].filename().string();
        if (!attemptResult.stderrText.empty()) {
            error << ": " << attemptResult.stderrText;
        }
        attemptErrors.push_back(error.str());
    }

    std::stringstream combined;
    combined << "Blind solve failed for all preprocessed inputs.";
    for (const auto &error : attemptErrors) {
        combined << "\n- " << error;
    }
    result.stderrText = combined.str();
    return result;
}

}