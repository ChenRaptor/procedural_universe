#include "Icosphere.hpp"
#include <cmath>
#include <algorithm>
#include "color.h"
#include <stdio.h>

// Opérateurs Vec3
Vec3 Vec3::operator+(const Vec3& b) const { return Vec3{x + b.x, y + b.y, z + b.z}; }
Vec3 Vec3::operator-(const Vec3& b) const { return Vec3{x - b.x, y - b.y, z - b.z}; }
Vec3 Vec3::operator*(float f) const { return Vec3{x * f, y * f, z * f}; }
Vec3 Vec3::normalize() const {
    float len = std::sqrt(x*x + y*y + z*z);
    return (len > 1e-5f) ? Vec3{x / len, y / len, z / len} : *this;
}

IcoSphere::IcoSphere(int subdivisions, float radius, float heightAmplitude, float lvlSea, float mountainNoiseScale)
    : subdivisions_(subdivisions), radius_(radius), heightAmplitude_(heightAmplitude), lvlSea_(lvlSea), mountainNoiseScale_(mountainNoiseScale)
{}

IcoSphere::IcoSphere(const IcoSphereConfig& config)
    : subdivisions_(config.subdivisions),
      radius_(config.radius),
      lvlSea_(config.lvlSea),
      continentOctaves_(config.continentOctaves),
      continentPersistence_(config.continentPersistence),
      continentNoiseScale_(config.continentNoiseScale),
      mountainOctaves_(config.mountainOctaves),
      mountainPersistence_(config.mountainPersistence),
      mountainNoiseScale_(config.mountainNoiseScale),
      heightAmplitude_(config.heightAmplitude),
      biomeOctaves_(config.biomeOctaves),
      biomePersistence_(config.biomePersistence),
      biomeNoiseScale_(config.biomeNoiseScale),
	  bigMountainOctaves_(config.bigMountainOctaves),
	  bigMountainPersistence_(config.bigMountainPersistence),
	  bigMountainNoiseScale_(config.bigMountainNoiseScale),


	  biomePalette_(config.biomePalette),
	  mountainPalette_(config.mountainPalette),
	  bigMoutainPalette_(config.bigMountainPalette),
      oceanPalette_(config.oceanPalette),
        desertPalette_(config.desertPalette),
        forestPalette_(config.forestPalette),
        tundraPalette_(config.tundraPalette),
        snowPalette_(config.snowPalette),
        mountainColors_(config.mountainColors)

{
    // Initialise autres membres au besoin
}
IcoSphere::~IcoSphere() {
    // Clear vectors (optionnel : destructeur automatiquement fait ça)
    vertices_.clear();
    indices_.clear();
    middlePointCache_.clear();
    sphereVertices_.clear();
    sphereIndices_.clear();
}

unsigned int IcoSphere::getMiddlePoint(unsigned int p1, unsigned int p2) {
    uint64_t key = (static_cast<uint64_t>(std::min(p1, p2)) << 32) | std::max(p1, p2);
    auto it = middlePointCache_.find(key);
    if (it != middlePointCache_.end())
        return it->second;

    Vec3 middle = (vertices_[p1] + vertices_[p2]) * 0.5f;
    middle = middle.normalize();

    vertices_.push_back(middle);
    unsigned int i = static_cast<unsigned int>(vertices_.size() - 1);
    middlePointCache_[key] = i;
    return i;
}

void IcoSphere::initIcosahedron() {
    vertices_.clear();
    indices_.clear();
    middlePointCache_.clear();

    const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;

    Vec3 v[] = {
        {-1,  t,  0}, { 1,  t,  0}, {-1, -t,  0}, { 1, -t,  0},
        { 0, -1,  t}, { 0,  1,  t}, { 0, -1, -t}, { 0,  1, -t},
        { t,  0, -1}, { t,  0,  1}, {-t,  0, -1}, {-t,  0,  1}
    };
    size_t vertCount = sizeof(v) / sizeof(Vec3);
    vertices_.reserve(vertCount);
    for (size_t i = 0; i < vertCount; ++i)
        vertices_.push_back(v[i].normalize());

    unsigned int idx[] = {
        0,11,5, 0,5,1, 0,1,7, 0,7,10, 0,10,11,
        1,5,9, 5,11,4, 11,10,2, 10,7,6, 7,1,8,
        3,9,4, 3,4,2, 3,2,6, 3,6,8, 3,8,9,
        4,9,5, 2,4,11, 6,2,10, 8,6,7, 9,8,1
    };
    size_t idxCount = sizeof(idx) / sizeof(unsigned int);
    indices_.reserve(idxCount);
    indices_.assign(idx, idx + idxCount);
}

