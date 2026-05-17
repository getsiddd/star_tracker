#!/usr/bin/env python3
"""
Utility to generate synthetic ECEF attitude data for visualization testing
and to demonstrate the ECEF attitude pipeline
"""

import json
import numpy as np
from datetime import datetime
import argparse


class ECEFAttitudeGenerator:
    """Generate synthetic ECEF attitude data"""
    
    def __init__(self, year=2026, month=5, day=17, hour=12, minute=0, second=0):
        self.year = year
        self.month = month
        self.day = day
        self.hour = hour
        self.minute = minute
        self.second = second
    
    def calculate_gmst(self):
        """Calculate Greenwich Mean Sidereal Time"""
        from datetime import datetime
        dt = datetime(self.year, self.month, self.day, 
                     self.hour, self.minute, int(self.second))
        
        # Simplified GMST calculation
        # This is a basic approximation
        year_fraction = (self.month - 1 + (self.day - 1) / 31.0) / 12.0
        gmst = (18.697374558 + 24.06570982441908 * 
               (datetime.utcnow().toordinal() - 2451545.0)) % 24
        
        # Convert to radians
        gmst_rad = (gmst / 24.0) * 2 * np.pi
        
        return gmst_rad
    
    def quaternion_to_dcm(self, q):
        """Convert quaternion to Direction Cosine Matrix"""
        real, i, j, k = q
        
        dcm = np.array([
            [1 - 2*(j**2 + k**2), 2*(i*j - k*real), 2*(i*k + j*real)],
            [2*(i*j + k*real), 1 - 2*(i**2 + k**2), 2*(j*k - i*real)],
            [2*(i*k - j*real), 2*(j*k + i*real), 1 - 2*(i**2 + j**2)]
        ])
        
        return dcm
    
    def generate_leo_orbit(self, altitude_km=400, inclination_deg=51.6, 
                          omega=0, nu=0):
        """
        Generate LEO satellite position
        
        Args:
            altitude_km: Altitude in km
            inclination_deg: Orbital inclination in degrees
            omega: Argument of perigee in radians
            nu: True anomaly in radians
        """
        earth_radius = 6371.0
        a = earth_radius + altitude_km  # Semi-major axis
        
        # Convert to radians
        inc_rad = np.radians(inclination_deg)
        
        # Perifocal coordinates (assuming circular orbit, e=0)
        r = a
        x_peri = r * np.cos(nu)
        y_peri = r * np.sin(nu)
        z_peri = 0
        
        # Rotate to ECI frame
        # Rotation: R_omega * R_inc * R_Omega (simplified for circular orbit)
        cos_inc = np.cos(inc_rad)
        sin_inc = np.sin(inc_rad)
        
        # Rotation matrix for inclination
        x_eci = x_peri
        y_eci = y_peri * cos_inc
        z_eci = y_peri * sin_inc
        
        # Add GMST rotation to get ECEF (simplified)
        gmst = self.calculate_gmst()
        cos_gmst = np.cos(gmst)
        sin_gmst = np.sin(gmst)
        
        x_ecef = x_eci * cos_gmst - y_eci * sin_gmst
        y_ecef = x_eci * sin_gmst + y_eci * cos_gmst
        z_ecef = z_eci
        
        return np.array([x_ecef, y_ecef, z_ecef]) * 1000  # Convert to meters
    
    def generate_nadir_attitude(self, position):
        """Generate quaternion for spacecraft with Z-axis pointing to nadir"""
        # Normalize position to get nadir direction
        pos_norm = position / np.linalg.norm(position)
        
        # For nadir-pointing satellite, we want z-axis toward Earth (negative position)
        # This is a simplified attitude - identity quaternion
        # In reality, you'd compute proper rotation matrix
        
        return np.array([1.0, 0.0, 0.0, 0.0])  # Identity (can be refined)
    
    def generate_dataset(self, count=1):
        """Generate multiple synthetic datasets"""
        datasets = []
        
        for i in range(count):
            # Vary orbital position
            nu = (i / max(count, 1)) * 2 * np.pi  # True anomaly
            
            position = self.generate_leo_orbit(nu=nu)
            quaternion = self.generate_nadir_attitude(position)
            
            # Normalize quaternion
            quaternion = quaternion / np.linalg.norm(quaternion)
            
            datasets.append({
                'position': (position / 1000).tolist(),  # Back to km
                'quaternion': quaternion.tolist(),
                'timestamp': f"{self.year}-{self.month:02d}-{self.day:02d}T{self.hour:02d}:{self.minute:02d}:{self.second:05.2f}Z",
                'gmst_rad': float(self.calculate_gmst()),
                'orbital_state': {
                    'altitude_km': 400,
                    'inclination_deg': 51.6,
                    'true_anomaly_rad': float(nu)
                }
            })
        
        return datasets


def create_sample_data():
    """Create sample ECEF data file"""
    generator = ECEFAttitudeGenerator(2026, 5, 17, 12, 0, 0)
    datasets = generator.generate_dataset(count=1)
    
    return datasets[0]


def main():
    parser = argparse.ArgumentParser(
        description='Generate synthetic ECEF attitude data'
    )
    parser.add_argument('--output', type=str, default='ecef_sample.json',
                       help='Output JSON file')
    parser.add_argument('--count', type=int, default=1,
                       help='Number of samples to generate')
    parser.add_argument('--altitude', type=float, default=400,
                       help='Orbital altitude in km')
    parser.add_argument('--inclination', type=float, default=51.6,
                       help='Orbital inclination in degrees')
    
    args = parser.parse_args()
    
    print(f"Generating {args.count} ECEF attitude sample(s)...")
    
    generator = ECEFAttitudeGenerator(2026, 5, 17, 12, 0, 0)
    datasets = generator.generate_dataset(count=args.count)
    
    # Write to file
    with open(args.output, 'w') as f:
        if args.count == 1:
            json.dump(datasets[0], f, indent=2)
        else:
            json.dump({'samples': datasets}, f, indent=2)
    
    print(f"✓ Saved to {args.output}")
    print(f"\nSample data:")
    print(f"  Position: {datasets[0]['position']} km")
    print(f"  Quaternion: {datasets[0]['quaternion']}")
    print(f"  Timestamp: {datasets[0]['timestamp']}")
    print(f"  GMST: {datasets[0]['gmst_rad']:.6f} rad")


if __name__ == '__main__':
    main()
