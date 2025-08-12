#include "Planet.hpp"
#include "IcoSphere.hpp"
#include <cmath>
#include <algorithm>
#include "color.h"
#include <stdio.h>
#include <chrono>
#include "KDTree3D.hpp"
#include <memory>

enum BiomeType {
    OCEAN = 0,
    DESERT,
    FOREST,
    TUNDRA,
    MOUNTAIN,
    SNOW,
    // Ajoute autant de biomes que tu souhaites
};

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
    _debug(config.debug),
    _max_subdivisions(config.max_subdivisions)
{
    biomePalettes_[OCEAN] = &oceanPalette_;
    biomePalettes_[DESERT] = &desertPalette_;
    biomePalettes_[FOREST] = &forestPalette_;
    biomePalettes_[TUNDRA] = &tundraPalette_;
    biomePalettes_[MOUNTAIN] = &mountainPalette_;
    biomePalettes_[SNOW] = &snowPalette_;
}

Planet::~Planet() {}

extern float fbmPerlinNoise(float x, float y, float z, int octaves, float persistence, float scale);


Color interpolateColorInPalette(float t, const std::vector<ColorPoint>& palette) {
    if (palette.empty()) return Color{0, 0, 0};

    // Remap t de [-1,1] en [0,1]
    float normalizedT = (t + 1.0f) / 2.0f;

    // Trouve l'intervalle où normalizedT se situe
    for (size_t i = 1; i < palette.size(); ++i) {
        float prevKey = (palette[i - 1].key + 1.0f) / 2.0f;
        float currKey = (palette[i].key + 1.0f) / 2.0f;

        if (normalizedT < currKey) {
            float localT = (normalizedT - prevKey) / (currKey - prevKey);
            return lerp(palette[i - 1].color, palette[i].color, localT);
        }
    }
    return palette.back().color;
}


constexpr int LUT_SIZE = 256;
std::array<Color, LUT_SIZE> generateColorLUT(const std::vector<ColorPoint>& palette) {
    std::array<Color, LUT_SIZE> lut{};
    for (int i = 0; i < LUT_SIZE; ++i) {
        float t = float(i) / (LUT_SIZE - 1);
        lut[i] = interpolateColorInPalette(t, palette); // fonction pour interpoler selon palette originale
    }
    return lut;
}

Color getColorFromNoiseLUT(float noise, const std::array<Color, LUT_SIZE>& lut) {
    int index = std::clamp(int(noise * (LUT_SIZE - 1)), 0, LUT_SIZE - 1);
    return lut[index];
}



// Fonction smoothstep classique pour les interpolations lisses
float smoothstep(float edge0, float edge1, float x) {
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}

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

