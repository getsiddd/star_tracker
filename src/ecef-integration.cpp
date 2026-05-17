#include "ecef-integration.hpp"
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <cstdlib>
#include <ctime>

namespace lost {

// Convert RA, Dec, Roll to quaternion attitude
// This assumes the spacecraft Z-axis points in the RA/Dec direction
// and the X-axis is rotated by Roll around the Z-axis
Quaternion BlindSolveToAttitude(double raDeg, double decDeg, double rollDeg) {
    // Convert degrees to radians
    double ra = raDeg * M_PI / 180.0;
    double dec = decDeg * M_PI / 180.0;
    double roll = rollDeg * M_PI / 180.0;
    
    // Create attitude from Euler angles
    // Use standard aerospace sequence: yaw (RA), pitch (Dec), roll (Roll)
    double cy = cos(ra * 0.5);
    double sy = sin(ra * 0.5);
    double cp = cos(dec * 0.5);
    double sp = sin(dec * 0.5);
    double cr = cos(roll * 0.5);
    double sr = sin(roll * 0.5);
    
    // ZYX Euler angle conversion to quaternion
    double q_real = cr * cp * cy + sr * sp * sy;
    double q_i = sr * cp * cy - cr * sp * sy;
    double q_j = cr * sp * cy + sr * cp * sy;
    double q_k = cr * cp * sy - sr * sp * cy;
    
    return Quaternion(q_real, q_i, q_j, q_k);
}

std::string ProcessBlindSolveToECEF(const BlindSolveResult &result, const BlindSolveOptions &options) {
    std::stringstream output;
    
    if (!options.ecefEnabled) {
        return "";
    }
    
    try {
        // Convert blind solve result to ECI quaternion attitude
        Quaternion eciQuat = BlindSolveToAttitude(result.raDeg, result.decDeg, result.rotationDeg);
        
        // Create Attitude object from quaternion
        Attitude eciAttitude(eciQuat);
        
        // Get current time and calculate GMST
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        struct tm timeinfo;
        gmtime_r(&time_t_now, &timeinfo);

        int year = timeinfo.tm_year + 1900;
        int month = timeinfo.tm_mon + 1;
        int day = timeinfo.tm_mday;
        int hour = timeinfo.tm_hour;
        int minute = timeinfo.tm_min;
        double second = static_cast<double>(timeinfo.tm_sec);
        
        double jd = TimeToJulianDate(year, month, day, hour, minute, second);
        double gmst = CalculateGMST(jd);
        
        // Transform to ECEF
        ECEFAttitude ecefAtt = GetECEFAttitude(eciAttitude, gmst);
        
        // Dummy satellite position (400 km altitude at equator)
        Position3D satPos = GeodeticToECEF(0.0, 0.0, 400.0);
        
        if (options.ecefConsoleOutput) {
            output << "\n=== ECEF Attitude ===\n";
            output << FormatECEFAttitudeOutput(ecefAtt, satPos);
            output << "\n";
        }
        
        char timestampBuffer[64] = {0};
        strftime(timestampBuffer, sizeof(timestampBuffer), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
        std::string timestamp(timestampBuffer);

        // Export to JSON if specified
        if (!options.ecefOutputPath.empty()) {
            ExportECEFToJSON(result, ecefAtt, satPos, jd, gmst, timestamp, options);
        }
        
        // Generate visualization if enabled
        if (options.visualization3DEnabled) {
            std::cout << "\n[ECEF] Generating 3D visualization..." << std::endl;
            std::string vizCmd = GenerateVisualizationCommand(ecefAtt, satPos, options);
            int ret = system(vizCmd.c_str());
            if (ret == 0 && !options.visualizationOutputPath.empty()) {
                std::cout << "[ECEF] Visualization saved to: " << options.visualizationOutputPath << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        output << "\n[ECEF] Error processing ECEF attitude: " << e.what() << "\n";
    }
    
    return output.str();
}

bool ExportECEFToJSON(const BlindSolveResult &result,
                      const ECEFAttitude &ecefAtt,
                      const Position3D &satPos,
                      double julianDate,
                      double gmst,
                      const std::string &timestamp,
                      const BlindSolveOptions &options) {
    if (options.ecefOutputPath.empty()) {
        return false;
    }
    
    try {
        std::ofstream file(options.ecefOutputPath);
        if (!file.is_open()) {
            std::cerr << "[ECEF] Failed to open output file: " << options.ecefOutputPath << std::endl;
            return false;
        }
        
           OrbitalState geodeticState = ECEFToGeodetic(satPos);
           const double distanceKm = satPos.Magnitude() / 1000.0;

           // Manually construct JSON
        file << "{\n";
        file << std::fixed << std::setprecision(6);

           // Timestamp and astronomy time terms.
           file << "  \"timestamp_utc\": \"" << timestamp << "\",\n";
           file << "  \"time\": {\n";
           file << "    \"julian_date\": " << julianDate << ",\n";
           file << "    \"gmst_rad\": " << gmst << ",\n";
           file << "    \"gmst_deg\": " << (gmst * 180.0 / M_PI) << "\n";
           file << "  },\n";

           // Blind solve metadata.
           file << "  \"blind_solve\": {\n";
           file << "    \"success\": " << (result.success ? "true" : "false") << ",\n";
           file << "    \"ra_deg\": " << result.raDeg << ",\n";
           file << "    \"dec_deg\": " << result.decDeg << ",\n";
           file << "    \"ra_hms\": \"" << result.raHMS << "\",\n";
           file << "    \"dec_dms\": \"" << result.decDMS << "\",\n";
           file << "    \"rotation_deg\": " << result.rotationDeg << ",\n";
           file << "    \"plate_scale_arcsec_per_pix\": " << result.plateScaleArcsecPerPix << ",\n";
           file << "    \"field_width_arcmin\": " << (result.fieldWidthDeg * 60.0) << ",\n";
           file << "    \"field_height_arcmin\": " << (result.fieldHeightDeg * 60.0) << ",\n";
           file << "    \"field_radius_deg\": " << result.fieldRadiusDeg << ",\n";
           file << "    \"wcs_file\": \"" << result.wcsFilePath << "\"\n";
           file << "  },\n";

           // ECEF attitude block.
           file << "  \"ecef\": {\n";
           file << "    \"quaternion\": {\n";
           file << "      \"real\": " << ecefAtt.quaternion.real << ",\n";
           file << "      \"i\": " << ecefAtt.quaternion.i << ",\n";
           file << "      \"j\": " << ecefAtt.quaternion.j << ",\n";
           file << "      \"k\": " << ecefAtt.quaternion.k << "\n";
           file << "    },\n";
           file << "    \"euler_angles_rad\": {\n";
           file << "      \"ra\": " << ecefAtt.eulerAngles.ra << ",\n";
           file << "      \"dec\": " << ecefAtt.eulerAngles.de << ",\n";
           file << "      \"roll\": " << ecefAtt.eulerAngles.roll << "\n";
           file << "    },\n";
           file << "    \"axis_directions\": {\n";
           file << "      \"x\": [" << ecefAtt.xAxis.x << ", " << ecefAtt.xAxis.y << ", " << ecefAtt.xAxis.z << "],\n";
           file << "      \"y\": [" << ecefAtt.yAxis.x << ", " << ecefAtt.yAxis.y << ", " << ecefAtt.yAxis.z << "],\n";
           file << "      \"z\": [" << ecefAtt.zAxis.x << ", " << ecefAtt.zAxis.y << ", " << ecefAtt.zAxis.z << "]\n";
           file << "    }\n";
           file << "  },\n";

           // Position and geodetic coordinate blocks.
           file << "  \"position_info\": {\n";
           file << "    \"ecef_km\": {\n";
           file << "      \"x\": " << satPos.x / 1000.0 << ",\n";
           file << "      \"y\": " << satPos.y / 1000.0 << ",\n";
           file << "      \"z\": " << satPos.z / 1000.0 << "\n";
           file << "    },\n";
           file << "    \"distance_from_earth_center_km\": " << distanceKm << "\n";
           file << "  },\n";
           file << "  \"geodetic\": {\n";
           file << "    \"latitude_deg\": " << geodeticState.latitude << ",\n";
           file << "    \"longitude_deg\": " << geodeticState.longitude << ",\n";
           file << "    \"altitude_km\": " << geodeticState.altitude << "\n";
           file << "  },\n";

           // Compatibility keys retained for existing tooling.
           file << "  \"position\": ["
               << satPos.x / 1000.0 << ", "
               << satPos.y / 1000.0 << ", "
               << satPos.z / 1000.0 << "],\n";
           file << "  \"quaternion\": ["
               << ecefAtt.quaternion.real << ", "
               << ecefAtt.quaternion.i << ", "
               << ecefAtt.quaternion.j << ", "
               << ecefAtt.quaternion.k << "],\n";
           file << "  \"euler_angles\": ["
               << ecefAtt.eulerAngles.ra << ", "
               << ecefAtt.eulerAngles.de << ", "
               << ecefAtt.eulerAngles.roll << "],\n";
           file << "  \"x_axis\": ["
               << ecefAtt.xAxis.x << ", "
               << ecefAtt.xAxis.y << ", "
               << ecefAtt.xAxis.z << "],\n";
           file << "  \"y_axis\": ["
               << ecefAtt.yAxis.x << ", "
               << ecefAtt.yAxis.y << ", "
               << ecefAtt.yAxis.z << "],\n";
           file << "  \"z_axis\": ["
               << ecefAtt.zAxis.x << ", "
               << ecefAtt.zAxis.y << ", "
               << ecefAtt.zAxis.z << "]\n";
        
        file << "}\n";
        file.close();
        
        std::cout << "[ECEF] ECEF attitude exported to: " << options.ecefOutputPath << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[ECEF] Error exporting to JSON: " << e.what() << std::endl;
        return false;
    }
}

std::string GenerateVisualizationCommand(const ECEFAttitude &ecefAtt, const Position3D &satPos,
                                         const BlindSolveOptions &options) {
    // Generate temporary JSON for visualization
    std::string tmpJson = "/tmp/ecef_viz_temp.json";
    std::ofstream file(tmpJson);
    
    file << std::fixed << std::setprecision(6);
    file << "{\n";
    file << "  \"position\": [" 
         << satPos.x / 1000.0 << ", " 
         << satPos.y / 1000.0 << ", " 
         << satPos.z / 1000.0 << "],\n";
    file << "  \"quaternion\": [" 
         << ecefAtt.quaternion.real << ", " 
         << ecefAtt.quaternion.i << ", " 
         << ecefAtt.quaternion.j << ", " 
         << ecefAtt.quaternion.k << "]\n";
    file << "}\n";
    file.close();
    
    std::stringstream cmd;
    cmd << "python3 tools/ecef_visualization.py --json " << tmpJson;
    
    if (!options.visualizationOutputPath.empty()) {
        cmd << " --output " << options.visualizationOutputPath;
    }
    
    cmd << " 2>/dev/null";  // Suppress Python output
    
    return cmd.str();
}

}  // namespace lost
