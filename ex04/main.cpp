#include <map>
#include <array>
#include <vector>
#include <cmath>
#include <cstdio>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>

// Shaders GLSL ES 3.0
const char* vertexShaderSrc = R"(#version 300 es
precision mediump float;

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

uniform float uAngle;
uniform mat4 uProjection;
uniform mat4 uView;

out float vHeight;
out vec3 vColor;

void main() {
    float c = cos(uAngle);
    float s = sin(uAngle);
    vec3 pos = aPos;

    // Rotation 2D autour de Z
    float x = pos.x * c - pos.y * s;
    float y = pos.x * s + pos.y * c;
    vec4 rotatedPos = vec4(x, y, pos.z, 1.0);

    gl_Position = uProjection * uView * rotatedPos;

    // Distance au centre : hauteur avec la déformation
    vHeight = length(pos);

    vColor = aColor;
}
)";

const char* fragmentShaderSrc = R"(#version 300 es
precision mediump float;

uniform float uLvlSea;

in float vHeight;
in vec3 vColor;
out vec4 FragColor;

void main() {
    vec3 color;

    // Seuils définissant les zones de hauteur (modifiable selon ta scène)
    float mountainStart = 1.01;
    float mountainPeak = 1.04;

    if (vHeight < uLvlSea + 0.01) {
        // Océan
        float t = (vHeight + 1.0) / (uLvlSea + 1.01);
        t = clamp(t, 0.0, 1.0);
        color = vec3(0.0, 0.0, 0.5 * t);
    } else {
        //float tLinear = clamp((vHeight - mountainStart) / (mountainPeak - mountainStart), 0.0, 1.0);

        //float gamma = 0.4;
        //float t = pow(tLinear, gamma);

        //vec3 landColor = vec3(0.1, 0.6, 0.1);
        //vec3 mountainColor = vec3(0.5, 0.5, 0.5);
        //vec3 snowColor = vec3(1.0, 1.0, 1.0);

        //vec3 intermediateColor = mix(landColor, mountainColor, t);

        //float snowThreshold = 0.7;
        //if (t > snowThreshold) {
        //    float snowT = (t - snowThreshold) / (1.0 - snowThreshold);
        //    color = mix(intermediateColor, snowColor, snowT);
        //} else {
        //    color = intermediateColor;
        //}

        color = vColor;
    }

    FragColor = vec4(color, 1.0);
}
)";

const float LVLSEA = 0.99;
const int SUBDIVISION_ISO = 6;