void Planet::computeVertices(const Vec3& v, size_t i)
{
    // Compute perlin noise values for the current vertex
    const float ContinentNoise		= fbmPerlinNoise(v.x, v.y, v.z, continentOctaves_, continentPersistence_, continentNoiseScale_);
    const float bigMountainNoise	= fbmPerlinNoise(v.x, v.y, v.z, bigMountainOctaves_, bigMountainPersistence_, bigMountainNoiseScale_);
    const float mountainNoise		= fbmPerlinNoise(v.x, v.y, v.z, mountainOctaves_, mountainPersistence_, mountainNoiseScale_);
    const float BiomeNoise			= fbmPerlinNoise(v.x, v.y, v.z, biomeOctaves_, biomePersistence_, biomeNoiseScale_);

    const float latitude			= std::acos(v.y) / M_PI;
    const float distanceToEquator	= std::abs(latitude - 0.5f);
    const float continentFactor		= (mountainNoise * bigMountainNoise * 0.6f) + (ContinentNoise * 0.4f);
    const float weightContinent   	= smoothstep(0.0f, 0.1f, ContinentNoise);
    const float weightBigMountain 	= smoothstep(0.0f, 0.2f, bigMountainNoise);

    // Calcul radius déformé
    float deformedRadius = radius_ + (continentFactor * heightAmplitude_);
    deformedRadius += weightBigMountain * weightContinent * bigMountainNoise * heightAmplitude_ / 4.0f;

    //const bool onEquator			= isShowedEquator_ && distanceToEquator < 0.0001f;
    const bool underWater			= (deformedRadius <= lvlSea_);

    if (underWater) deformedRadius = lvlSea_;

    _LODMaxVertices[i] = deformedRadius * glm::vec3(v.x, v.y, v.z);

    // --- COULEUR ---
    if (underWater) 
    {
		// Equator handler
        //_LODMaxColors[i] = onEquator ? Color{1.0f,0.0f,0.0f} : getColorFromNoise(continentFactor, oceanPalette_);
		_LODMaxColors[i] = getColorFromNoise(continentFactor, oceanPalette_);
        return;
    }

    // Continue ici pour la terre ferme
    const float altitudeNormalized = (deformedRadius - radius_) / heightAmplitude_;
    const float temperature = computeTemperature(latitude, altitudeNormalized, v);
    const float humidity    = computeHumidity(v);

    int biomeIdx = getBiomeIndex(temperature, humidity, deformedRadius, lvlSea_);
    Color biomeColor = getColorFromNoise(BiomeNoise, *biomePalettes_[biomeIdx]);
    float factor = (mountainNoise * bigMountainNoise * 1.0f);
    Color mountainColor = getColorFromNoise(factor, mountainColors_);
    float absFactor = std::abs(tanhf(20.0f * factor));
    float invMix = 0.5f - absFactor / 2;
    float mix    = 0.5f + absFactor / 2;
    Color finalColor{
        biomeColor.r * invMix + mountainColor.r * mix,
        biomeColor.g * invMix + mountainColor.g * mix,
        biomeColor.b * invMix + mountainColor.b * mix
    };

	// Equator handler
    //_LODMaxColors[i] = onEquator ? Color{1.0f,0.0f,0.0f} : finalColor;
	_LODMaxColors[i] = finalColor;
}