void IcoSphere::subdivideTriangles() {
    std::vector<unsigned int> newIndices;
    newIndices.reserve(indices_.size() * 4);

    for (size_t i = 0; i < indices_.size(); i += 3) {
        unsigned int v1 = indices_[i];
        unsigned int v2 = indices_[i + 1];
        unsigned int v3 = indices_[i + 2];

        unsigned int a = getMiddlePoint(v1, v2);
        unsigned int b = getMiddlePoint(v2, v3);
        unsigned int c = getMiddlePoint(v3, v1);

        newIndices.push_back(v1);
        newIndices.push_back(a);
        newIndices.push_back(c);

        newIndices.push_back(v2);
        newIndices.push_back(b);
        newIndices.push_back(a);

        newIndices.push_back(v3);
        newIndices.push_back(c);
        newIndices.push_back(b);

        newIndices.push_back(a);
        newIndices.push_back(b);
        newIndices.push_back(c);
    }

    indices_ = std::move(newIndices);
}

extern float fbmPerlinNoise(float x, float y, float z, int octaves, float persistence, float scale);

// Fonction smoothstep classique pour les interpolations lisses
float smoothstep(float edge0, float edge1, float x) {
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}

enum BiomeType {
    OCEAN = 0,
    DESERT,
    FOREST,
    TUNDRA,
    MOUNTAIN,
    SNOW,
    // Ajoute autant de biomes que tu souhaites
};

int getBiomeIndex(float temperature, float humidity, float altitude, float seaLevel) {
    if (altitude < seaLevel) return OCEAN;
    if (temperature > 0.7f) {
        if (humidity < 0.3f) return DESERT;
        else return FOREST;
    } else if (temperature > 0.3f) {
        if (humidity < 0.3f) return TUNDRA; // steppe simplifiée ici
        else return FOREST;
    } else {
        if (humidity < 0.3f) return TUNDRA;
        else return SNOW;
    }
}

float computeTemperature(float latitude, float altitude, const Vec3 &v) {
    // Base temperature based on latitude and altitude
    float baseTemp = 1.0f - std::abs(latitude - 0.5f)*2.0f - altitude * 0.7f;
    baseTemp = std::clamp(baseTemp, 0.0f, 1.0f);

    // Ajout bruit FBM multi-échelle (exemple)
    float tempNoise1 = fbmPerlinNoise(v.x, v.y, v.z, 4, 0.9f, 2.0f);
    float tempNoise2 = fbmPerlinNoise(v.x, v.y, v.z, 4, 0.9f, 20.0f);
    float temperature = baseTemp + 0.3f * tempNoise1 + 0.15f * tempNoise2;

    return temperature; // Valeur de température calculée
}

float computeHumidity(const Vec3 &v) {
    // Humidity based on latitude and altitude
    float humidityNoise1 = fbmPerlinNoise(v.x + 100, v.y + 100, v.z + 100, 4, 0.5f, 2.0f);
    float humidityNoise2 = fbmPerlinNoise(v.x + 200, v.y + 200, v.z + 200, 4, 0.6f, 20.0f);
    float humidity = 0.7f * humidityNoise1 + 0.3f * humidityNoise2;
    humidity = (humidity + 1.0f) * 0.5f * 0.70f;

    return humidity; // Valeur d'humidité calculée
}

