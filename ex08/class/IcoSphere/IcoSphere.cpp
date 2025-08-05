#include "IcoSphere.hpp"
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

IcoSphere::IcoSphere() {}

IcoSphere::~IcoSphere() {
    vertices.clear();
    indices.clear();
    middlePointCache_.clear();
}

unsigned int IcoSphere::getMiddlePoint(unsigned int p1, unsigned int p2) {
    uint64_t key = (static_cast<uint64_t>(std::min(p1, p2)) << 32) | std::max(p1, p2);
    auto it = middlePointCache_.find(key);
    if (it != middlePointCache_.end())
        return it->second;

    Vec3 middle = (vertices[p1] + vertices[p2]) * 0.5f;
    middle = middle.normalize();

    vertices.push_back(middle);
    unsigned int i = static_cast<unsigned int>(vertices.size() - 1);
    middlePointCache_[key] = i;
    return i;
}

void IcoSphere::initIcosahedron() {
    vertices.clear();
    indices.clear();
    middlePointCache_.clear();

    const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;

    Vec3 v[] = {
        {-1,  t,  0}, { 1,  t,  0}, {-1, -t,  0}, { 1, -t,  0},
        { 0, -1,  t}, { 0,  1,  t}, { 0, -1, -t}, { 0,  1, -t},
        { t,  0, -1}, { t,  0,  1}, {-t,  0, -1}, {-t,  0,  1}
    };
    size_t vertCount = sizeof(v) / sizeof(Vec3);
    vertices.reserve(vertCount);
    for (size_t i = 0; i < vertCount; ++i)
        vertices.push_back(v[i].normalize());

    unsigned int idx[] = {
        0,11,5, 0,5,1, 0,1,7, 0,7,10, 0,10,11,
        1,5,9, 5,11,4, 11,10,2, 10,7,6, 7,1,8,
        3,9,4, 3,4,2, 3,2,6, 3,6,8, 3,8,9,
        4,9,5, 2,4,11, 6,2,10, 8,6,7, 9,8,1
    };
    size_t idxCount = sizeof(idx) / sizeof(unsigned int);
    indices.reserve(idxCount);
    indices.assign(idx, idx + idxCount);
}

void IcoSphere::subdivideTriangles() {
    std::vector<unsigned int> newIndices;
    newIndices.reserve(indices.size() * 4);

    for (size_t i = 0; i < indices.size(); i += 3) {
        unsigned int v1 = indices[i];
        unsigned int v2 = indices[i + 1];
        unsigned int v3 = indices[i + 2];

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

    indices = std::move(newIndices);
}

void IcoSphere::generate(int subdivisions) {
    initIcosahedron();
    for (int i = 0; i < subdivisions; ++i)
        subdivideTriangles();
    this->subdivisions = subdivisions;
}