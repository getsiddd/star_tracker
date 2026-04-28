#include "quad-utils.hpp"

#include <math.h>
#include <algorithm>
#include <set>
#include <unordered_set>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h> 

#include <boost/log/trivial.hpp>

namespace lost {
    // Convert RA/Dec to tangent plane projection around ref star
    const double DEG_TO_RAD = M_PI / 180.0;

    HealPixTile::HealPixTile(int healpixTileID, int nside){
        healpixTileID = healpixTileID;

        int row = healpixTileID / nside;
        int col = healpixTileID % nside;

        double ra_per_tile = (2*M_PI) / nside;
        double dec_per_tile = (M_PI) / nside;

        ra_min = col * ra_per_tile;
        ra_max = (col + 1) * ra_per_tile;
        dec_min = row * dec_per_tile - M_PI/2;
        dec_max = (row + 1) * dec_per_tile - M_PI/2;
    }

    std::vector<CatalogStar> getStarsForProcessingQuad(HealPixTile tile, HealPix healpix){
        std::vector<HealPixTile> tiles = getNeighbourTiles(tile, healpix);
        std::vector<CatalogStar> stars;
        for(int i=0;i< tiles.size() && tiles.size() > 0; i++){
            for (int j = 0; j< tiles[i].stars.size() && tiles[i].stars.size() > 0; j++){
                stars.push_back(tiles[i].stars[j]);
            }
        }
        return stars;
    }

    std::vector<HealPixTile> getNeighbourTiles(HealPixTile tile, HealPix healpix){
        std::vector<HealPixTile> tiles;
        tiles.push_back(tile);
        for (int i=0; i < tile.neighbors.size() && tile.neighbors.size() > 0; i++){
            int tile_id = tile.neighbors[i];
            tiles.push_back(healpix[tile_id]);
        }
        return tiles;
    }

    HealPix assignStarsToTiles(Catalog catalog, HealPix healpix) {
        for (int tile_id = 0; tile_id < NPIX; ++tile_id) {
            HealPixTile tile = healpix[tile_id];
            for (int i=0; i < catalog.size(); i++) {
                CatalogStar star = catalog[i];
                bool in_ra_range = (star.ra >= tile.ra_min && star.ra < tile.ra_max);
                bool in_dec_range = (star.dec >= tile.dec_min && star.dec < tile.dec_max);

                if (!(in_ra_range && in_dec_range)) continue;

                // Check if already exists
                bool exists = std::any_of(tile.stars.begin(), tile.stars.end(), [&](const CatalogStar& s) {
                    return s.name == star.name;
                });
                if (exists) continue;

                // Apply star limit
                if (healpix[tile_id].stars.size() < MAX_STARS_PER_TILE) {
                    healpix[tile_id].stars.push_back(star);
                } else {
                    // Optional: Replace faintest if brighter
                    auto worstIt = std::max_element(healpix[tile_id].stars.begin(), healpix[tile_id].stars.end(), [](const CatalogStar& a, const CatalogStar& b) {
                        return a.magnitude > b.magnitude; // higher mag = fainter
                    });
                    if (star.magnitude < worstIt->magnitude) {
                        *worstIt = star;
                    }
                }
            }
        }
        return healpix;
    }

