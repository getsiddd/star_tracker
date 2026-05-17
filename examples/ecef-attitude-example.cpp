/**
 * @file ecef-attitude-example.cpp
 * 
 * Example demonstrating ECEF attitude calculation and output
 * Shows integration of ECEF transformations with star tracker results
 */

#include "ecef-attitude.hpp"
#include "attitude-utils.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>

using namespace lost;

/**
 * Example function showing how to compute and output ECEF attitude
 */
void ExampleECEFAttitudeCalculation() {
    std::cout << "\n=== ECEF Attitude Calculation Example ===" << std::endl;
    
    // Step 1: Get current time
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    struct tm* timeinfo = std::gmtime(&time_t);
    
    int year = timeinfo->tm_year + 1900;
    int month = timeinfo->tm_mon + 1;
    int day = timeinfo->tm_mday;
    int hour = timeinfo->tm_hour;
    int minute = timeinfo->tm_min;
    double second = timeinfo->tm_sec;
    
    std::cout << "\nCurrent UTC Time: " 
              << std::setfill('0')
              << std::setw(4) << year << "-"
              << std::setw(2) << month << "-"
              << std::setw(2) << day << "T"
              << std::setw(2) << hour << ":"
              << std::setw(2) << minute << ":"
              << std::setw(2) << (int)second << "Z" << std::endl;
    
    // Step 2: Calculate Julian Date
    double jd = TimeToJulianDate(year, month, day, hour, minute, second);
    std::cout << "\nJulian Date: " << std::fixed << std::setprecision(6) << jd << std::endl;
    
    // Step 3: Calculate GMST
    double gmst = CalculateGMST(jd);
    std::cout << "GMST: " << std::fixed << std::setprecision(6) << gmst << " rad ("
              << gmst * 180.0 / M_PI << "°)" << std::endl;
    
    // Step 4: Create a sample ECI attitude (from star tracker output)
    // This would normally come from attitude estimation algorithms
    Vec3 axis(0, 0, 1);  // Z-axis
    float angle = 0.1f;   // Small rotation
    Quaternion eciQuaternion(axis, angle);
    Attitude eciAttitude(eciQuaternion);
    
    std::cout << "\n--- ECI Frame Attitude (Star Tracker Output) ---" << std::endl;
    std::cout << "Quaternion: ("
              << eciQuaternion.real << ", "
              << eciQuaternion.i << ", "
              << eciQuaternion.j << ", "
              << eciQuaternion.k << ")" << std::endl;
    
    // Step 5: Transform to ECEF
    ECEFAttitude ecefAttitude = GetECEFAttitude(eciAttitude, gmst);
    
    std::cout << "\n--- ECEF Frame Attitude (Computed) ---" << std::endl;
    std::cout << "Quaternion: ("
              << ecefAttitude.quaternion.real << ", "
              << ecefAttitude.quaternion.i << ", "
              << ecefAttitude.quaternion.j << ", "
              << ecefAttitude.quaternion.k << ")" << std::endl;
    
    // Step 6: Create sample satellite position (LEO orbit, ~400 km altitude)
    // International Space Station-like orbit
    double altitude_km = 400.0;
    double latitude = 51.6;   // ISS inclination
    double longitude = 0.0;
    
    Position3D satECEF = GeodeticToECEF(latitude, longitude, altitude_km);
    
    std::cout << "\n--- Satellite Position (ECEF) ---" << std::endl;
    std::cout << "X: " << satECEF.x / 1000.0 << " km" << std::endl;
    std::cout << "Y: " << satECEF.y / 1000.0 << " km" << std::endl;
    std::cout << "Z: " << satECEF.z / 1000.0 << " km" << std::endl;
    std::cout << "Distance from Earth center: " 
              << satECEF.Magnitude() / 1000.0 << " km" << std::endl;
    
    // Step 7: Calculate nadir pointing information
    Vec3 nadirDir = GetNadirDirection(satECEF);
    double nadirAngle = GetNadirPointingAngle(ecefAttitude, nadirDir);
    
    std::cout << "\n--- Nadir Pointing Analysis ---" << std::endl;
    std::cout << "Nadir direction: ("
              << nadirDir.x << ", "
              << nadirDir.y << ", "
              << nadirDir.z << ")" << std::endl;
    std::cout << "Z-axis pointing angle from nadir: " 
              << nadirAngle * 180.0 / M_PI << "°" << std::endl;
    
    if (nadirAngle < 0.1) {
        std::cout << "✓ Spacecraft is nadir-pointing (Earth-facing)" << std::endl;
    } else {
        std::cout << "✗ Spacecraft is NOT nadir-pointing" << std::endl;
    }
    
    // Step 8: Output complete formatted results
    std::cout << FormatECEFAttitudeOutput(ecefAttitude, satECEF);
}

