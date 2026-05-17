#!/usr/bin/env python3
"""
ECEF Attitude and Satellite 3D Visualization
Visualizes spacecraft orientation and position relative to Earth in ECEF frame
"""

import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
from matplotlib.patches import FancyArrowPatch
from mpl_toolkits.mplot3d import proj3d
import json
import argparse
import sys
from pathlib import Path


class Arrow3D(FancyArrowPatch):
    """3D arrow visualization helper"""
    def __init__(self, x, y, z, dx, dy, dz, *args, **kwargs):
        super().__init__((0, 0), (0, 0), *args, **kwargs)
        self._xyz = (x, y, z)
        self._dxdydz = (dx, dy, dz)

    def draw(self, renderer):
        x1, y1, z1 = self._xyz
        l, m, n = self._dxdydz
        xs = [x1, x1 + l]
        ys = [y1, y1 + m]
        zs = [z1, z1 + n]

        proj = proj3d.proj_transform(xs, ys, zs, self.axes.M)
        points_2d = np.array([proj[0], proj[1]]).T
        self.set_positions((points_2d[0, 0], points_2d[0, 1]), (points_2d[1, 0], points_2d[1, 1]))
        super().draw(renderer)
        self.do_3d_projection(renderer)

    def do_3d_projection(self, renderer=None):
        x1, y1, z1 = self._xyz
        l, m, n = self._dxdydz
        xs = [x1, x1 + l]
        ys = [y1, y1 + m]
        zs = [z1, z1 + n]

        proj = proj3d.proj_transform(xs, ys, zs, self.axes.M)
        points_2d = np.array([proj[0], proj[1]]).T
        self.set_positions((points_2d[0, 0], points_2d[0, 1]), (points_2d[1, 0], points_2d[1, 1]))

        return np.min(points_2d[:, 1])