    HealPix generateHealPixTiles(int nside){
        HealPix tiles;
        std::unordered_map<std::string, int> coordToIndex;
        std::set<std::string> seen_keys;

        int numBands = 4 * nside;  // Number of declination bands
        double decStep = M_PI / numBands;

        std::vector<int> raDivsPerBand(numBands);
        int index = 0;

        // First pass: create all tiles
        for (int i = 0; i < numBands; ++i) {
            double dec_min = -M_PI/2 + i * decStep;
            double dec_max = dec_min + decStep;
            int raDiv = static_cast<int>(round(2 * nside * cos((dec_min + dec_max) / 2)));
            raDiv = std::max(1, static_cast<int>(round(2 * nside * cos((dec_min + dec_max) / 2))));
            raDivsPerBand[i] = raDiv;

            for (int j = 0; j < raDiv; ++j) {
                std::string key = std::to_string(i) + "_" + std::to_string(j);
                if (seen_keys.count(key)) {
                    std::cerr << "Duplicate tile detected: " << key << "\n";
                    continue; // Skip duplicate
                }
                seen_keys.insert(key);
                double ra_min = j * (2 * M_PI / raDiv);
                double ra_max = ra_min + (2 * M_PI / raDiv);

                HealPixTile tile{index, i, j, ra_min, ra_max, dec_min, dec_max, {}};
                coordToIndex[key] = index;
                tiles.push_back(tile);
                ++index;
            }
        }

        // Second pass: assign neighbors
        for (auto& tile : tiles) {
            int i = tile.iBand;
            int j = tile.jRA;
            int raDiv = raDivsPerBand[i];

            auto findIndex = [&](int ni, int nj) -> int {
                if (ni < 0 || ni >= numBands) return -1;
                int nRaDiv = raDivsPerBand[ni];
                nj = (nj + nRaDiv) % nRaDiv;  // wrap-around in RA
                std::string key = std::to_string(ni) + "_" + std::to_string(nj);
                if (coordToIndex.count(key)) return coordToIndex[key];
                return -1;
            };

            std::set<int> addedNeighbors; // To track and avoid duplicates

            // 8-way neighbors
            std::vector<std::pair<int, int>> offsets = {
                {-1, -1}, {-1, 0}, {-1, 1},
                {0, -1},           {0, 1},
                {1, -1},  {1, 0},  {1, 1}
            };

            for (auto [di, dj] : offsets) {
                int ni = i + di;
                int nj = j + dj;
                int neighborIdx = findIndex(ni, nj);
                if (neighborIdx != -1 && addedNeighbors.insert(neighborIdx).second) {
                    tile.neighbors.push_back(neighborIdx);
                }
            }
        }
        
        return tiles;
    }

        /// Parse the star catalog to quad catalog from the star catalog.
    std::vector<CatalogQuad> HealPix2QuadParse(HealPix healpix) {
        std::vector<CatalogQuad> quads, q;

        std::cout << "Generating Quads for " << healpix.size() << " tiles." << std::endl;        

        int quad_id = 0;

        for (size_t i = 0; i < healpix.size(); ++i) {
            std::vector<CatalogStar> catalog = getStarsForProcessingQuad(healpix[i], healpix);
            if (catalog.size()){
                q = Star2QuadParse(catalog, healpix[i].healpixTileID);
                quads.insert(quads.end(), q.begin(), q.end());
            }
        }

        return quads;
    }


    Vec2 ProjectToPlane(double refRA, double refDEC, double targetRA, double targetDEC) {
        double dra = (targetRA - refRA);
        double dec0 = refDEC;
        double dec = targetDEC;

        double x = cos(dec) * sin(dra);
        double y = sin(dec) * cos(dec0) - cos(dec) * sin(dec0) * cos(dra);

        Vec2 planeCoordinates;
        planeCoordinates.x = x;
        planeCoordinates.y = y;

        return planeCoordinates;
    }

    const double MIN_LEN_SQ_THRESHOLD = 1e-4;  // Typical threshold to avoid tiny or degenerate quads

    bool isBetween(Vec2 A, Vec2 B, Vec2 P) {
        double dx = B.x - A.x;
        double dy = B.y - A.y;
        double px = P.x - A.x;
        double py = P.y - A.y;
    
        double dot = px * dx + py * dy;
        double len_sq = dx * dx + dy * dy;
        return (dot > 0 && dot < len_sq);
    }

    bool checkQuadArea(Vec2 A, Vec2 B, Vec2 C, Vec2 D, double minAreaThreshold = 0.0001) {
        // Compute area using the shoelace formula for quadrilateral
        double area = 0.5 * std::fabs(
            A.x * B.y + B.x * C.y + C.x * D.y + D.x * A.y
            - A.y * B.x - B.y * C.x - C.y * D.x - D.y * A.x
        );

        return area >= minAreaThreshold;
    }