/**
 * Example with multiple orbits around Earth
 */
void ExampleOrbitTrajectory() {
    std::cout << "\n=== Orbit Trajectory Example ===" << std::endl;
    
    // Calculate GMST for current time
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    struct tm* timeinfo = std::gmtime(&time_t);
    
    int year = timeinfo->tm_year + 1900;
    int month = timeinfo->tm_mon + 1;
    int day = timeinfo->tm_mday;
    int hour = timeinfo->tm_hour;
    int minute = timeinfo->tm_min;
    double second = timeinfo->tm_sec;
    
    double jd = TimeToJulianDate(year, month, day, hour, minute, second);
    double gmst = CalculateGMST(jd);
    
    std::cout << "\nTrajectory around Earth (10 points):\n" << std::endl;
    std::cout << std::setw(5) << "Pt" 
              << std::setw(12) << "Lon (°)"
              << std::setw(12) << "Lat (°)"
              << std::setw(12) << "Alt (km)"
              << std::setw(15) << "Dist from EC (km)" << std::endl;
    std::cout << std::string(56, '-') << std::endl;
    
    for (int i = 0; i < 10; ++i) {
        double longitude = (i / 10.0) * 360.0;
        double latitude = 51.6;  // ISS inclination
        double altitude = 400.0;
        
        Position3D pos = GeodeticToECEF(latitude, longitude, altitude);
        OrbitalState state = ECEFToGeodetic(pos);
        
        std::cout << std::setw(5) << i
                  << std::fixed << std::setprecision(2)
                  << std::setw(12) << state.longitude
                  << std::setw(12) << state.latitude
                  << std::setw(12) << state.altitude
                  << std::setw(15) << pos.Magnitude() / 1000.0 << std::endl;
    }
}

/**
 * Example demonstrating attitude transformations
 */
void ExampleAttitudeTransformations() {
    std::cout << "\n=== Attitude Transformation Example ===" << std::endl;
    
    // Create a rotation quaternion in ECI frame
    float angle = M_PI / 4.0;  // 45 degrees
    Vec3 axis = Vec3(1, 0, 0).Normalize();
    Quaternion eciQuat(axis, angle);
    
    std::cout << "\nOriginal ECI Quaternion (45° rotation around X-axis):" << std::endl;
    std::cout << "(" << eciQuat.real << ", " 
              << eciQuat.i << ", " 
              << eciQuat.j << ", " 
              << eciQuat.k << ")" << std::endl;
    
    // Transform through different GMST values
    std::cout << "\nTransformation through different GMST values:\n" << std::endl;
    std::cout << std::setw(15) << "GMST (rad)"
              << std::setw(20) << "ECEF Quaternion" << std::endl;
    std::cout << std::string(35, '-') << std::endl;
    
    for (int i = 0; i <= 4; ++i) {
        double gmst = (i / 4.0) * 2.0 * M_PI;
        Quaternion ecefQuat = TransformECItoECEF(eciQuat, gmst);
        
        std::cout << std::fixed << std::setprecision(4)
                  << std::setw(15) << gmst
                  << std::setw(20) << "["
                  << ecefQuat.real << ", "
                  << ecefQuat.i << ", "
                  << ecefQuat.j << ", "
                  << ecefQuat.k << "]" << std::endl;
    }
}

// Main function for demonstration
int main(int argc, char** argv) {
    std::cout << "╔════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  ECEF Attitude System - Comprehensive Example          ║" << std::endl;
    std::cout << "║  Demonstrates star tracker integration with ECEF       ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════╝" << std::endl;
    
    // Run examples
    ExampleECEFAttitudeCalculation();
    ExampleOrbitTrajectory();
    ExampleAttitudeTransformations();
    
    std::cout << "\n═══════════════════════════════════════════════════════" << std::endl;
    std::cout << "✓ Examples completed successfully!" << std::endl;
    std::cout << "\nNext steps:" << std::endl;
    std::cout << "  1. Integrate GetECEFAttitude() into your attitude estimation pipeline" << std::endl;
    std::cout << "  2. Use FormatECEFAttitudeOutput() for result logging" << std::endl;
    std::cout << "  3. Export data with positions and quaternions to JSON" << std::endl;
    std::cout << "  4. Visualize with: python3 tools/ecef_visualization.py" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════\n" << std::endl;
    
    return 0;
}