//--- Perlin Noise Implementation ---
// Table de permutations (256 éléments)
static const int permutation[256] = {
  151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,
  140,36,103,30,69,142,8,99,37,240,21,10,23,190,6,148,
  247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,
  57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,
  74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,
  60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54,
  65,25,63,161,1,216,80,73,209,76,132,187,208,89,18,169,
  200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,
  52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,
  207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,
  119,248,152,2,44,154,163,70,221,153,101,155,167,43,172,9,
  129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,
  218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,
  81,51,145,235,249,14,239,107,49,192,214,31,181,199,106,157,
  184,84,204,176,115,121,50,45,127,4,150,254,138,236,205,93,
  222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

// Tableau doublé pour éviter overflow
static std::array<int, 512> p;

void initPermutation() {
    for (int i = 0; i < 256; i++) {
        p[i] = permutation[i];
        p[256 + i] = permutation[i];
    }
}

static float fade(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

static float lerp(float t, float a, float b) {
    return a + t * (b - a);
}

static float grad(int hash, float x, float y, float z) {
    int h = hash & 15;
    float u = (h < 8) ? x : y;
    float v = (h < 4) ? y : (h == 12 || h == 14) ? x : z;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

float PerlinNoise3D(float x, float y, float z) {
    int X = (int)floor(x) & 255;
    int Y = (int)floor(y) & 255;
    int Z = (int)floor(z) & 255;

    x -= floor(x);
    y -= floor(y);
    z -= floor(z);

    float u = fade(x);
    float v = fade(y);
    float w = fade(z);

    int A = p[X] + Y;
    int AA = p[A] + Z;
    int AB = p[A + 1] + Z;
    int B = p[X + 1] + Y;
    int BA = p[B] + Z;
    int BB = p[B + 1] + Z;

    float res = lerp(w,
        lerp(v,
            lerp(u, grad(p[AA], x, y, z), grad(p[BA], x - 1, y, z)),
            lerp(u, grad(p[AB], x, y - 1, z), grad(p[BB], x - 1, y - 1, z))
        ),
        lerp(v,
            lerp(u, grad(p[AA + 1], x, y, z - 1), grad(p[BA + 1], x - 1, y, z - 1)),
            lerp(u, grad(p[AB + 1], x, y - 1, z - 1), grad(p[BB + 1], x - 1, y - 1, z - 1))
        )
    );

    return res;
}


float fbmPerlinNoise(float x, float y, float z, int octaves, float persistence, float scale) {
    float total = 0.0f;
    float frequency = scale;
    float amplitude = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; ++i) {
        total += PerlinNoise3D(x * frequency, y * frequency, z * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }

    return total / maxValue;  // normalisation approximative entre -1 et 1
}

// Structure simple 3D
struct Vec3 {
    float x, y, z;

    Vec3 operator+(const Vec3& b) const { return {x + b.x, y + b.y, z + b.z}; }
    Vec3 operator-(const Vec3& b) const { return {x - b.x, y - b.y, z - b.z}; }
    Vec3 operator*(float f) const { return {x * f, y * f, z * f}; }
    Vec3 normalize() const {
        float len = sqrtf(x*x + y*y + z*z);
        return (len > 0.00001f) ? Vec3{x / len, y / len, z / len} : *this;
    }
};

// Variables globales pour les sommets et indices
std::vector<Vec3> vertices;
std::vector<unsigned int> indices;

// Fonction utilitaire pour générer une clé unique d'une arête selon ses sommets
uint64_t edgeKey(unsigned int a, unsigned int b) {
    return (static_cast<uint64_t>(std::min(a, b)) << 32) | std::max(a, b);
}

// Cache pour éviter duplication des sommets lors de la subdivision
std::map<uint64_t, unsigned int> middlePointCache;

// Crée ou récupère le sommet au milieu de deux sommets existants
unsigned int getMiddlePoint(unsigned int p1, unsigned int p2) {
    uint64_t key = edgeKey(p1, p2);
    auto it = middlePointCache.find(key);
    if (it != middlePointCache.end()) return it->second;

    Vec3 point1 = vertices[p1];
    Vec3 point2 = vertices[p2];
    Vec3 middle = (point1 + point2) * 0.5f;
    middle = middle.normalize();

    vertices.push_back(middle);
    unsigned int i = (unsigned int)vertices.size() - 1;
    middlePointCache[key] = i;
    return i;
}

// Subdivision récursive
void subdivideTriangles(int depth) {
    for (int i = 0; i < depth; ++i) {
        std::vector<unsigned int> newIndices;
        for (size_t j = 0; j < indices.size(); j += 3) {
            unsigned int v1 = indices[j];
            unsigned int v2 = indices[j+1];
            unsigned int v3 = indices[j+2];

            unsigned int a = getMiddlePoint(v1, v2);
            unsigned int b = getMiddlePoint(v2, v3);
            unsigned int c = getMiddlePoint(v3, v1);

            newIndices.insert(newIndices.end(), {v1, a, c});
            newIndices.insert(newIndices.end(), {v2, b, a});
            newIndices.insert(newIndices.end(), {v3, c, b});
            newIndices.insert(newIndices.end(), {a, b, c});
        }
        indices = std::move(newIndices);
    }
}

// Initialisation de l'icosaèdre de base
void initIcosahedron() {
    vertices.clear();
    indices.clear();
    middlePointCache.clear();

    const float t = (1.0f + sqrtf(5.0f)) / 2.0f;

    // 12 sommets initiaux (non normalisés ici)
    std::vector<Vec3> v = {
        {-1,  t,  0}, { 1,  t,  0}, {-1, -t,  0}, { 1, -t,  0},
        { 0, -1,  t}, { 0,  1,  t}, { 0, -1, -t}, { 0,  1, -t},
        { t,  0, -1}, { t,  0,  1}, {-t,  0, -1}, {-t,  0,  1}
    };
    // Normalise tous les sommets sur la sphère unité
    for (auto& vert : v) {
        vertices.push_back(vert.normalize());
    }

    // 20 faces triangulaires de l'icosaèdre
    unsigned int idx[] = {
        0,11,5, 0,5,1, 0,1,7, 0,7,10, 0,10,11,
        1,5,9, 5,11,4, 11,10,2, 10,7,6, 7,1,8,
        3,9,4, 3,4,2, 3,2,6, 3,6,8, 3,8,9,
        4,9,5, 2,4,11, 6,2,10, 8,6,7, 9,8,1
    };
    indices.assign(idx, idx + sizeof(idx)/sizeof(unsigned int));
}

// --- Perlin Noise (identique à ton code déjà fourni) ---

// (Place ici la fonction PerlinNoise3D et la table de permutations...)


// --- Génération de l'icosphère avec déformation bruitée ---

const float RADIUS = 1.0f;
const float noiseScale = 1.0f;      // fréquence du bruit
const float heightAmplitude = 0.1f; // amplitude de la déformation

// Cette fonction crée un buffer sphère avec déformation de bruit
// et stocke dans sphereVertices et sphereIndices les données pour OpenGL
std::vector<float> sphereVertices;
std::vector<unsigned short> sphereIndices;

void generateIcosphereWithNoise(int subdivisions) {
    initIcosahedron();
    subdivideTriangles(subdivisions);

    sphereVertices.clear();
    sphereIndices.clear();

    // Pour chaque sommet, applique la déformation bruitée et crée couleur
    for (const auto& v : vertices) {
        // Paramètres d'octaves à ajuster selon le rendu souhaité
        int octaves = 8;
        float persistence = 0.8f;

        float noiseVal = fbmPerlinNoise(v.x, v.y, v.z, octaves, persistence, noiseScale);

        float heightOffset = noiseVal * heightAmplitude;

        float deformedRadius = RADIUS + heightOffset;

        if (deformedRadius < LVLSEA) {
            //float blendFactor = 0.3f; // à ajuster entre 0 (pas de correction) et 1 (clamp dur)
            //deformedRadius = deformedRadius * (1.0f - blendFactor) + minRadius * blendFactor;
            deformedRadius = LVLSEA;
        }

        float posX = deformedRadius * v.x;
        float posY = deformedRadius * v.y;
        float posZ = deformedRadius * v.z;

        int colorOctaves = 3;               // moins d'octaves pour régions plus larges
        float colorPersistence = 0.6f;
        float colorNoiseScale = 60;      // fréquence plus basse pour grandes régions
        float colorNoiseScale2 = 1;

        float noiseValColor = fbmPerlinNoise(v.x, v.y, v.z, colorOctaves, colorPersistence, colorNoiseScale);
        float noiseValColor_r = fbmPerlinNoise(v.x, v.y, v.z, colorOctaves, colorPersistence, colorNoiseScale2);
        float noiseValColor_g = fbmPerlinNoise(v.x, v.y, v.z, colorOctaves, colorPersistence, colorNoiseScale2);
        float noiseValColor_b = fbmPerlinNoise(v.x, v.y, v.z, colorOctaves, colorPersistence, colorNoiseScale2);

        //float posX = (RADIUS + heightOffset) * v.x;
        //float posY = (RADIUS + heightOffset) * v.y;
        //float posZ = (RADIUS + heightOffset) * v.z;

        // Exemple simple : palette verte-brune mixée selon noiseValColor
        float blend = 0.5f + 0.5f * noiseVal;  // normalisé [0,1] Altitude
        float region = 0.5f + 0.5f * noiseValColor;  // normalisé [0,1] Altitude
        
        float r2 = 0.3 * noiseValColor_r;
        float g2 = 1 * noiseValColor_g;
        float b2 = 0.2f * noiseValColor_b;
        //float r = blend * 0.7f + (region * 0.25f + r2 * 0.75) * 0.3f;
        //float g = blend * 0.7f + (region * 0.25f + g2 * 0.75) * 0.3f;
        //float b = blend * 0.7f + (region * 0.25f + b2 * 0.75) * 0.3f;

        float r = (blend * 0.7f + region * 0.3f) * 0.5f + r2 * 0.5f;
        float g = (blend * 0.7f + region * 0.3f) * 0.5f + g2 * 0.5f;
        float b = (blend * 0.7f + region * 0.3f) * 0.5f + b2 * 0.5f;
        //float r = blend * 0.3f + (1.0f - blend) * 0.1f;      // mélange brun clair / vert foncé
        //float g = blend * 0.6f + (1.0f - blend) * 0.3f;
        //float b = blend * 0.2f + (1.0f - blend) * 0.1f;

        // mix final possible avec la valeur ‘c’ précédente (par exemple selon hauteur)


        //float c = 0.5f + 0.5f * noiseVal;

        sphereVertices.push_back(posX);
        sphereVertices.push_back(posY);
        sphereVertices.push_back(posZ);

        sphereVertices.push_back(r);
        sphereVertices.push_back(g);
        sphereVertices.push_back(b);
    }

    // Indices : conversion de unsigned int vers unsigned short avec précaution
    for (unsigned int idx : indices) {
        if (idx > 65535) {
            // Attention : dépassement index pour unsigned short, augmenter le type si nécessaire!
            printf("Index dépasse 65535, attention!\n");
        }
        sphereIndices.push_back(static_cast<unsigned short>(idx));
    }
}

GLuint program, vao, vbo, ebo;

// Emplacements des uniforms à stocker après récupération dans init()
GLint uProjectionLoc = -1;
GLint uViewLoc = -1;
GLint uAngleLoc = -1;
GLint uLvlSeaLoc = -1;

// Fonction pour compiler un shader
GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        char buffer[512];
        glGetShaderInfoLog(shader, 512, nullptr, buffer);
        printf("Erreur shader: %s\n", buffer);
    }
    return shader;
}

void init_webgl_context() {
    EmscriptenWebGLContextAttributes attr;
    emscripten_webgl_init_context_attributes(&attr);
    attr.majorVersion = 2; // WebGL2
    attr.minorVersion = 0;
    attr.alpha = false;
    attr.depth = true;    // Important : activer tampon de profondeur
    attr.stencil = false;
    attr.antialias = true;
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context("#canvas", &attr);
    if (ctx <= 0) {
        printf("Échec création contexte WebGL2\n");
        exit(1);
    }
    emscripten_webgl_make_context_current(ctx);
    glEnable(GL_DEPTH_TEST);  // Activer test de profondeur APRES contexte actif
}

void init() {
    init_webgl_context();

    double width, height;
    emscripten_get_element_css_size("#canvas", &width, &height);
    int dpr = (int)emscripten_get_device_pixel_ratio();
    int canvasWidth = (int)(width * dpr);
    int canvasHeight = (int)(height * dpr);
    emscripten_set_canvas_element_size("#canvas", canvasWidth, canvasHeight);

    initPermutation();

    // Génère une icosphère subdivisée (par exemple 3 subdivisions ~ 642 sommets)
    generateIcosphereWithNoise(SUBDIVISION_ISO);

    GLuint vert = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);

    program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    glDeleteShader(vert);
    glDeleteShader(frag);

    uProjectionLoc = glGetUniformLocation(program, "uProjection");
    uViewLoc = glGetUniformLocation(program, "uView");
    uAngleLoc = glGetUniformLocation(program, "uAngle");
    uLvlSeaLoc = glGetUniformLocation(program, "uLvlSea");

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sphereVertices.size() * sizeof(float), sphereVertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sphereIndices.size() * sizeof(unsigned short), sphereIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}


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

void render() {
    int width, height;
    static float angle = 0.0f;
    angle += 0.01f;

    emscripten_get_canvas_element_size("#canvas", &width, &height);
    glViewport(0, 0, width, height);

    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float view[16];
    lookAt(view, 0, 5, 5, 0, 0, 0, 0, 1, 0);

    float projection[16];
    float aspect = (float)width / (float)height;
    perspective(projection, 45.f, aspect, 0.1f, 100.f);

    glUseProgram(program);
    glUniformMatrix4fv(uProjectionLoc, 1, GL_FALSE, projection);
    glUniformMatrix4fv(uViewLoc, 1, GL_FALSE, view);
    glUniform1f(uAngleLoc, angle);
    glUniform1f(uLvlSeaLoc, LVLSEA);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)sphereIndices.size(), GL_UNSIGNED_SHORT, 0);

}


int main() {
    init();
    emscripten_set_main_loop(render, 0, true);
    return 0;
}
