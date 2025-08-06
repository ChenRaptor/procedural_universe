#include "Atmosphere.hpp"
#include "IcoSphere.hpp"

Atmosphere::Atmosphere(unsigned int subdivisions) :
	_subdivisions(subdivisions)
{}

Atmosphere::~Atmosphere() {}

void Atmosphere::generate() {
	IcoSphere* solid = new IcoSphere();
	solid->generate(_subdivisions);
    _wireframeSphere = solid;

	size_t vertexCount =  _wireframeSphere->vertices.size();
    size_t indexCount = _wireframeSphere->indices.size();

    _sphereVertices.clear();
    _sphereIndices.clear();
    _sphereVertices.resize(vertexCount * 9);
    _sphereIndices.reserve(indexCount);

    for (size_t i = 0; i < vertexCount; ++i) {
        const Vec3& v = _wireframeSphere->vertices[i];

        _sphereVertices[9 * i + 0] = 1.2 * v.x;
        _sphereVertices[9 * i + 1] = 1.2 * v.y;
        _sphereVertices[9 * i + 2] = 1.2 * v.z;

        _sphereVertices[9 * i + 3] = 0.5f;
        _sphereVertices[9 * i + 4] = 0.5f;
        _sphereVertices[9 * i + 5] = 0.5f;

        _sphereVertices[9 * i + 6] = 0.f;
        _sphereVertices[9 * i + 7] = 0.f;
        _sphereVertices[9 * i + 8] = 0.f;
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
}

void Atmosphere::prepare_render() {
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
}


void Atmosphere::render() {
    glBindVertexArray(_vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(_sphereIndices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}