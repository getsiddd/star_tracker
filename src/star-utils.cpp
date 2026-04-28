#include "star-utils.hpp"

#include <math.h>
#include <assert.h>
#include <algorithm>
#include <set>
#include <iostream>
#include <fstream>
#include <sstream>
#include "serialize-helpers.hpp"

namespace lost {

// brightest star first
bool CatalogStarMagnitudeCompare(const CatalogStar &a, const CatalogStar &b) {
    return a.magnitude < b.magnitude;
}

Catalog NarrowCatalog(const Catalog &catalog, int maxMagnitude, int maxStars, float minSeparation) {
    Catalog result;
    for (int i = 0; i < (int)catalog.size(); i++) {
        if (catalog[i].magnitude <= maxMagnitude) {
            result.push_back(catalog[i]);
        }
    }
    // remove stars that are too close to each other
    std::set<int> tooCloseIndices;
    // filter out stars that are too close together
    // easy enough to n^2 brute force, the catalog isn't that big
    for (int i = 0; i < (int)result.size(); i++) {
        for (int j = i+1; j < (int)result.size(); j++) {
            if (AngleUnit(result[i].spatial, result[j].spatial) < minSeparation) {
                tooCloseIndices.insert(i);
                tooCloseIndices.insert(j);
            }
        }
    }

    // Erase all the stars whose indices are in tooCloseIndices from the result.
    // Loop backwards so indices don't get messed up as we iterate.
    for (auto it = tooCloseIndices.rbegin(); it != tooCloseIndices.rend(); it++) {
        result.erase(result.begin() + *it);
    }

    // and finally limit to n brightest stars
    if (maxStars < (int)result.size()) {
        std::sort(result.begin(), result.end(), CatalogStarMagnitudeCompare);
        result.resize(maxStars);
    }

    return result;
}

/// Read and parse the full catalog from disk. If called multiple times, will re-use the first result.

#ifndef DEFAULT_BSC_PATH
#define DEFAULT_BSC_PATH "conf/bright-star-catalog.tsv"
#endif

/// Parse the bright star catalog from the TSV file on disk.
std::vector<CatalogStar> BscParse(std::string tsvPath) {
    std::vector<CatalogStar> result;
    float raj2000, dej2000;
    int magnitudeHigh, magnitudeLow, name;
    char weird;
    char dot; // To consume the '.' separator

    std::ifstream file(tsvPath);  // Open the file

    if (!file) {
        std::cerr << "Error opening file: " << strerror(errno) << std::endl;
        return result;
    }
    std::string line;
    std::string format = "%lf|%lf|%d|%c|%d.%d";
    
    while (std::getline(file, line)) {  // Read line by line
        std::istringstream iss(line);
        std::string raStr, decStr, nameStr, weirdStr, magHighStr, magLowStr;

        if (std::getline(iss, raStr, '|') &&
            std::getline(iss, decStr, '|') &&
            std::getline(iss, nameStr, '|') &&
            std::getline(iss, weirdStr, '|') &&
            std::getline(iss, magHighStr, '.') &&
            std::getline(iss, magLowStr)) {

            double ra = std::stod(raStr);       // Convert RA to double
            double dec = std::stod(decStr);     // Convert Dec to double
            int id = std::stoi(nameStr);          // Convert ID to int
            char weird = weirdStr.empty() ? ' ' : weirdStr[0];  // Handle empty field
            magnitudeHigh = std::stoi(magHighStr); // Convert magnitude to float
            magnitudeLow = std::stoi(magLowStr); // Convert magnitude to float

            result.push_back(CatalogStar(DegToRad(ra),
                                         DegToRad(dec),
                                         magnitudeHigh * 100 + (magnitudeHigh < 0 ? -magnitudeLow : magnitudeLow),
                                         id));
        }
    }

    file.close();
    std::cout << "Loaded " << result.size() << " stars from the catalog." << std::endl;
    return result;
}


const Catalog &CatalogRead() {
    static bool readYet = false;
    static std::vector<CatalogStar> catalog;

    if (!readYet) {

        readYet = true;
        char *tsvPath = getenv("LOST_BSC_PATH");
        catalog = BscParse(tsvPath ? tsvPath : DEFAULT_BSC_PATH);

        // perform essential narrowing
        // remove all stars with exactly the same position as another, keeping the one with brighter magnitude
        std::sort(catalog.begin(), catalog.end(), [](const CatalogStar &a, const CatalogStar &b) {
            return a.spatial.x < b.spatial.x;
        });
        for (int i = catalog.size()-1; i > 0; i--) {
            if ((catalog[i].spatial - catalog[i-1].spatial).Magnitude() < (5e-5)) { // 70 stars removed at this threshold.
                if (catalog[i].magnitude > catalog[i-1].magnitude) {
                    catalog.erase(catalog.begin() + i);
                } else {
                    catalog.erase(catalog.begin() + i - 1);
                }
            }
        }
    }

    return catalog;
}

/// Return a pointer to the star with the given name, or NULL if not found.
Catalog::const_iterator FindNamedStar(const Catalog &catalog, int name) {
    for (auto it = catalog.cbegin(); it != catalog.cend(); ++it) {
        if (it->name == name) {
            return it;
        }
    }
    return catalog.cend();
}

/**
 * Serialize a CatalogStar into a byte buffer.
 * Use SerializeLengthCatalogStar() to determine how many bytes to allocate in `buffer`
 * @param inclMagnitude Whether to include the magnitude of the star.
 * @param inclName Whether to include the (numerical) name of the star.
 * @param buffer[out] Where the serialized star is stored.
 */
void SerializeCatalogStar(SerializeContext *ser, const CatalogStar &catalogStar, bool inclMagnitude, bool inclName) {
    SerializeVec3(ser, catalogStar.spatial);
    if (inclMagnitude) {
        SerializePrimitive<float>(ser, catalogStar.magnitude);
    }
    if (inclName) {
        // TODO: double check that bools aren't some special bitwise thing in C++
        SerializePrimitive<int16_t>(ser, catalogStar.name);
    }
}

/**
 * Deserialize a catalog star.
 * @warn The `inclMagnitude` and `inclName` parameters must be the same as passed to SerializeCatalogStar()
 * @sa SerializeCatalogStar
 */
CatalogStar DeserializeCatalogStar(DeserializeContext *des, bool inclMagnitude, bool inclName) {
    CatalogStar result;
    result.spatial = DeserializeVec3(des);
    if (inclMagnitude) {
        result.magnitude = DeserializePrimitive<float>(des);
    } else {
        result.magnitude = -424242; // TODO, what to do about special values, since there's no good ones for ints.
    }
    if (inclName) {
        result.name = DeserializePrimitive<int16_t>(des);
    } else {
        result.name = -1;
    }
    return result;
}

/**
 * Serialize the catalog to `buffer`.
 * Use SerializeLengthCatalog() to determine how many bytes to allocate in `buffer`
 * @param inclMagnitude,inclName See SerializeCatalogStar()
 */
void SerializeCatalog(SerializeContext *ser, const Catalog &catalog, bool inclMagnitude, bool inclName) {
    SerializePrimitive<int16_t>(ser, catalog.size());

    // flags
    int8_t flags = (inclMagnitude) | (inclName << 1);
    SerializePrimitive<int8_t>(ser, flags);

    for (const CatalogStar &catalogStar : catalog) {
        SerializeCatalogStar(ser, catalogStar, inclMagnitude, inclName);
    }
}

/**
 * Deserialize a catalog.
 * @param[out] inclMagnitudeReturn,inclNameReturn Will store whether `inclMagnitude` and `inclNameReturn` were set in the corresponding SerializeCatalog() call.
 */
Catalog DeserializeCatalog(DeserializeContext *des, bool *inclMagnitudeReturn, bool *inclNameReturn) {
    bool inclName, inclMagnitude;
    Catalog result;

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
        result.push_back(DeserializeCatalogStar(des, inclMagnitude, inclName));
    }

    return result;
}

float MagToBrightness(int mag) {
    return std::pow(10.0, -mag/(250.0));
}

}