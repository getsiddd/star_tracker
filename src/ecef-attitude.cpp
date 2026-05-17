#include "ecef-attitude.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>

namespace lost {

// WGS84 Earth constants
static constexpr double EARTH_RADIUS_A = 6378137.0;      // Semi-major axis (meters)
static constexpr double EARTH_RADIUS_B = 6356752.3142;   // Semi-minor axis (meters)
static constexpr double EARTH_ECCENTRICITY_SQ = 0.0066943799901;  // e^2

/**
 * Calculate Greenwich Mean Sidereal Time
 * Formula based on USNO Circular 163 and standard astronomical algorithms
 */
double CalculateGMST(double julianDate) {
    double jd0 = std::floor(julianDate + 0.5) - 0.5;
    double jCent = (jd0 - 2451545.0) / 36525.0;
    double jCentFrac = (julianDate + 0.5 - std::floor(julianDate + 0.5));
    
    // Earth rotation angle (radians)
    double angle = 2.0 * M_PI * (0.7790572732640 + 1.00273781191135448 * jCentFrac);
    angle = std::fmod(angle, 2.0 * M_PI);
    
    if (angle < 0) angle += 2.0 * M_PI;
    
    return angle;
}

/**
 * Convert calendar date/time to Julian Date
 */
double TimeToJulianDate(int year, int month, int day, 
                        int hour, int minute, double second) {
    // Adjust for January/February
    int a = (14 - month) / 12;
    int y = year + 4800 - a;
    int m = month + 12 * a - 3;
    
    // Julian Day Number (noon)
    int jdn = day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
    
    // Convert time to fractional day
    double fracDay = (hour + minute / 60.0 + second / 3600.0) / 24.0;
    
    // Julian Date
    return jdn + fracDay;
}

/**
 * Create rotation quaternion for Z-axis rotation (GMST rotation)
 */
static Quaternion CreateZAxisRotation(double angle) {
    // Quaternion for rotation around Z-axis: q = [cos(θ/2), 0, 0, sin(θ/2)]
    double halfAngle = angle / 2.0;
    return Quaternion(Vec3(0, 0, std::sin(halfAngle)), 2.0 * halfAngle);
}

/**
 * Transform ECI attitude to ECEF
 * ECEF = R_z(GMST) * ECI, where R_z is rotation around Z-axis
 */
Quaternion TransformECItoECEF(const Quaternion& eciQuaternion, double gmst) {
    // Earth rotation quaternion (rotation around Z-axis by GMST)
    Quaternion earthRotation = CreateZAxisRotation(gmst);
    
    // ECEF = Earth rotation * ECI
    return earthRotation * eciQuaternion;
}

/**
 * Transform ECEF attitude to ECI
 * ECI = R_z(-GMST) * ECEF
 */
Quaternion TransformECEFtoECI(const Quaternion& ecefQuaternion, double gmst) {
    // Inverse rotation (negative GMST)
    Quaternion earthRotationInverse = CreateZAxisRotation(-gmst);
    
    // ECI = Earth rotation inverse * ECEF
    return earthRotationInverse * ecefQuaternion;
}

/**
 * Get complete ECEF attitude
 */
ECEFAttitude GetECEFAttitude(const Attitude& eciAttitude, double gmst) {
    ECEFAttitude ecefAtt;
    
    if (!eciAttitude.IsKnown()) {
        return ecefAtt;  // Return empty/invalid attitude
    }
    
    // Transform quaternion from ECI to ECEF
    Quaternion eciQuat = eciAttitude.GetQuaternion();
    ecefAtt.quaternion = TransformECItoECEF(eciQuat, gmst);
    
    // Get other representations
    ecefAtt.dcm = QuaternionToDCM(ecefAtt.quaternion);
    ecefAtt.eulerAngles = Attitude(ecefAtt.quaternion).ToSpherical();
    
    // Get axis directions (how spacecraft axes point in ECEF)
    ecefAtt.xAxis = ecefAtt.quaternion.Rotate(Vec3(1, 0, 0));
    ecefAtt.yAxis = ecefAtt.quaternion.Rotate(Vec3(0, 1, 0));
    ecefAtt.zAxis = ecefAtt.quaternion.Rotate(Vec3(0, 0, 1));
    
    return ecefAtt;
}

/**
 * Convert ECEF coordinates to geodetic (lat/lon/alt)
 * Using iterative method
 */
OrbitalState ECEFToGeodetic(const Position3D& position) {
    OrbitalState state;
    state.ecefPosition = position;
    
    // Convert from meters to km
    double x = position.x / 1000.0;
    double y = position.y / 1000.0;
    double z = position.z / 1000.0;
    
    double a = EARTH_RADIUS_A / 1000.0;  // Convert to km
    double e2 = EARTH_ECCENTRICITY_SQ;
    
    double p = std::sqrt(x*x + y*y);
    
    // Longitude
    state.longitude = std::atan2(y, x) * 180.0 / M_PI;
    
    // Latitude and altitude (iterative method)
    double lat = std::atan2(z, p * (1.0 - e2));
    double alt = 0;
    
    for (int i = 0; i < 5; ++i) {
        double sinLat = std::sin(lat);
        double N = a / std::sqrt(1.0 - e2 * sinLat * sinLat);
        alt = p / std::cos(lat) - N;
        lat = std::atan2(z, p * (1.0 - e2 * N / (N + alt)));
    }
    
    state.latitude = lat * 180.0 / M_PI;
    state.altitude = alt;
    
    return state;
}

/**
 * Convert geodetic to ECEF coordinates
 */
Position3D GeodeticToECEF(double latitude, double longitude, double altitude) {
    double lat_rad = latitude * M_PI / 180.0;
    double lon_rad = longitude * M_PI / 180.0;
    double alt_km = altitude;  // Already in km
    
    double a = EARTH_RADIUS_A / 1000.0;  // Convert to km
    double e2 = EARTH_ECCENTRICITY_SQ;
    
    double sinLat = std::sin(lat_rad);
    double cosLat = std::cos(lat_rad);
    double sinLon = std::sin(lon_rad);
    double cosLon = std::cos(lon_rad);
    
    double N = a / std::sqrt(1.0 - e2 * sinLat * sinLat);
    
    double x = (N + alt_km) * cosLat * cosLon * 1000.0;  // Back to meters
    double y = (N + alt_km) * cosLat * sinLon * 1000.0;
    double z = (N * (1.0 - e2) + alt_km) * sinLat * 1000.0;
    
    return Position3D(x, y, z);
}

/**
 * Get nadir direction (toward Earth center)
 */
Vec3 GetNadirDirection(const Position3D& satPosition) {
    float x = static_cast<float>(-satPosition.x);
    float y = static_cast<float>(-satPosition.y);
    float z = static_cast<float>(-satPosition.z);
    
    Vec3 nadir(x, y, z);
    return nadir.Normalize();
}

/**
 * Get zenith direction (away from Earth)
 */
Vec3 GetZenithDirection(const Position3D& satPosition) {
    float x = static_cast<float>(satPosition.x);
    float y = static_cast<float>(satPosition.y);
    float z = static_cast<float>(satPosition.z);
    
    Vec3 zenith(x, y, z);
    return zenith.Normalize();
}

/**
 * Calculate nadir pointing angle
 */
double GetNadirPointingAngle(const ECEFAttitude& ecefAttitude, 
                             const Vec3& nadirDirection) {
    // Typically Z-axis of spacecraft should point to nadir
    float dotProduct = ecefAttitude.zAxis * nadirDirection;
    
    // Clamp to [-1, 1] to avoid numerical errors with acos
    dotProduct = (dotProduct > 1.0f) ? 1.0f : ((dotProduct < -1.0f) ? -1.0f : dotProduct);
    
    return std::acos(static_cast<double>(dotProduct));
}

/**
 * Format ECEF attitude for output
 */
std::string FormatECEFAttitudeOutput(const ECEFAttitude& ecefAttitude,
                                     const Position3D& satPosition) {
    std::stringstream ss;
    
    ss << "\n=== ECEF ATTITUDE OUTPUT ===\n";
    ss << std::fixed << std::setprecision(6);
    
    ss << "Quaternion (ECEF Frame):\n";
    ss << "  real: " << ecefAttitude.quaternion.real << "\n";
    ss << "  i: " << ecefAttitude.quaternion.i << "\n";
    ss << "  j: " << ecefAttitude.quaternion.j << "\n";
    ss << "  k: " << ecefAttitude.quaternion.k << "\n";
    
    ss << "\nEuler Angles (RA, Dec, Roll in radians):\n";
    ss << "  RA: " << ecefAttitude.eulerAngles.ra << " rad\n";
    ss << "  Dec: " << ecefAttitude.eulerAngles.de << " rad\n";
    ss << "  Roll: " << ecefAttitude.eulerAngles.roll << " rad\n";
    
    ss << "\nSpacecraft Axis Directions (ECEF):\n";
    ss << "  X-axis: (" << ecefAttitude.xAxis.x << ", " 
       << ecefAttitude.xAxis.y << ", " << ecefAttitude.xAxis.z << ")\n";
    ss << "  Y-axis: (" << ecefAttitude.yAxis.x << ", " 
       << ecefAttitude.yAxis.y << ", " << ecefAttitude.yAxis.z << ")\n";
    ss << "  Z-axis: (" << ecefAttitude.zAxis.x << ", " 
       << ecefAttitude.zAxis.y << ", " << ecefAttitude.zAxis.z << ")\n";
    
    ss << "\nSatellite Position (ECEF):\n";
    ss << "  X: " << satPosition.x / 1000.0 << " km\n";
    ss << "  Y: " << satPosition.y / 1000.0 << " km\n";
    ss << "  Z: " << satPosition.z / 1000.0 << " km\n";
    ss << "  Distance from Earth center: " << satPosition.Magnitude() / 1000.0 << " km\n";
    
    OrbitalState state = ECEFToGeodetic(satPosition);
    ss << "\nGeodetic Coordinates:\n";
    ss << "  Latitude: " << state.latitude << "°\n";
    ss << "  Longitude: " << state.longitude << "°\n";
    ss << "  Altitude: " << state.altitude << " km\n";
    
    ss << "\n=============================\n";
    
    return ss.str();
}

// Forward declarations needed for Quaternion access
// These assume the Quaternion class has public getters
// If not, we'll need to add them to attitude-utils.hpp

}  // namespace lost
