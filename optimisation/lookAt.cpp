#include <cmath>

void perspective(float* m, float fovy, float aspect, float near, float far) {
    float f = 1.0f / tanf(fovy * 3.14159265f / 360.0f);
    m[0] = f / aspect; m[1] = 0; m[2] = 0;                          m[3] = 0;
    m[4] = 0;          m[5] = f; m[6] = 0;                          m[7] = 0;
    m[8] = 0;          m[9] = 0; m[10] = (far + near) / (near - far); m[11] = -1;
    m[12] = 0;         m[13] = 0; m[14] = (2*far*near) / (near - far); m[15] = 0;
}

void subtract(const float a[3], const float b[3], float out[3]) {
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

void normalize(float v[3]) {
    float len = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (len > 0.00001f) {
        v[0] /= len;
        v[1] /= len;
        v[2] /= len;
    }
}

void cross(const float a[3], const float b[3], float out[3]) {
    out[0] = a[1]*b[2] - a[2]*b[1];
    out[1] = a[2]*b[0] - a[0]*b[2];
    out[2] = a[0]*b[1] - a[1]*b[0];
}

void lookAt(float m[16],
            float eyeX, float eyeY, float eyeZ,
            float centerX, float centerY, float centerZ,
            float upX, float upY, float upZ) {
    float f[3], up[3] = {upX, upY, upZ};
    float s[3], u[3];
    float eye[3] = {eyeX, eyeY, eyeZ};
    float center[3] = {centerX, centerY, centerZ};

    subtract(center, eye, f);
    normalize(f);

    cross(f, up, s);
    normalize(s);

    cross(s, f, u);

    m[0] = s[0];   m[4] = s[1];   m[8]  = s[2];   m[12] = 0.0f;
    m[1] = u[0];   m[5] = u[1];   m[9]  = u[2];   m[13] = 0.0f;
    m[2] = -f[0];  m[6] = -f[1];  m[10] = -f[2];  m[14] = 0.0f;
    m[3] = 0.0f;   m[7] = 0.0f;   m[11] = 0.0f;   m[15] = 1.0f;

    m[12] = - (s[0]*eye[0] + s[1]*eye[1] + s[2]*eye[2]);
    m[13] = - (u[0]*eye[0] + u[1]*eye[1] + u[2]*eye[2]);
    m[14] =   (f[0]*eye[0] + f[1]*eye[1] + f[2]*eye[2]);
}