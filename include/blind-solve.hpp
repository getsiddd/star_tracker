#ifndef BLIND_SOLVE_H
#define BLIND_SOLVE_H

#include <string>

namespace lost {

class BlindSolveOptions {
public:
    std::string imagePath = "";
    std::string indexDirectory = "";
    std::string outputDirectory = "logs/blind-solve";
    float scaleLowArcsecPerPix = 0.1f;
    float scaleHighArcsecPerPix = 120.0f;
    int downsample = 2;
    int timeoutSeconds = 120;
    bool overwrite = false;

    // Image preprocessing (enabled by default).
    bool preprocessEnabled = true;
    // Supported modes: "global" (resize whole image) or "tiles" (crop overlapping tiles).
    std::string preprocessMode = "global";
    // Longest side limit for global resize while preserving aspect ratio.
    int preprocessMaxDimension = 2200;
    // If >0, force global preprocessing resize to this width (height kept proportional).
    int preprocessTargetWidth = 720;

    // Tile mode settings.
    int tileWidth = 1800;
    int tileHeight = 1800;
    int tileOverlap = 300;

    // ECEF and visualization options (defaults to enabled for both)
    bool ecefEnabled = true;              // Enable ECEF transformation (default: true)
    bool visualization3DEnabled = true;   // Enable 3D visualization (default: true)
    std::string visualizationOutputPath = "";  // Path to save 3D visualization (PNG)
    std::string ecefOutputPath = "";      // Path to save ECEF attitude data (JSON)
    bool ecefConsoleOutput = true;        // Print ECEF attitude to console (default: true)
};

class BlindSolveResult {
public:
    bool success = false;
    double raDeg = 0.0;
    double decDeg = 0.0;
    double rotationDeg = 0.0;
    double plateScaleArcsecPerPix = 0.0;
    double fieldWidthDeg = 0.0;
    double fieldHeightDeg = 0.0;
    double fieldRadiusDeg = 0.0;
    std::string raHMS = "";        // RA in hh:mm:ss.ss format
    std::string decDMS = "";       // Dec in ±dd:mm:ss.ss format
    std::string wcsFilePath = "";
    std::string stdoutText = "";
    std::string stderrText = "";
};

BlindSolveResult BlindSolve(const BlindSolveOptions &values);

}

#endif