void Planet::generate(unsigned int subdivision) {
    static std::unique_ptr<KDTree3D> kdTreeMax;

    if (subdivision > _max_subdivisions) {
        printf("Planet: Invalid subdivision %u, max is %u\n", subdivision, _max_subdivisions);
        return;
    }

    if (_LODLevels.size() <= subdivision) {
        printf("Planet: Resizing _LODLevels from %zu to %u\n", _LODLevels.size(), subdivision + 1);
        _LODLevels.resize(_max_subdivisions + 1);
    }

    if (_LODLevels[subdivision].sphereVertices.size() > 0)
        return;

    if (!_LODMaxsolid) {
        printf("Planet: Generating max subdivision solid for LOD %u\n", _max_subdivisions);
        _LODMaxsolid = new IcoSphere();
        _LODMaxsolid->generate(_max_subdivisions);

        // Construire k-d tree sur sommets max subdivision
        std::vector<Vec3> pointsMax;
        pointsMax.reserve(_LODMaxsolid->vertices.size());
        for (const auto& v : _LODMaxsolid->vertices)
            pointsMax.push_back({v.x, v.y, v.z});
        auto kdStart = std::chrono::high_resolution_clock::now();
        kdTreeMax.reset(new KDTree3D(pointsMax));
        auto kdEnd = std::chrono::high_resolution_clock::now();


        printf("KDTree construction: %lld ms\n", 
               std::chrono::duration_cast<std::chrono::milliseconds>(kdEnd - kdStart).count());

        // Calculer valeurs Perlin max subdivision multi-octave
        _LODMaxVertices.resize(pointsMax.size());
        _LODMaxColors.resize(pointsMax.size());
        //_LODLevels.resize(_max_subdivisions + 1);

        for (size_t i = 0; i < _LODMaxsolid->vertices.size(); ++i) {
            const Vec3& v = _LODMaxsolid->vertices[i];
            computeVertices(v, i);
        }

        //_LUTocean = generateColorLUT(oceanPalette_);
    }

    IcoSphere *solid;
    if (subdivision == _max_subdivisions)
    {
        solid = _LODMaxsolid;
        printf("Planet: Using precomputed max subdivision solid for LOD %u\n", _max_subdivisions);
    }
    else
    {
        solid = new IcoSphere();
        solid->generate(subdivision);
    }

    size_t vertexCount = solid->vertices.size();
    size_t indexCount = solid->indices.size();
	const std::vector<Vec3>& vertices = solid->vertices;

    _sphereVertices.clear();
    _sphereIndices.clear();
    _sphereVertices.resize(vertexCount * 9);
    _sphereIndices.reserve(indexCount);

    for (size_t i = 0; i < vertexCount; ++i) {
        const Vec3& v = vertices[i];

        size_t nearestIndex = kdTreeMax->nearestNeighbor({v.x, v.y, v.z});
		const glm::vec<3, float> nearestVertex = _LODMaxVertices[nearestIndex];
		const Color& nearestColor = _LODMaxColors[nearestIndex];

        _sphereVertices[9 * i + 0] = nearestVertex.x;
        _sphereVertices[9 * i + 1] = nearestVertex.y;
        _sphereVertices[9 * i + 2] = nearestVertex.z;

        _sphereVertices[9 * i + 3] = nearestColor.r;
        _sphereVertices[9 * i + 4] = nearestColor.g;
        _sphereVertices[9 * i + 5] = nearestColor.b;
    }

    // Indices
    _sphereIndices.assign(solid->indices.begin(), solid->indices.end());

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

    _LODLevels[subdivision].sphereVertices = _sphereVertices;
    _LODLevels[subdivision].sphereIndices = _sphereIndices;
}

void Planet::generateAtmosphere() {
    _atmosphere = new Atmosphere(_max_subdivisions - 5, radius_ * 1.019f, HEX(0xCCD0D2)); // Couleur bleu ciel
    _atmosphere->generateAllLODs();
}

Planet& Planet::generateAllLODs() {
    auto start = std::chrono::high_resolution_clock::now();

    generate(9);
    generate(8);
    generate(7);
    generate(6);
    generate(5);
    generate(4);
    generate(3);
    generate(2);
    generate(1);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    printf("Planet: generateAllLODs took %lld ms\n", duration);

    return *this;
}



void Planet::prepare_render() {
    if (_vao != 0) {
        glDeleteVertexArrays(1, &_vao);
        glDeleteBuffers(1, &_vbo);
        glDeleteBuffers(1, &_ebo);
    }

    glGenVertexArrays(1, &_vao);
    glGenBuffers(1, &_vbo);
    glGenBuffers(1, &_ebo);

    glBindVertexArray(_vao);

    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, _LODLevels[_LODSelected].sphereVertices.size() * sizeof(float), _LODLevels[_LODSelected].sphereVertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, _LODLevels[_LODSelected].sphereIndices.size() * sizeof(unsigned int), _LODLevels[_LODSelected].sphereIndices.data(), GL_STATIC_DRAW);

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

    if (_atmosphere) {
        _atmosphere->setLODSelected(_LODSelected - 5);
        _atmosphere->prepare_render();
    }
}

void Planet::setLODSelected(unsigned int lod) {
    if (_LODSelected == lod)
        return;
    if (lod >= 10) {
        printf("Planet: Invalid LOD selected: %u, max is %d\n", lod, 9);
        return;
    }
    if (lod < 4)
        lod = 4;
    printf("Planet: Setting LOD selected to %u\n", lod);
    _LODSelected = lod;
    prepare_render();
}

void Planet::render() {
    glBindVertexArray(_vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(_LODLevels[_LODSelected].sphereIndices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    //_atmosphere->render();
}