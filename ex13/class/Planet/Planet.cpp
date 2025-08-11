#include "Planet.hpp"
#include "IcoSphere.hpp"
#include <cmath>
#include <algorithm>
#include "color.h"
#include <stdio.h>
#include <chrono>

Planet::Planet(const PlanetConfig& config) :
	subdivisions_(config.subdivisions),
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
	mountainColors_(config.mountainColors),
    _debug(config.debug)
{}

Planet::~Planet() {}

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

Planet& Planet::generate() {
    std::chrono::high_resolution_clock::time_point start;
    if (_debug)
        start = std::chrono::high_resolution_clock::now();
	_wireframeSphere = new IcoSphere();
	_wireframeSphere->generate(subdivisions_);

    size_t vertexCount =  _wireframeSphere->vertices.size();
    size_t indexCount = _wireframeSphere->indices.size();

    _sphereVertices.clear();
    _sphereIndices.clear();
    _sphereVertices.resize(vertexCount * 9);
    _sphereIndices.reserve(indexCount);

    for (size_t i = 0; i < vertexCount; ++i) {
        const Vec3& v = _wireframeSphere->vertices[i];

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

        _sphereVertices[9 * i + 0] = deformedRadius * v.x;
        _sphereVertices[9 * i + 1] = deformedRadius * v.y;
        _sphereVertices[9 * i + 2] = deformedRadius * v.z;

        _sphereVertices[9 * i + 6] = 0.f;
        _sphereVertices[9 * i + 7] = 0.f;
        _sphereVertices[9 * i + 8] = 0.f;

        const float equatorWidth = 0.0001f;
        float latitude = (std::acos(v.y) / M_PI);
        float distanceToEquator = std::abs(latitude - 0.5f);

        if (deformedRadius == lvlSea_)
        {

            Color oceaneColor = getColorFromNoise(((mountainNoise * bigMountainNoise * 0.6f) + (ContinentNoise * 0.4f)), oceanPalette_);
            float r = oceaneColor.r;
            float g = oceaneColor.g;
            float b = oceaneColor.b;

            if (isShowedEquator_ && distanceToEquator < equatorWidth) {
                // Couleur trait rouge équateur
                _sphereVertices[9 * i + 3] = 1.0f;
                _sphereVertices[9 * i + 4] = 0.0f;
                _sphereVertices[9 * i + 5] = 0.0f;
            } else {
                // Couleur normale calculée (biomeColor par exemple)
                _sphereVertices[9 * i + 3] = r;
                _sphereVertices[9 * i + 4] = g;
                _sphereVertices[9 * i + 5] = b;
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

        Color biomeColor = getColorFromNoise(BiomeNoise, *palette);

        float factor = (mountainNoise * bigMountainNoise * 1.0f);
        Color mountainColor = getColorFromNoise(factor, mountainColors_);
        float factorNonLin = tanhf(20.0f * factor);
        float absFactor = std::abs(factorNonLin);

        float r = biomeColor.r * (0.5f - absFactor / 2) + mountainColor.r * (0.5f + absFactor / 2);
        float g = biomeColor.g * (0.5f - absFactor / 2) + mountainColor.g * (0.5f + absFactor / 2);
        float b = biomeColor.b * (0.5f - absFactor / 2) + mountainColor.b * (0.5f + absFactor / 2);

        _sphereVertices[9 * i + 3] = r;
        _sphereVertices[9 * i + 4] = g;
        _sphereVertices[9 * i + 5] = b;

        if (isShowedEquator_ && distanceToEquator < equatorWidth) {
            // Couleur trait rouge équateur
            _sphereVertices[9 * i + 3] = 1.0f;
            _sphereVertices[9 * i + 4] = 0.0f;
            _sphereVertices[9 * i + 5] = 0.0f;
        } else {
            // Couleur normale calculée (biomeColor par exemple)
            _sphereVertices[9 * i + 3] = r;
            _sphereVertices[9 * i + 4] = g;
            _sphereVertices[9 * i + 5] = b;
        }
    }


    // Indices
    _sphereIndices.assign(_wireframeSphere->indices.begin(), _wireframeSphere->indices.end());

    // Calcul des normales par accumulation
    std::vector<Vec3> normals(vertexCount, Vec3{0.f, 0.f, 0.f});
    for (size_t i = 0; i < _sphereIndices.size(); i += 3) {
        unsigned int i0 = _sphereIndices[i];
        unsigned int i1 = _sphereIndices[i + 1];
        unsigned int i2 = _sphereIndices[i + 2];

        Vec3 v0{_sphereVertices[9 * i0], _sphereVertices[9 * i0 + 1], _sphereVertices[9 * i0 + 2]};
        Vec3 v1{_sphereVertices[9 * i1], _sphereVertices[9 * i1 + 1], _sphereVertices[9 * i1 + 2]};
        Vec3 v2{_sphereVertices[9 * i2], _sphereVertices[9 * i2 + 1], _sphereVertices[9 * i2 + 2]};

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
        _sphereVertices[9 * i + 6] = n.x;
        _sphereVertices[9 * i + 7] = n.y;
        _sphereVertices[9 * i + 8] = n.z;
    }

    _atmosphere = new Atmosphere(subdivisions_ - 5, radius_ * 1.019f, HEX(0xCCD0D2)); // Couleur bleu ciel
    _atmosphere->generateAllLODs();
    if (_debug)
    {
        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> elapsed = end - start;
        printf("Temps de génération de l'icosphère : %.6f secondes\n", elapsed.count());
    }

    return *this;
}

void Planet::prepare_render() {
    if (!_buffersInitialized) {
        // Générer buffers OpenGL
        glGenVertexArrays(1, &_vao);
        glGenBuffers(1, &_vbo);
        glGenBuffers(1, &_ebo);

        glBindVertexArray(_vao);

        glBindBuffer(GL_ARRAY_BUFFER, _vbo);
        glBufferData(GL_ARRAY_BUFFER, _sphereVertices.size() * sizeof(float), _sphereVertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, _sphereIndices.size() * sizeof(unsigned int), _sphereIndices.data(), GL_STATIC_DRAW);

        // Position
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Couleur
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // Normales
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);

        _buffersInitialized = true;
    }
    if (_atmosphere) {
        printf("Planet LOD selected: %u\n", _LODSelected - 5);
        _atmosphere->setLODSelected(_LODSelected - 5);
        _atmosphere->prepare_render();
    }
}

void Planet::setLODSelected(unsigned int lod) {
    if (_LODSelected == lod)
        return;
    //if (lod < 3) {
    //    printf("LOD must be at least 3, got %u\n", lod);
    //    return;
    //}
    if (lod >= 10) {
        printf("Invalid LOD selected: %u, max is %d\n", lod, 9);
        return;
    }
    _LODSelected = lod;
    prepare_render();
}

void Planet::render() {
    glBindVertexArray(_vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(_sphereIndices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    //_atmosphere->render();
}