    bool isColinear(Vec2 A, Vec2 B, Vec2 P) {
        double cross = (B.x - A.x) * (P.y - A.y) - (B.y - A.y) * (P.x - A.x);
        return std::fabs(cross) < 1e-6;
    }

    bool isBaseLength(Vec2 A, Vec2 B) {
        double dx = B.x - A.x;
        double dy = B.y - A.y;
        double len_sq = dx * dx + dy * dy;
        return len_sq <= MAX_LEN_SQ && len_sq > MIN_LEN_SQ;
    }

    bool checkMinimumArea(Vec2 A, Vec2 B, Vec2 C) {
        double area = 0.5 * std::fabs((B.x - A.x)*(C.y - A.y) - (B.y - A.y)*(C.x - A.x));
        return area >= AREA_MIN_THRESHOLD;
    }

    double distance(Vec2 p1, Vec2 p2) {
        double dx = p2.x - p1.x;
        double dy = p2.y - p1.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    double angleBetween(Vec2 a, Vec2 b, Vec2 c) {
        // Returns angle at point b formed by a-b-c
        Vec2 ab = {a.x - b.x, a.y - b.y};
        Vec2 cb = {c.x - b.x, c.y - b.y};
        double dot = ab.x * cb.x + ab.y * cb.y;
        double mag_ab = std::sqrt(ab.x * ab.x + ab.y * ab.y);
        double mag_cb = std::sqrt(cb.x * cb.x + cb.y * cb.y);
        double cosTheta = dot / (mag_ab * mag_cb + 1e-9);
        return std::acos(std::clamp(cosTheta, -1.0, 1.0));  // in radians
    }

    bool isShapeQualityAcceptable(Vec2 A, Vec2 B, Vec2 C, Vec2 D) {
        // Check aspect ratio of AB vs CD
        double lenAB = distance(A, B);
        double lenCD = distance(C, D);
        double aspectRatio = std::max(lenAB, lenCD) / std::min(lenAB, lenCD + 1e-9);
        if (aspectRatio > 5.0) return false;  // too skewed

        // Check internal angles (in degrees)
        double angleA = angleBetween(D, A, B) * (180.0 / M_PI);
        double angleB = angleBetween(A, B, C) * (180.0 / M_PI);
        double angleC = angleBetween(B, C, D) * (180.0 / M_PI);
        double angleD = angleBetween(C, D, A) * (180.0 / M_PI);

        double minAngle = std::min({angleA, angleB, angleC, angleD});
        double maxAngle = std::max({angleA, angleB, angleC, angleD});
        if (minAngle < 10.0) return false;  // too sharp
        if (maxAngle < 170.0) return false;

        return true;
    }

    bool isDistanceRatioConsistent(Vec2 A, Vec2 B, Vec2 C, Vec2 D, double maxRatio = 3.0) {
        double lenAB = distance(A, B);
        double lenBC = distance(B, C);
        double lenCD = distance(C, D);
        double lenDA = distance(D, A);

        double maxLen = std::max({lenAB, lenBC, lenCD, lenDA});
        double minLen = std::min({lenAB, lenBC, lenCD, lenDA});

        return (maxLen / (minLen + 1e-9)) <= maxRatio;
    }

    float computeQuadArea(const CatalogQuad& quad) {
        Vec2 A = {0, 0};
        Vec2 B = ProjectToPlane(quad.A.ra, quad.A.dec, quad.B.ra, quad.B.dec);
        Vec2 C = ProjectToPlane(quad.A.ra, quad.A.dec, quad.C.ra, quad.C.dec);
        Vec2 D = ProjectToPlane(quad.A.ra, quad.A.dec, quad.D.ra, quad.D.dec);

        auto triangle_area = [](Vec2 A, Vec2 B, Vec2 C) {
            return 0.5f * std::fabs((B.x - A.x)*(C.y - A.y) - (B.y - A.y)*(C.x - A.x));
        };

        return triangle_area(A, B, C) + triangle_area(A, B, D);
    }

    float computeAspectRatio(const CatalogQuad& quad) {
        Vec2 A = {0, 0};
        Vec2 B = ProjectToPlane(quad.A.ra, quad.A.dec, quad.B.ra, quad.B.dec);
        Vec2 C = ProjectToPlane(quad.A.ra, quad.A.dec, quad.C.ra, quad.C.dec);
        Vec2 D = ProjectToPlane(quad.A.ra, quad.A.dec, quad.D.ra, quad.D.dec);

        float len_ab = distance(A, B);
        float len_cd = distance(C, D);
        return len_cd > 1e-6f ? len_ab / len_cd : 0.0f;
    }

    float computeQuadScore(const CatalogQuad& quad) {
        float area = computeQuadArea(quad);
        float aspect = computeAspectRatio(quad);
        float symmetryPenalty = std::fabs(quad.normalized_C.x - (1.0f - quad.normalized_D.x));
        return area / (1.0f + symmetryPenalty + std::fabs(aspect - 1.0f));
    }

    bool isConvex(Vec2 A, Vec2 B, Vec2 C, Vec2 D) {
        auto cross = [](Vec2 a, Vec2 b, Vec2 c) {
            return (b.x - a.x)*(c.y - a.y) - (b.y - a.y)*(c.x - a.x);
        };
        bool ab_bc = cross(A, B, C) > 0;
        bool bc_cd = cross(B, C, D) > 0;
        bool cd_da = cross(C, D, A) > 0;
        bool da_ab = cross(D, A, B) > 0;
        return (ab_bc == bc_cd) && (bc_cd == cd_da) && (cd_da == da_ab);
    }

    bool hasValidAngles(const Vec2& A, const Vec2& B, const Vec2& C, const Vec2& D) {
        double angleA = angleBetween(D, A, B) * (180.0 / M_PI);
        double angleB = angleBetween(A, B, C) * (180.0 / M_PI);
        double angleC = angleBetween(B, C, D) * (180.0 / M_PI);
        double angleD = angleBetween(C, D, A) * (180.0 / M_PI);

        double maxAngle = std::max({angleA, angleB, angleC, angleD});
        return maxAngle < 170.0;  // Reject quads with very flat corners
    }

    bool hasSufficientDiagonalSeparation(Vec2 A, Vec2 B, Vec2 C, Vec2 D, double threshold = 1e-5) {
        double diag1 = distance(A, C);  // AC
        double diag2 = distance(B, D);  // BD
        return std::abs(diag1 - diag2) >= threshold;
    }

    bool isCounterClockwise(Vec2 A, Vec2 B, Vec2 C, Vec2 D) {
        double signedArea =
            A.x * B.y + B.x * C.y + C.x * D.y + D.x * A.y -
            A.y * B.x - B.y * C.x - C.y * D.x - D.y * A.x;
        
        return signedArea >= 0;  // true = CCW, false = CW
    }


    // Utility to format doubles with precision
    std::string formatDouble(double value, int precision = 4) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(precision) << value;
        return oss.str();
    }

