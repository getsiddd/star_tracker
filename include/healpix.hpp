// healpix.h
#ifndef HEALPIX_H
#define HEALPIX_H

#include <vector>
#include <cmath>
#include <utility>

struct Star {
    double ra;      // Right Ascension in degrees
    double dec;     // Declination in degrees
    double mag;     // Magnitude
};

struct Tile {
    int id;
    std::vector<Star> stars;
};

class HealpixTiler {
public:
    HealpixTiler(int nside);
    int ang2pix(double ra, double dec) const;
    void assignStarsToTiles(const std::vector<Star>& stars);
    const std::vector<Tile>& getTiles() const;

private:
    int nside;
    int npix;
    std::vector<Tile> tiles;
};

#endif // HEALPIX_H