#ifndef ECEF_ATTITUDE_H
#define ECEF_ATTITUDE_H

#include <cmath>
#include <vector>
#include "attitude-utils.hpp"

namespace lost {

/**
 * ECEF (Earth-Centered Earth-Fixed) Attitude and Coordinate Transformation System
 * 
 * Handles transformation between different reference frames:
 * - Body Frame: Spacecraft's own coordinate system
 * - ECI Frame: Earth-Centered Inertial (fixed to stars)
 * - ECEF Frame: Earth-Centered Earth-Fixed (rotates with Earth)
 */

/// Represents a spacecraft position in 3D space
struct Position3D {
    double x;  // meters or km
    double y;
    double z;
    
    Position3D() : x(0), y(0), z(0) {}
    Position3D(double x, double y, double z) : x(x), y(y), z(z) {}
    
    double Magnitude() const {
        return std::sqrt(x*x + y*y + z*z);
    }
};

/// Represents attitude in ECEF frame with multiple representations
struct ECEFAttitude {
    Quaternion quaternion;      // Quaternion representation
    EulerAngles eulerAngles;    // Euler angles (RA, Dec, Roll)
    Mat3 dcm;                   // Direction Cosine Matrix
    Vec3 xAxis;                 // Direction spacecraft X-axis points
    Vec3 yAxis;                 // Direction spacecraft Y-axis points
    Vec3 zAxis;                 // Direction spacecraft Z-axis points (typically nadir/Earth-pointing)
};

/// Time and orbital parameters
struct OrbitalState {
    double julianDate;          // Julian Date for epoch
    double gmst;                // Greenwich Mean Sidereal Time (radians)
    Position3D ecefPosition;    // Satellite position in ECEF frame
    Position3D eciPosition;     // Satellite position in ECI frame
    double latitude;            // Geodetic latitude (degrees)
    double longitude;           // Geodetic longitude (degrees)
    double altitude;            // Altitude above ellipsoid (km)
};

/**
 * Calculate Greenwich Mean Sidereal Time
 * @param julianDate Julian Date (UT1 or UTC)
 * @return GMST in radians (0 to 2*pi)
 */
double CalculateGMST(double julianDate);

/**
 * Convert current time to Julian Date
 * @param year Year (e.g., 2026)
 * @param month Month (1-12)
 * @param day Day (1-31)
 * @param hour Hour (0-23)
 * @param minute Minute (0-59)
 * @param second Second (0-59)
 * @return Julian Date
 */
double TimeToJulianDate(int year, int month, int day, 
                        int hour, int minute, double second);

/**
 * Transform quaternion from ECI frame to ECEF frame
 * @param eciQuaternion Attitude quaternion in ECI frame
 * @param gmst Greenwich Mean Sidereal Time (radians)
 * @return Attitude quaternion in ECEF frame
 */
Quaternion TransformECItoECEF(const Quaternion& eciQuaternion, double gmst);

/**
 * Transform quaternion from ECEF frame to ECI frame
 * @param ecefQuaternion Attitude quaternion in ECEF frame
 * @param gmst Greenwich Mean Sidereal Time (radians)
 * @return Attitude quaternion in ECI frame
 */
Quaternion TransformECEFtoECI(const Quaternion& ecefQuaternion, double gmst);

/**
 * Get complete ECEF attitude information from ECI attitude
 * @param eciAttitude Attitude in ECI frame
 * @param gmst Greenwich Mean Sidereal Time (radians)
 * @return Complete ECEF attitude with all representations
 */
ECEFAttitude GetECEFAttitude(const Attitude& eciAttitude, double gmst);

/**
 * Convert cartesian ECEF position to geodetic coordinates
 * @param position Position in ECEF coordinates
 * @return OrbitalState with geodetic coordinates filled
 */
OrbitalState ECEFToGeodetic(const Position3D& position);

/**
 * Convert geodetic coordinates to ECEF position
 * @param latitude Geodetic latitude (degrees)
 * @param longitude Geodetic longitude (degrees)
 * @param altitude Altitude above ellipsoid (km)
 * @return Position in ECEF coordinates
 */
Position3D GeodeticToECEF(double latitude, double longitude, double altitude);

/**
 * Get direction vector in ECEF frame pointing to Earth center (nadir)
 * @param satPosition Satellite position in ECEF
 * @return Normalized vector pointing from satellite toward Earth center
 */
Vec3 GetNadirDirection(const Position3D& satPosition);

/**
 * Get direction vector in ECEF frame pointing away from Earth (zenith)
 * @param satPosition Satellite position in ECEF
 * @return Normalized vector pointing from Earth center through satellite
 */
Vec3 GetZenithDirection(const Position3D& satPosition);

/**
 * Calculate angle between spacecraft pointing direction and nadir
 * @param ecefAttitude Spacecraft attitude in ECEF
 * @param nadirDirection Direction to Earth center
 * @return Angle in radians
 */
double GetNadirPointingAngle(const ECEFAttitude& ecefAttitude, 
                             const Vec3& nadirDirection);

/**
 * Format ECEF attitude for human-readable output
 * @param ecefAttitude Attitude to format
 * @param satPosition Satellite position
 * @return Formatted string with all attitude information
 */
std::string FormatECEFAttitudeOutput(const ECEFAttitude& ecefAttitude,
                                     const Position3D& satPosition);

}  // namespace lost

#endif // ECEF_ATTITUDE_H