    CatalogQuad::CatalogQuad(CatalogStar Point_A, CatalogStar Point_B, CatalogStar Point_C, CatalogStar Point_D){
        A = Point_A;
        B = Point_B;
        C = Point_C;
        D = Point_D;

        Vec2 vectorA, vectorB, vectorC, vectorD;

        vectorA.x = vectorA.y = 0;  // center at A
        vectorB = ProjectToPlane(A.ra, A.dec, B.ra, B.dec);
        vectorC = ProjectToPlane(A.ra, A.dec, C.ra, C.dec);
        vectorD = ProjectToPlane(A.ra, A.dec, D.ra, D.dec);

        double dx = vectorB.x - vectorA.x;
        double dy = vectorB.y - vectorA.y;
        double len = std::sqrt(dx * dx + dy * dy);
        
        if (len == 0) {
            quadDescriptor.a = quadDescriptor.b = quadDescriptor.c = quadDescriptor.d = 0;
        }
        else {
            double ux = dx / len;
            double uy = dy / len;
            double vx = -uy;
            double vy = ux;
    
            normalized_C.x = quadDescriptor.a = ((vectorC.x - vectorA.x) * ux + (vectorC.y - vectorA.y) * uy);
            normalized_C.y = quadDescriptor.b = ((vectorC.x - vectorA.x) * vx + (vectorC.y - vectorA.y) * vy);
            normalized_D.x = quadDescriptor.c = ((vectorD.x - vectorA.x) * ux + (vectorD.y - vectorA.y) * uy);
            normalized_C.y = quadDescriptor.d = ((vectorD.x - vectorA.x) * vx + (vectorD.y - vectorA.y) * vy);

            quadDescriptor.a = normalized_C.x;
            quadDescriptor.b = normalized_C.y;
            quadDescriptor.c = normalized_D.x;
            quadDescriptor.d = normalized_D.y;

        }
    }

