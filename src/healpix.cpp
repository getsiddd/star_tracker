// healpix.cpp
#include "healpix.hpp"

HealpixTiler::HealpixTiler(int nside) : nside(nside) {
    npix = 12 * nside * nside; // Total tiles in HEALPix
    tiles.resize(npix);
    for (int i = 0; i < npix; ++i) {
        tiles[i].id = i;
    }
}

int HealpixTiler::ang2pix(double ra, double dec) const {
    // Normalize RA and Dec to unit vector on sphere
    double theta = (90.0 - dec) * M_PI / 180.0;
    double phi = ra * M_PI / 180.0;

    int z = static_cast<int>(nside * std::cos(theta));
    int y = static_cast<int>(nside * std::sin(theta) * std::sin(phi));
    int x = static_cast<int>(nside * std::sin(theta) * std::cos(phi));

    int index = std::abs((x * 73856093 ^ y * 19349663 ^ z * 83492791) % npix);
    return index;
}

void HealpixTiler::assignStarsToTiles(const std::vector<Star>& stars) {
    for (const auto& star : stars) {
        int pix = ang2pix(star.ra, star.dec);
        tiles[pix].stars.push_back(star);
    }
}

const std::vector<Tile>& HealpixTiler::getTiles() const {
    return tiles;
}