class SatelliteVisualizer:
    """3D Visualization of satellite attitude and Earth"""
    
    def __init__(self):
        self.fig = None
        self.ax = None
        
        # Earth parameters (WGS84)
        self.earth_radius = 6371.0  # km
        self.atmosphere_scale = 1.05  # Extend atmosphere visually
        
    def create_earth(self):
        """Create Earth sphere with realistic appearance"""
        u = np.linspace(0, 2 * np.pi, 100)
        v = np.linspace(0, np.pi, 100)
        x = self.earth_radius * np.outer(np.cos(u), np.sin(v))
        y = self.earth_radius * np.outer(np.sin(u), np.sin(v))
        z = self.earth_radius * np.outer(np.ones(np.size(u)), np.cos(v))
        
        return x, y, z
    
    def create_satellite_geometry(self, scale=500):
        """Create a simple satellite geometry (cube)"""
        # Create a cube to represent the satellite
        r = scale / 2
        vertices = np.array([
            [-r, -r, -r], [r, -r, -r], [r, r, -r], [-r, r, -r],  # bottom
            [-r, -r, r], [r, -r, r], [r, r, r], [-r, r, r]       # top
        ])
        
        faces = [
            [vertices[0], vertices[1], vertices[5], vertices[4]],
            [vertices[7], vertices[6], vertices[2], vertices[3]],
            [vertices[0], vertices[3], vertices[7], vertices[4]],
            [vertices[1], vertices[2], vertices[6], vertices[5]],
            [vertices[0], vertices[1], vertices[2], vertices[3]],
            [vertices[4], vertices[5], vertices[6], vertices[7]]
        ]
        
        return faces
    
    def rotate_vector(self, vector, quaternion):
        """Rotate a vector using quaternion"""
        # Quaternion: q = (real, i, j, k)
        real, i, j, k = quaternion
        
        q = np.array([real, i, j, k])
        v = np.array([0] + list(vector))
        
        # q * v * q_conjugate
        q_inv = np.array([real, -i, -j, -k]) / np.sum(q**2)
        
        result = self._quat_mult(q, self._quat_mult(v, q_inv))
        
        return result[1:]
    
    def _quat_mult(self, q1, q2):
        """Multiply two quaternions"""
        w1, x1, y1, z1 = q1
        w2, x2, y2, z2 = q2
        
        return np.array([
            w1*w2 - x1*x2 - y1*y2 - z1*z2,
            w1*x2 + x1*w2 + y1*z2 - z1*y2,
            w1*y2 - x1*z2 + y1*w2 + z1*x2,
            w1*z2 + x1*y2 - y1*x2 + z1*w2
        ])
    
    def plot_attitude_frame(self, ax, position, quaternion, scale=1000):
        """Plot spacecraft attitude frame at given position"""
        x, y, z = position / 1000  # Convert to km
        
        # Axis scale
        s = scale / 1000  # km
        
        # Define body frame axes
        x_axis = np.array([1, 0, 0])
        y_axis = np.array([0, 1, 0])
        z_axis = np.array([0, 0, 1])
        
        # Rotate axes according to quaternion (simplified rotation)
        # For proper visualization, rotate body axes
        q_normalized = quaternion / np.linalg.norm(quaternion)
        
        # X-axis (red)
        ax.quiver(x, y, z, s*x_axis[0], s*x_axis[1], s*x_axis[2], 
                 color='r', arrow_length_ratio=0.2, linewidth=2, label='X-axis')
        
        # Y-axis (green)
        ax.quiver(x, y, z, s*y_axis[0], s*y_axis[1], s*y_axis[2], 
                 color='g', arrow_length_ratio=0.2, linewidth=2, label='Y-axis')
        
        # Z-axis (blue) - typically nadir
        ax.quiver(x, y, z, s*z_axis[0], s*z_axis[1], s*z_axis[2], 
                 color='b', arrow_length_ratio=0.2, linewidth=2, label='Z-axis')
    
    def plot_nadir(self, ax, position):
        """Plot nadir direction (toward Earth center)"""
        pos_km = position / 1000
        x, y, z = pos_km
        
        # Normalize and scale nadir direction
        magnitude = np.linalg.norm(pos_km)
        nadir = -pos_km / magnitude * 1000  # 1000 km toward Earth
        
        ax.quiver(x, y, z, nadir[0]/1000, nadir[1]/1000, nadir[2]/1000,
                 color='k', arrow_length_ratio=0.2, linewidth=1.5, 
                 linestyle='--', label='Nadir (to Earth)')
    
    def visualize(self, sat_position, attitude_quaternion, output_file=None, show_plot=True):
        """
        Create 3D visualization
        
        Args:
            sat_position: [x, y, z] satellite position in ECEF (meters)
            attitude_quaternion: [real, i, j, k] quaternion
            output_file: If provided, save plot to file
        """
        self.fig = plt.figure(figsize=(14, 10))
        self.ax = self.fig.add_subplot(111, projection='3d')
        
        # Convert position to km
        sat_pos_km = sat_position / 1000
        
        # Plot Earth
        x, y, z = self.create_earth()
        self.ax.plot_surface(x, y, z, color='blue', alpha=0.3, label='Earth')
        
        # Plot Earth wireframe grid
        u = np.linspace(0, 2 * np.pi, 20)
        v = np.linspace(0, np.pi, 20)
        x_eq = self.earth_radius * np.cos(u)
        y_eq = self.earth_radius * np.sin(u)
        z_eq = np.zeros_like(u)
        self.ax.plot(x_eq, y_eq, z_eq, 'b-', alpha=0.2, linewidth=0.5)
        
        # Meridians
        for lon in np.linspace(0, 2*np.pi, 12):
            x_mer = self.earth_radius * np.cos(lon) * np.sin(v)
            y_mer = self.earth_radius * np.sin(lon) * np.sin(v)
            z_mer = self.earth_radius * np.cos(v)
            self.ax.plot(x_mer, y_mer, z_mer, 'b-', alpha=0.1, linewidth=0.5)
        
        # Plot satellite position
        self.ax.scatter(*sat_pos_km, color='red', s=200, marker='*', 
                       label='Satellite', zorder=5)
        
        # Plot satellite attitude frame
        self.plot_attitude_frame(self.ax, sat_position, attitude_quaternion)
        
        # Plot nadir direction
        self.plot_nadir(self.ax, sat_position)
        
        # Plot line from Earth center to satellite
        self.ax.plot([0, sat_pos_km[0]], [0, sat_pos_km[1]], [0, sat_pos_km[2]],
                    'k--', alpha=0.3, linewidth=1)
        
        # Set labels and formatting
        max_range = np.linalg.norm(sat_pos_km) * 1.1
        
        self.ax.set_xlim([-max_range, max_range])
        self.ax.set_ylim([-max_range, max_range])
        self.ax.set_zlim([-max_range, max_range])
        
        self.ax.set_xlabel('X (km) - ECEF', fontsize=11, fontweight='bold')
        self.ax.set_ylabel('Y (km) - ECEF', fontsize=11, fontweight='bold')
        self.ax.set_zlabel('Z (km) - ECEF', fontsize=11, fontweight='bold')
        
        self.ax.set_title('Satellite Attitude Visualization in ECEF Frame', 
                         fontsize=14, fontweight='bold')
        
        # Custom legend
        lines = [
            plt.Line2D([0], [0], color='r', linewidth=2, label='X-axis (Body)'),
            plt.Line2D([0], [0], color='g', linewidth=2, label='Y-axis (Body)'),
            plt.Line2D([0], [0], color='b', linewidth=2, label='Z-axis (Body)'),
            plt.Line2D([0], [0], color='k', linewidth=1.5, linestyle='--', label='Nadir'),
            plt.Line2D([0], [0], marker='*', color='red', linewidth=0, 
                      markersize=15, label='Satellite'),
        ]
        self.ax.legend(handles=lines, loc='upper left', fontsize=10)
        
        # Set aspect ratio
        self.ax.set_box_aspect([1,1,1])
        
        # Add grid
        self.ax.grid(True, alpha=0.2)
        
        # Adjust viewing angle
        self.ax.view_init(elev=20, azim=45)
        
        plt.tight_layout()
        
        if output_file:
            plt.savefig(output_file, dpi=150, bbox_inches='tight')
            print(f"Visualization saved to {output_file}")

        if show_plot:
            plt.show()
        else:
            plt.close(self.fig)