    /// Parse the star catalog to quad catalog from the star catalog.
    std::vector<CatalogQuad> Star2QuadParse(std::vector<CatalogStar> cat, int healpixTileID) {
        std::vector<CatalogQuad> result;

        int quad_id = 0;

        std::vector<CatalogStar> catalog = cat;

        for (size_t i = 0; i < catalog.size(); ++i) {
            for (size_t j = i+1; j < catalog.size(); ++j) {
                if (i == j) continue; 

                CatalogStar A = catalog[i];
                CatalogStar B = catalog[j];

                Vec2 vectorA, vectorB;
                vectorA.x = vectorA.y = 0;  // center at A
                vectorB = ProjectToPlane(A.ra, A.dec, B.ra, B.dec);

                if (isBaseLength(vectorA, vectorB) == false) continue;
    
                for (size_t k = 0; k < catalog.size(); ++k) {
                    if (k == i || k == j) continue;
                    CatalogStar C = catalog[k];
                    Vec2 vectorC;
                    vectorC = ProjectToPlane(A.ra, A.dec, C.ra, C.dec);

                    if (isColinear(vectorA, vectorB, vectorC) || !isBetween(vectorA, vectorB, vectorC)) continue;
                    if (!checkMinimumArea(vectorA, vectorB, vectorC)) continue;
    
                    for (size_t l = 0; l < catalog.size(); ++l) {
                        if (l == i || l == j || l == k) continue;
                        CatalogStar D = catalog[l];
                        Vec2 vectorD;
                        vectorD = ProjectToPlane(A.ra, A.dec, D.ra, D.dec);
                        
                        if (!checkMinimumArea(vectorA, vectorB, vectorD)) continue;
                        if (isColinear(vectorA, vectorB, vectorD) || !isBetween(vectorA, vectorB, vectorD)) continue;
                        if (!checkQuadArea(vectorA, vectorB, vectorC, vectorD)) continue;
                        if (!isShapeQualityAcceptable(vectorA, vectorB, vectorC, vectorD)) continue;
                        if (!isConvex(vectorA, vectorB, vectorC, vectorD)) continue;
                        if (!isDistanceRatioConsistent(vectorA, vectorB, vectorC, vectorD)) continue;
                        if (!hasSufficientDiagonalSeparation(vectorA, vectorB, vectorC, vectorD)) continue;
                        if (!isCounterClockwise(vectorA, vectorB, vectorC, vectorD)) continue;

                        std::set<int> star_ids = {A.name, B.name, C.name, D.name};
                        if (star_ids.size() < 4) continue;  // Duplicate star found, skip this quad

                        if ((vectorC.x <= vectorD.x) && (vectorC.x + vectorD.x) <= 1){ 
                            result.push_back(CatalogQuad(A, B, C, D, healpixTileID));
                        }
                    }
                }
            }
        }

        std::cout << "Generated " << result.size() << " quads from HealPix Tile: " << healpixTileID << " having " << catalog.size() << " stars." << std::endl;
        // Sort by score and keep best
        std::sort(result.begin(), result.end(), [](const CatalogQuad& a, const CatalogQuad& b) {
            return computeQuadScore(a) > computeQuadScore(b);
        });

        if (result.size() > MAX_QUADS_PER_TILE) result.resize(MAX_QUADS_PER_TILE);
        return result;
    }

