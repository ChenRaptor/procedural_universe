#include "Icosphere.hpp"
#include <cmath>
#include <algorithm>

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
      biomeNoiseScale_(config.biomeNoiseScale)
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

    // Calcul positions déformées + couleurs, normales à zero
    for (size_t i = 0; i < vertexCount; ++i) {
        const Vec3& v = vertices_[i];

        int octaves = 8;
        float persistence = 0.9f;
        float noiseVal = fbmPerlinNoise(v.x, v.y, v.z, octaves, persistence, mountainNoiseScale_);
        float heightOffset = noiseVal * heightAmplitude_;
        float deformedRadius = radius_ + heightOffset;
        if (deformedRadius < lvlSea_) deformedRadius = lvlSea_;

        float posX = deformedRadius * v.x;
        float posY = deformedRadius * v.y;
        float posZ = deformedRadius * v.z;

        int colorOctaves = 3;
        float colorPersistence = 0.6f;
        float colorNoiseScale = 60.f;
        float noiseValColor = fbmPerlinNoise(v.x, v.y, v.z, colorOctaves, colorPersistence, colorNoiseScale);

        float blend = ((0.5f + 0.5f * noiseVal) * 0.7f) + ((0.5f + 0.5f * noiseValColor) * 0.3f);

        sphereVertices_[9 * i + 0] = posX;
        sphereVertices_[9 * i + 1] = posY;
        sphereVertices_[9 * i + 2] = posZ;
        sphereVertices_[9 * i + 3] = blend * 0.5f + 0.7f * 0.5f;
        sphereVertices_[9 * i + 4] = blend * 0.5f + 0.8f * 0.5f;
        sphereVertices_[9 * i + 5] = blend * 0.5f + 0.6f * 0.5f;

        // Initialisation normales à zéro
        sphereVertices_[9 * i + 6] = 0.f;
        sphereVertices_[9 * i + 7] = 0.f;
        sphereVertices_[9 * i + 8] = 0.f;
    }

    // Copie indices
    sphereIndices_.assign(indices_.begin(), indices_.end());

    // Calcul normales par accumulation pour chaque triangle
    std::vector<Vec3> normals(vertexCount, Vec3{0.f, 0.f, 0.f});
    for (size_t i = 0; i < sphereIndices_.size(); i += 3) {
        unsigned int i0 = sphereIndices_[i];
        unsigned int i1 = sphereIndices_[i + 1];
        unsigned int i2 = sphereIndices_[i + 2];

        Vec3 v0{
            sphereVertices_[9 * i0],
            sphereVertices_[9 * i0 + 1],
            sphereVertices_[9 * i0 + 2]
        };
        Vec3 v1{
            sphereVertices_[9 * i1],
            sphereVertices_[9 * i1 + 1],
            sphereVertices_[9 * i1 + 2]
        };
        Vec3 v2{
            sphereVertices_[9 * i2],
            sphereVertices_[9 * i2 + 1],
            sphereVertices_[9 * i2 + 2]
        };

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

    // Normalise normales finales
    for (size_t i = 0; i < vertexCount; ++i) {
        Vec3 n = normals[i].normalize();
        sphereVertices_[9 * i + 6] = n.x;
        sphereVertices_[9 * i + 7] = n.y;
        sphereVertices_[9 * i + 8] = n.z;
    }
}