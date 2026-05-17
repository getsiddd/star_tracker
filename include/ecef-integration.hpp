#ifndef ECEF_INTEGRATION_H
#define ECEF_INTEGRATION_H

#include <string>
#include "attitude-utils.hpp"
#include "ecef-attitude.hpp"
#include "blind-solve.hpp"

namespace lost {

/**
 * Integration module for converting blind solve results to ECEF attitude
 * and handling visualization/output
 */

/**
 * Convert blind solve result (RA, Dec, Roll) to ECI attitude quaternion
 * 
 * @param raDeg Right Ascension in degrees (0-360)
 * @param decDeg Declination in degrees (-90 to +90)
 * @param rollDeg Roll angle in degrees (0-360)
 * @return Quaternion representing attitude in ECI frame
 */
Quaternion BlindSolveToAttitude(double raDeg, double decDeg, double rollDeg);

/**
 * Process blind solve result and generate ECEF attitude with optional output
 * 
 * @param result Blind solve result with RA/Dec/Roll
 * @param options Options controlling output format and visualization
 * @return Formatted string with ECEF attitude information
 */
std::string ProcessBlindSolveToECEF(const BlindSolveResult &result, const BlindSolveOptions &options);

/**
 * Export ECEF attitude to JSON format for visualization
 * 
 * @param ecefAtt ECEF attitude structure
 * @param satPos Satellite position in ECEF frame
 * @param timestamp ISO8601 timestamp string
 * @param options Options with output path
 * @return true if successful, false otherwise
 */
bool ExportECEFToJSON(const BlindSolveResult &result,
                      const ECEFAttitude &ecefAtt,
                      const Position3D &satPos,
                      double julianDate,
                      double gmst,
                      const std::string &timestamp,
                      const BlindSolveOptions &options);

/**
 * Generate 3D visualization from ECEF attitude
 * 
 * @param ecefAtt ECEF attitude structure  
 * @param satPos Satellite position in ECEF frame
 * @param options Options with visualization output path
 * @return Command to run Python visualization script
 */
std::string GenerateVisualizationCommand(const ECEFAttitude &ecefAtt, const Position3D &satPos,
                                         const BlindSolveOptions &options);

}  // namespace lost

#endif  // ECEF_INTEGRATION_H