    std::vector<CatalogQuad> removeDuplicateQuads(std::vector<CatalogQuad> quads) {
        std::vector<CatalogQuad> uniqueQuads;
        std::set<std::vector<int>> seenQuads;

        for (const auto& quad : quads) {
            std::vector<int> starIds = {quad.A.name, quad.B.name, quad.C.name, quad.D.name};
            std::sort(starIds.begin(), starIds.end());  // Normalized representation

            if (seenQuads.count(starIds) == 0) {
                seenQuads.insert(starIds);
                uniqueQuads.push_back(quad);
            }
        }

        return uniqueQuads;
    }

    /**
     * Serialize a CatalogStar into a byte buffer.
     * Use SerializeLengthCatalogStar() to determine how many bytes to allocate in `buffer`
     * @param inclMagnitude Whether to include the magnitude of the star.
     * @param inclName Whether to include the (numerical) name of the star.
     * @param buffer[out] Where the serialized star is stored.
     */
    void SerializeCatalogQuad(SerializeContext *ser, const CatalogQuad &quad, bool inclMagnitude, bool inclName) {
        SerializeVec2(ser, quad.normalized_C);
        SerializeVec2(ser, quad.normalized_D);
        SerializeVec4(ser, quad.quadDescriptor);
        SerializePrimitive<float>(ser, quad.rotation);
        SerializePrimitive<float>(ser, quad.ra_center);
        SerializePrimitive<float>(ser, quad.dec_center);
        SerializePrimitive<int16_t>(ser, quad.magnitude);
        SerializePrimitive<int16_t>(ser, quad.healpix);
        SerializePrimitive<int16_t>(ser, quad.quadID);
        SerializePrimitive<float>(ser, quad.scale);
    }

    /**
     * Serialize the catalog to `buffer`.
     * Use SerializeLengthCatalog() to determine how many bytes to allocate in `buffer`
     * @param inclMagnitude,inclName See SerializeCatalogStar()
     */
    void SerializeCatalogQuads(SerializeContext *ser, const CatalogQuads &quads, bool inclMagnitude, bool inclName) {
        SerializePrimitive<int16_t>(ser, quads.size());

        // flags 
        int8_t flags = (inclMagnitude) | (inclName << 1);
        SerializePrimitive<int8_t>(ser, flags);

        for (const CatalogQuad &quad : quads) {
            SerializeCatalogQuad(ser, quad, inclMagnitude, inclName);
        }
    }

    /**
     * Deserialize a catalog star.
     * @warn The `inclMagnitude` and `inclName` parameters must be the same as passed to SerializeCatalogStar()
     * @sa SerializeCatalogStar
     */
    CatalogQuad DeserializeCatalogQuad(DeserializeContext *des, bool inclMagnitude, bool inclName) {
        CatalogQuad result;
        result.normalized_C = DeserializeVec2(des);
        result.normalized_D = DeserializeVec2(des);
        result.quadDescriptor = DeserializeVec4(des);
        
        result.rotation = DeserializePrimitive<float>(des);
        result.ra_center = DeserializePrimitive<float>(des);
        result.dec_center = DeserializePrimitive<float>(des);

        result.scale = DeserializePrimitive<float>(des);

        return result;
    }

    /**
     * Deserialize a catalog.
     * @param[out] inclMagnitudeReturn,inclNameReturn Will store whether `inclMagnitude` and `inclNameReturn` were set in the corresponding SerializeCatalog() call.
     */
    CatalogQuads DeserializeCatalogQuads(DeserializeContext *des, bool *inclMagnitudeReturn, bool *inclNameReturn) {
        bool inclName, inclMagnitude;
        CatalogQuads result;

        int16_t numStars = DeserializePrimitive<int16_t>(des);

        int8_t flags = DeserializePrimitive<int8_t>(des);
        inclMagnitude = (flags) & 1;
        inclName = (flags>>1) & 1;

        if (inclMagnitudeReturn != NULL) {
            *inclMagnitudeReturn = inclMagnitude;
        }
        if (inclNameReturn != NULL) {
            *inclNameReturn = inclName;
        }

        for (int i = 0; i < numStars; i++) {
            result.push_back(DeserializeCatalogQuad(des, inclMagnitude, inclName));
        }

        return result;
    }




}


