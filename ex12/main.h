#ifndef MAIN_H
#define MAIN_H

float fbmPerlinNoise(float x, float y, float z, int octaves, float persistence, float scale);
void initPermutation();

void lookAt(float m[16], float eyeX, float eyeY, float eyeZ, float centerX, float centerY, float centerZ, float upX, float upY, float upZ);
void perspective(float* m, float fovy, float aspect, float near, float far);

extern const float LVLSEA;
extern const char* fragmentShaderComposite;
extern const char* vertexShaderQuad;

#endif