void IcoSphere::generate() {
    initIcosahedron();
    for (int i = 0; i < subdivisions_; ++i)
        subdivideTriangles();

    size_t vertexCount = vertices_.size();
    size_t indexCount = indices_.size();

    sphereVertices_.clear();
    sphereIndices_.clear();
    sphereVertices_.resize(vertexCount * 9); // pos(3), color(3), normal(3)
    sphereIndices_.reserve(indexCount);

    for (size_t i = 0; i < vertexCount; ++i) {
        const Vec3& v = vertices_[i];

        // Bruits pour altitude, montagnes, grandes montagnes, biomes
        float ContinentNoise = fbmPerlinNoise(v.x, v.y, v.z, continentOctaves_, continentPersistence_, continentNoiseScale_);
        float bigMountainNoise = fbmPerlinNoise(v.x, v.y, v.z, bigMountainOctaves_, bigMountainPersistence_, bigMountainNoiseScale_);
        float mountainNoise = fbmPerlinNoise(v.x, v.y, v.z, mountainOctaves_, mountainPersistence_, mountainNoiseScale_);
        float BiomeNoise = fbmPerlinNoise(v.x, v.y, v.z, biomeOctaves_, biomePersistence_, biomeNoiseScale_);

        // Lissage poids continent
        const float threshold = 0.1f;
        float weightContinent = smoothstep(0.0f, threshold, ContinentNoise);
        float weightBigMountain = smoothstep(0.0f, 0.2f, bigMountainNoise);

        // Altitude déformée
        float deformedRadius = radius_ + (((mountainNoise * bigMountainNoise * 0.6f) + (ContinentNoise * 0.4f)) * heightAmplitude_);
        deformedRadius += weightBigMountain * weightContinent * bigMountainNoise * heightAmplitude_ / 4;
        float tempDeformedRadius = deformedRadius;

        if (deformedRadius <= lvlSea_) deformedRadius = lvlSea_;

        sphereVertices_[9 * i + 0] = deformedRadius * v.x;
        sphereVertices_[9 * i + 1] = deformedRadius * v.y;
        sphereVertices_[9 * i + 2] = deformedRadius * v.z;

        sphereVertices_[9 * i + 6] = 0.f;
        sphereVertices_[9 * i + 7] = 0.f;
        sphereVertices_[9 * i + 8] = 0.f;

        const float equatorWidth = 0.0001f;
        float latitude = (std::acos(v.y) / M_PI);
        float distanceToEquator = std::abs(latitude - 0.5f);

        if (deformedRadius == lvlSea_)
        {

            Color oceaneColor = getColorFromNoise(((mountainNoise * bigMountainNoise * 0.6f) + (ContinentNoise * 0.4f)), oceanPalette_);
            //0.998
            //float factor = (lvlSea_ - tempDeformedRadius) * 500.0f;
            //float blueValue = 0.4f - 0.2f * factor;
            //blueValue = std::clamp(blueValue, 0.0f, 1.0f);  // clamp pour sécurité
            float r = oceaneColor.r;
            float g = oceaneColor.g;
            float b = oceaneColor.b;

            if (isShowedEquator_ && distanceToEquator < equatorWidth) {
                // Couleur trait rouge équateur
                sphereVertices_[9 * i + 3] = 1.0f;
                sphereVertices_[9 * i + 4] = 0.0f;
                sphereVertices_[9 * i + 5] = 0.0f;
            } else {
                // Couleur normale calculée (biomeColor par exemple)
                sphereVertices_[9 * i + 3] = r;
                sphereVertices_[9 * i + 4] = g;
                sphereVertices_[9 * i + 5] = b;
            }
            continue;
        }

        float altitudeNormalized = (deformedRadius - radius_) / heightAmplitude_;

        float temperature = computeTemperature(latitude, altitudeNormalized, v);
        float humidity = computeHumidity(v);

        int biomeIdx = getBiomeIndex(temperature, humidity, deformedRadius, lvlSea_);

        const std::vector<ColorPoint>* palette = nullptr;
        switch (biomeIdx) {
            case OCEAN: palette = &oceanPalette_; break;
            case DESERT: palette = &desertPalette_; break;
            case FOREST: palette = &forestPalette_; break;
            case TUNDRA: palette = &tundraPalette_; break;
            case MOUNTAIN: palette = &mountainPalette_; break;
            case SNOW: palette = &snowPalette_; break;
            default: palette = &forestPalette_; break;
        }

        //Color biomeColor = getColorFromNoise(BiomeNoise, *palette);

        //float r = biomeColor.r;
        //float g = biomeColor.g;
        //float b = biomeColor.b;


        // facteur entre 0 et 1 selon l'altitude (0 = niveau mer, 1 = sommet haut)
        //float factor = std::clamp((altitudeNormalized - lvlSea_) / (1.0f - lvlSea_), 0.0f, 1.0f);
        //float maxAlt = radius_ + heightAmplitude_;
        //float minAlt = radius_ - heightAmplitude_;

        //float factor = tempDeformedRadius - radius_ / heightAmplitude_;
        //float factor = (mountainNoise * bigMountainNoise * 0.6f + ContinentNoise * 0.4f);



        //// tu peux assombrir ou éclaircir ta couleur de biome
        Color biomeColor = getColorFromNoise(BiomeNoise, *palette);
        //float r = biomeColor.r * 0.5f + 0.5f * factor; // Ajustement de la couleur selon le facteur
        //float g = biomeColor.g * 0.5f + 0.5f * factor;
        //float b = biomeColor.b * 0.5f + 0.5f * factor;


        float factor = (mountainNoise * bigMountainNoise * 1.0f);
        Color mountainColor = getColorFromNoise(factor, mountainColors_);
        // Étaler la distribution des valeurs (tanh, sigmoid, exp, au choix)
        //float factorNonLin = tanhf(8.0f * factor); // pousse plus vite vers +1/-1

        // Utilise ce factor modifié pour la couleur :
        float factorNonLin = tanhf(20.0f * factor);
        float absFactor = std::abs(factorNonLin);

        // Limitez absFactor entre 0 et 1 avec un contrôle doux sur la montée (exponentiel ou puissance)
        //float weight = pow(absFactor, 2.0f); // 1.1f pour une montée un peu plus rapide que linéaire

        // Limite le poids à 1 max
        //if (weight > 1.0f) weight = 1.0f;

        //float r = biomeColor.r * 0.5f + 0.5f * mountainColor.r;
        //float g = biomeColor.g * 0.5f + 0.5f * mountainColor.g;
        //float b = biomeColor.b * 0.5f + 0.5f * mountainColor.b;

        float r = biomeColor.r * (0.5f - absFactor / 2) + mountainColor.r * (0.5f + absFactor / 2);
        float g = biomeColor.g * (0.5f - absFactor / 2) + mountainColor.g * (0.5f + absFactor / 2);
        float b = biomeColor.b * (0.5f - absFactor / 2) + mountainColor.b * (0.5f + absFactor / 2);

        sphereVertices_[9 * i + 3] = r;
        sphereVertices_[9 * i + 4] = g;
        sphereVertices_[9 * i + 5] = b;

        if (isShowedEquator_ && distanceToEquator < equatorWidth) {
            // Couleur trait rouge équateur
            sphereVertices_[9 * i + 3] = 1.0f;
            sphereVertices_[9 * i + 4] = 0.0f;
            sphereVertices_[9 * i + 5] = 0.0f;
        } else {
            // Couleur normale calculée (biomeColor par exemple)
            sphereVertices_[9 * i + 3] = r;
            sphereVertices_[9 * i + 4] = g;
            sphereVertices_[9 * i + 5] = b;
        }
    }


    // Indices
    sphereIndices_.assign(indices_.begin(), indices_.end());

    // Calcul des normales par accumulation
    std::vector<Vec3> normals(vertexCount, Vec3{0.f, 0.f, 0.f});
    for (size_t i = 0; i < sphereIndices_.size(); i += 3) {
        unsigned int i0 = sphereIndices_[i];
        unsigned int i1 = sphereIndices_[i + 1];
        unsigned int i2 = sphereIndices_[i + 2];

        Vec3 v0{sphereVertices_[9 * i0], sphereVertices_[9 * i0 + 1], sphereVertices_[9 * i0 + 2]};
        Vec3 v1{sphereVertices_[9 * i1], sphereVertices_[9 * i1 + 1], sphereVertices_[9 * i1 + 2]};
        Vec3 v2{sphereVertices_[9 * i2], sphereVertices_[9 * i2 + 1], sphereVertices_[9 * i2 + 2]};

        Vec3 edge1 = v1 - v0;
        Vec3 edge2 = v2 - v0;

        Vec3 normal{
            edge1.y * edge2.z - edge1.z * edge2.y,
            edge1.z * edge2.x - edge1.x * edge2.z,
            edge1.x * edge2.y - edge1.y * edge2.x
        };
        normal = normal.normalize();

        normals[i0] = normals[i0] + normal;
        normals[i1] = normals[i1] + normal;
        normals[i2] = normals[i2] + normal;
    }

    for (size_t i = 0; i < vertexCount; ++i) {
        Vec3 n = normals[i].normalize();
        sphereVertices_[9 * i + 6] = n.x;
        sphereVertices_[9 * i + 7] = n.y;
        sphereVertices_[9 * i + 8] = n.z;
    }
}