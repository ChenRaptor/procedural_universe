#include "Atmosphere.hpp"
#include "IcoSphere.hpp"
#include "KDTree3D.hpp"
#include "stdio.h"

Atmosphere::Atmosphere(unsigned int max_subdivisions, float radius, Color atmosphereColor) :
	_max_subdivisions(max_subdivisions),
    _radius(radius),
    _atmosphereColor(atmosphereColor)
{}

Atmosphere::~Atmosphere() {}

void Atmosphere::generate(unsigned int subdivision) {
    static std::unique_ptr<KDTree3D> kdTreeMax;

    if (subdivision > _max_subdivisions) {
        printf("Atmosphere: Invalid subdivision %u, max is %u\n", subdivision, _max_subdivisions);
        return;
    }

    if (_LODLevels[subdivision].sphereVertices.size() > 0)
        return;

    if (!_LODMaxsolid) {
        printf("Atmosphere: Generating max subdivision solid for LOD %u\n", _max_subdivisions);
        _LODMaxsolid = new IcoSphere();
        _LODMaxsolid->generate(_max_subdivisions);

        // Construire k-d tree sur sommets max subdivision
        std::vector<Vec3> pointsMax;
        pointsMax.reserve(_LODMaxsolid->vertices.size());
        for (const auto& v : _LODMaxsolid->vertices)
            pointsMax.push_back({v.x, v.y, v.z});
        kdTreeMax.reset(new KDTree3D(pointsMax));
        //kdTreeMax.reset(new FastSpatialLookup(pointsMax, 0.05f));

        // Calculer valeurs Perlin max subdivision multi-octave
        _LODMaxVertices.resize(pointsMax.size());
        _LODMaxColors.resize(pointsMax.size());
        _LODLevels.resize(_max_subdivisions + 1);


        for (size_t i = 0; i < _LODMaxsolid->vertices.size(); ++i) {
            const Vec3& v = _LODMaxsolid->vertices[i];

            _LODMaxVertices[i] = _radius * glm::vec3(v.x, v.y, v.z);
            _LODMaxColors[i] = _atmosphereColor;
        }
    }

    IcoSphere *solid = new IcoSphere();
    solid->generate(subdivision);


	size_t vertexCount =  solid->vertices.size();
    size_t indexCount = solid->indices.size();

    _sphereVertices.clear();
    _sphereIndices.clear();
    _sphereVertices.resize(vertexCount * 9);
    _sphereIndices.reserve(indexCount);

    for (size_t i = 0; i < vertexCount; ++i) {
        const Vec3& v = solid->vertices[i];

        size_t nearestIndex = kdTreeMax->nearestNeighbor({v.x, v.y, v.z});

        _sphereVertices[9 * i + 0] = _LODMaxVertices[nearestIndex].x;
        _sphereVertices[9 * i + 1] = _LODMaxVertices[nearestIndex].y;
        _sphereVertices[9 * i + 2] = _LODMaxVertices[nearestIndex].z;

        _sphereVertices[9 * i + 3] = _LODMaxColors[nearestIndex].r;
        _sphereVertices[9 * i + 4] = _LODMaxColors[nearestIndex].g;
        _sphereVertices[9 * i + 5] = _LODMaxColors[nearestIndex].b;

        _sphereVertices[9 * i + 6] = 0.f;
        _sphereVertices[9 * i + 7] = 0.f;
        _sphereVertices[9 * i + 8] = 0.f;
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

void Atmosphere::generateAllLODs() {
    //for (unsigned int i = 0; i <= _max_subdivisions; ++i) {
    //    generate(i);
    //}
    //for (unsigned int i = _max_subdivisions; i > 0; ++i) {
    //    generate(i);
    //}

    generate(4);
    generate(3);
    generate(2);
    generate(1);
}

void Atmosphere::prepare_render() {
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
}

void Atmosphere::setLODSelected(unsigned int lod) {
    if (_LODSelected == lod)
        return;
    if (lod > _max_subdivisions) {
        printf("Atmosphere: Invalid subdivision %u, max is %u\n", lod, _max_subdivisions);
        return;
    }
    if (lod < 3)
        lod = 3;
    _LODSelected = lod;
    prepare_render();
}

void Atmosphere::render() {
    glBindVertexArray(_vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(_LODLevels[_LODSelected].sphereIndices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}