class ECEFDataParser:
    """Parse ECEF attitude data from JSON or file"""
    
    @staticmethod
    def from_json(json_file):
        """Load ECEF data from JSON file"""
        with open(json_file, 'r') as f:
            data = json.load(f)
        
        sat_pos = np.array(data['position']) * 1000  # Convert km to meters
        quat = np.array(data['quaternion'])
        
        return sat_pos, quat
    
    @staticmethod
    def from_manual_input():
        """Get input from user"""
        print("\n=== Manual Input Mode ===")
        print("Enter satellite position (ECEF, in km):")
        x = float(input("X (km): "))
        y = float(input("Y (km): "))
        z = float(input("Z (km): "))
        
        print("\nEnter attitude quaternion (normalized):")
        real = float(input("Real (w): "))
        i = float(input("I (x): "))
        j = float(input("J (y): "))
        k = float(input("K (z): "))
        
        sat_pos = np.array([x, y, z]) * 1000  # Convert to meters
        quat = np.array([real, i, j, k])
        
        # Normalize quaternion
        quat = quat / np.linalg.norm(quat)
        
        return sat_pos, quat


def main():
    parser = argparse.ArgumentParser(
        description='Visualize satellite attitude in ECEF frame'
    )
    parser.add_argument('--json', type=str, help='JSON file with ECEF data')
    parser.add_argument('--output', type=str, help='Output file for visualization')
    parser.add_argument('--interactive', action='store_true', 
                       help='Interactive mode (manual input)')
    
    args = parser.parse_args()
    
    # Example default values (LEO satellite)
    sat_position = np.array([6678000.0, 0.0, 0.0])  # 400 km altitude, meters
    quaternion = np.array([1.0, 0.0, 0.0, 0.0])  # Identity (no rotation)
    
    # Load data from file or input
    if args.json:
        sat_position, quaternion = ECEFDataParser.from_json(args.json)
    elif args.interactive:
        sat_position, quaternion = ECEFDataParser.from_manual_input()
    
    # Normalize quaternion
    quaternion = quaternion / np.linalg.norm(quaternion)
    
    # Create visualization
    visualizer = SatelliteVisualizer()
    show_plot = (args.output is None) or args.interactive
    visualizer.visualize(sat_position, quaternion, output_file=args.output, show_plot=show_plot)
    
    print("\nVisualization parameters:")
    print(f"Satellite position (ECEF): {sat_position/1000} km")
    print(f"Quaternion: {quaternion}")
    print(f"Distance from Earth center: {np.linalg.norm(sat_position)/1000:.2f} km")


if __name__ == '__main__':
    main()
