#include <emscripten.h>
#include <cmath>

extern "C" {

float* vertices = nullptr;
float* normals = nullptr;
unsigned short* indices = nullptr;
int vertexCount = 0;
int indexCount = 0;

EMSCRIPTEN_KEEPALIVE
void generateSphere(float radius, int latBands, int longBands) {
    if(vertices) delete[] vertices;
    if(normals) delete[] normals;
    if(indices) delete[] indices;

    vertexCount = (latBands + 1) * (longBands + 1);
    indexCount = latBands * longBands * 6;

    vertices = new float[vertexCount * 3];
    normals = new float[vertexCount * 3];
    indices = new unsigned short[indexCount];

    int vertIdx = 0;
    for (int lat = 0; lat <= latBands; ++lat) {
        float theta = lat * M_PI / latBands;
        float sinTheta = std::sin(theta);
        float cosTheta = std::cos(theta);

        for (int lon = 0; lon <= longBands; ++lon) {
            float phi = lon * 2 * M_PI / longBands;
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            float x = cosPhi * sinTheta;
            float y = cosTheta;
            float z = sinPhi * sinTheta;

            vertices[vertIdx*3+0] = radius * x;
            vertices[vertIdx*3+1] = radius * y;
            vertices[vertIdx*3+2] = radius * z;

            normals[vertIdx*3+0] = x;
            normals[vertIdx*3+1] = y;
            normals[vertIdx*3+2] = z;

            vertIdx++;
        }
    }

    int idx = 0;
    for(int lat=0; lat < latBands; ++lat) {
        for(int lon=0; lon < longBands; ++lon) {
            int first = (lat * (longBands + 1)) + lon;
            int second = first + longBands + 1;

            indices[idx++] = first;
            indices[idx++] = second;
            indices[idx++] = first + 1;

            indices[idx++] = second;
            indices[idx++] = second + 1;
            indices[idx++] = first + 1;
        }
    }
}

EMSCRIPTEN_KEEPALIVE
float* getVertices() { return vertices; }
EMSCRIPTEN_KEEPALIVE
float* getNormals() { return normals; }
EMSCRIPTEN_KEEPALIVE
unsigned short* getIndices() { return indices; }
EMSCRIPTEN_KEEPALIVE
int getVertexCount() { return vertexCount; }
EMSCRIPTEN_KEEPALIVE
int getIndexCount() { return indexCount; }

}
