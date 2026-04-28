#ifndef QUAD_UTILS_H
#define QUAD_UTILS_H

#include "attitude-utils.hpp"
#include "star-utils.hpp"
#include <set>

namespace lost {

    const int NSIDE = 8; // HEALPix tiling resolution
    const int NPIX = NSIDE * NSIDE;
    const double MAG_THRESHOLD = 6.0; // For BSC catalog
    const double MIN_LEN_SQ = 0.01;
    const double AREA_MIN_THRESHOLD = 0.0001; // Minimum area for a valid quad
    const double MIN_ANGULAR_SEP = 0.5; // degrees
    const double MAX_LEN_SQ = 4.0;
    const float RA_DEC_TOLERANCE = 1e-6;
    const int MAX_QUADS_PER_TILE = 200;
    const int MAX_STARS_PER_TILE = 20; // or your custom limit

    class HealPixTile {
    public:
        int healpixTileID;
        std::vector<CatalogStar> stars;

        // Tile Range
        double ra_min, ra_max;
        double dec_min, dec_max;

        int iBand, jRA; // grid coordinates
        std::vector<int> neighbors; // indices of neighboring tiles

        HealPixTile(int healpixTileID, int nside);
        HealPixTile(
            int healpixTileID, int iBand, int jRA,
            double ra_min, double ra_max, 
            double dec_min, double dec_max,
            std::vector<int> neighbors
        ) :
            healpixTileID(healpixTileID), iBand(iBand), jRA(jRA),
            ra_min(ra_min), ra_max(ra_max), 
            dec_min(dec_min), dec_max(dec_max), neighbors(neighbors) {};
    };

    typedef std::vector<HealPixTile> HealPix;

    std::vector<HealPixTile> getNeighbourTiles(HealPixTile tile, HealPix healpix);
    std::vector<CatalogStar> getStarsForProcessingQuad(HealPixTile tile, HealPix healpix);
    HealPix assignStarsToTiles(Catalog catalog, HealPix healpix);
    HealPix generateHealPixTiles(int nside);

    const HealPix &HealPixRead();

    class CatalogQuad {
    public:
        CatalogQuad() = default;
        CatalogQuad(CatalogStar Point_A, CatalogStar Point_B, CatalogStar Point_C, CatalogStar Point_D);
        CatalogQuad(CatalogStar Point_A, CatalogStar Point_B, CatalogStar Point_C, CatalogStar Point_D, int healpix): healpix(healpix) {
            CatalogQuad(Point_A, Point_B, Point_C, Point_D);
        }

        CatalogStar A, B, C, D;

        /// Approximate horizontal radius of the bright area in pixels.
        Vec2 normalized_C, normalized_D;
        Vec4 quadDescriptor;

        float rotation; // angle between AB and celestial coordinate frame

        float ra_center, dec_center;
        /**
         * A relative measure of magnitude of the star. Larger is brighter.
         * It's impossible to tell the true magnitude of the star from the image, without really good camera calibration. Anyway, this field is not meant to correspond to the usual measurement of magnitude. Instead, it's just some measure of brightness which may be specific to the centroiding algorithm. For example, it might be the total number of bright pixels in the star.
         */
        int magnitude;
        int quadID;
        int healpix;
        float scale;
        // eccentricity?

    };

    class Quad {
    public:
        Quad(Star Point_A, Star Point_B, Star Point_C, Star Point_D);
        
    };

    typedef std::vector<CatalogQuad> CatalogQuads;
    typedef std::vector<Quad> Quads;

    std::vector<CatalogQuad> Star2QuadParse(std::vector<CatalogStar> catalog, int healpixTileID);
    std::vector<CatalogQuad> HealPix2QuadParse(HealPix healpix);
    std::vector<CatalogQuad> removeDuplicateQuads(std::vector<CatalogQuad> quads);

    void SerializeCatalogQuads(SerializeContext *, const CatalogQuads &, bool inclMagnitude, bool inclName);
    // sets magnited and name to whether the catalog in the database contained magnitude and name
    CatalogQuads DeserializeCatalogQuads(DeserializeContext *des, bool *inclMagnitudeReturn, bool *inclNameReturn);
    CatalogQuads::const_iterator FindNamedStar(const CatalogQuads &quads, int name);

}

#endif