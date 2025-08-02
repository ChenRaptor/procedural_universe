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
layout(location = 2) in vec3 aNormal;    // <-- ajout

uniform float uAngle;
uniform mat4 uProjection;
uniform mat4 uView;

out float vHeight;
out vec3 vColor;
out vec3 vNormal;                        // <-- ajout

void main() {
    float c = cos(uAngle);
    float s = sin(uAngle);
    vec3 pos = aPos;

    float x = pos.x * c - pos.y * s;
    float y = pos.x * s + pos.y * c;
    vec3 rotatedPos = vec3(x, y, pos.z);

    gl_Position = uProjection * uView * vec4(rotatedPos, 1.0);

    vHeight = length(pos);
    vColor = aColor;

    // Appliquer la même rotation sur la normale
    float nx = aNormal.x * c - aNormal.y * s;
    float ny = aNormal.x * s + aNormal.y * c;
    float nz = aNormal.z;

    vNormal = normalize(vec3(nx, ny, nz));

    //vNormal = aNormal;
}
)";

const char* fragmentShaderSrc = R"(#version 300 es
precision mediump float;

uniform float uLvlSea;
uniform vec3 uLightDir;  // direction lumière normalisée

in float vHeight;
in vec3 vColor;
in vec3 vNormal;
out vec4 FragColor;

void main() {

    // Seuils définissant les zones de hauteur (modifiable selon ta scène)
    float mountainStart = 1.01;
    float mountainPeak = 1.04;

    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(uLightDir);

    float diff = max(dot(normal, lightDir), 0.0);

    // Couleur éclairée diffuse + un ambiant faible
    vec3 ambient = 0.3 * vColor;
    vec3 diffuse = diff * vColor;

    vec3 color = ambient + diffuse;

    if (vHeight < uLvlSea + 0.01) {
        color = vec3(0.0, 0.0, 0.3 + 0.2 * (uLvlSea + 0.01 - vHeight) * 50.0);
    }

    //FragColor = vec4(normalize(vNormal) * 0.5 + 0.5, 1.0);
    FragColor = vec4(color, 1.0);
}
)";

const float LVLSEA = 0.989;
const int SUBDIVISION_ISO = 9;
float radius = 2.0f;         // distance caméra <-> cible (zoom)
float cameraYaw = 0.0f;      // angle horizontal (azimut)
float cameraPitch = 0.0f;    // angle vertical (élévation), limité pour éviter flip
bool isDragging = false;
double lastMouseX = 0;
double lastMouseY = 0;

EM_BOOL mouse_down_callback(int eventType, const EmscriptenMouseEvent* e, void* userData) {
    if (eventType == EMSCRIPTEN_EVENT_MOUSEDOWN) {
        printf("DRAG\n");
        isDragging = true;
        lastMouseX = e->clientX;
        lastMouseY = e->clientY;
    } else if (eventType == EMSCRIPTEN_EVENT_MOUSEUP) {
        printf("UNDRAG\n");
        isDragging = false;
    }
    return true;
}

EM_BOOL mouse_move_callback(int eventType, const EmscriptenMouseEvent* e, void* userData) {
    printf("MOVE\n");
    if (isDragging) {
        double deltaX = e->clientX - lastMouseX;
        double deltaY = e->clientY - lastMouseY;

        lastMouseX = e->clientX;
        lastMouseY = e->clientY;

        //float sensitivity = 0.05f;
        cameraYaw += deltaX * 0.05f;
        cameraPitch += deltaY * 0.05f;
        printf("%f\n", cameraYaw);
        printf("%f\n", cameraPitch);

        // Appliquer la rotation en fonction de deltaX et deltaY
        // Exemple : rotationYaw += deltaX * sensibility
        //           rotationPitch += deltaY * sensibility
        // puis mise à jour de la direction caméra

        // (à implémenter selon ta gestion de la caméra)
    }
    return true;
}




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
const float noiseScale = 2.0f;      // fréquence du bruit
const float heightAmplitude = 0.02f; // amplitude de la déformation

// Cette fonction crée un buffer sphère avec déformation de bruit
// et stocke dans sphereVertices et sphereIndices les données pour OpenGL
std::vector<float> sphereVertices;
std::vector<unsigned int> sphereIndices;

void generateIcosphereWithNoise(int subdivisions) {
    initIcosahedron();
    subdivideTriangles(subdivisions);

    sphereVertices.clear();
    sphereIndices.clear();

    // --- Étape 1 : Création des sommets déformés + couleur ---
    for (const auto& v : vertices) {
        int octaves = 8;
        float persistence = 0.9f;

        float noiseVal = fbmPerlinNoise(v.x, v.y, v.z, octaves, persistence, noiseScale);
        float heightOffset = noiseVal * heightAmplitude;
        float deformedRadius = RADIUS + heightOffset;

        if (deformedRadius < LVLSEA) {
            deformedRadius = LVLSEA;
        }

        float posX = deformedRadius * v.x;
        float posY = deformedRadius * v.y;
        float posZ = deformedRadius * v.z;

        int colorOctaves = 3;
        float colorPersistence = 0.6f;
        float colorNoiseScale = 60.f;

        float noiseValColor = fbmPerlinNoise(v.x, v.y, v.z, colorOctaves, colorPersistence, colorNoiseScale);
        float blend = 0.5f + 0.5f * noiseVal;
        float region = 0.5f + 0.5f * noiseValColor;

        float r = (blend * 0.7f + region * 0.3f) * 0.5f + 0.7f * 0.5f;
        float g = (blend * 0.7f + region * 0.3f) * 0.5f + 0.8f * 0.5f; 
        float b = (blend * 0.7f + region * 0.3f) * 0.5f + 0.6f * 0.5f;

        sphereVertices.push_back(posX);
        sphereVertices.push_back(posY);
        sphereVertices.push_back(posZ);
        sphereVertices.push_back(r);
        sphereVertices.push_back(g);
        sphereVertices.push_back(b);
    }

    // --- Étape 2 : Préparer les indices ---
    sphereIndices.clear();
    for (unsigned int idx : indices) {
        sphereIndices.push_back(static_cast<unsigned int>(idx));
    }

    // --- Étape 3 : Calcul des normales ---
    size_t vertexCount = vertices.size();
    std::vector<Vec3> normals(vertexCount, Vec3{0.0f, 0.0f, 0.0f});

    for (size_t i = 0; i < sphereIndices.size(); i += 3) {
        unsigned int i0 = sphereIndices[i];
        unsigned int i1 = sphereIndices[i+1];
        unsigned int i2 = sphereIndices[i+2];

        Vec3 v0 = {sphereVertices[6*i0], sphereVertices[6*i0+1], sphereVertices[6*i0+2]};
        Vec3 v1 = {sphereVertices[6*i1], sphereVertices[6*i1+1], sphereVertices[6*i1+2]};
        Vec3 v2 = {sphereVertices[6*i2], sphereVertices[6*i2+1], sphereVertices[6*i2+2]};

        Vec3 edge1 = v1 - v0;
        Vec3 edge2 = v2 - v0;

        Vec3 normal = {
            edge1.y*edge2.z - edge1.z*edge2.y,
            edge1.z*edge2.x - edge1.x*edge2.z,
            edge1.x*edge2.y - edge1.y*edge2.x
        };
        normal = normal.normalize();

        normals[i0] = normals[i0] + normal;
        normals[i1] = normals[i1] + normal;
        normals[i2] = normals[i2] + normal;
    }

    for (auto& n : normals) {
        n = n.normalize();
    }

    // --- Étape 4 : Construire le buffer final avec normales incluses ---
    std::vector<float> sphereVerticesWithNormals;
    sphereVerticesWithNormals.reserve(vertexCount * 9);

    for (size_t i = 0; i < vertexCount; ++i) {
        sphereVerticesWithNormals.push_back(sphereVertices[6*i + 0]); // posX
        sphereVerticesWithNormals.push_back(sphereVertices[6*i + 1]); // posY
        sphereVerticesWithNormals.push_back(sphereVertices[6*i + 2]); // posZ

        sphereVerticesWithNormals.push_back(sphereVertices[6*i + 3]); // r
        sphereVerticesWithNormals.push_back(sphereVertices[6*i + 4]); // g
        sphereVerticesWithNormals.push_back(sphereVertices[6*i + 5]); // b

        sphereVerticesWithNormals.push_back(normals[i].x);
        sphereVerticesWithNormals.push_back(normals[i].y);
        sphereVerticesWithNormals.push_back(normals[i].z);
    }

    sphereVertices = std::move(sphereVerticesWithNormals);
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

    emscripten_set_mousedown_callback("#canvas", nullptr, true, mouse_down_callback);
    emscripten_set_mouseup_callback("#canvas", nullptr, true, mouse_down_callback);
    emscripten_set_mousemove_callback("#canvas", nullptr, true, mouse_move_callback);


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
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sphereIndices.size() * sizeof(unsigned int), sphereIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

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
    angle += 0.001f;

    emscripten_get_canvas_element_size("#canvas", &width, &height);
    glViewport(0, 0, width, height);

    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float view[16];

    // Calcul position caméra orbitale selon angles
    float camPosX = radius * cosf(cameraPitch) * sinf(cameraYaw);
    float camPosY = radius * sinf(cameraPitch);
    float camPosZ = radius * cosf(cameraPitch) * cosf(cameraYaw);

    // Point fixe regardé (exemple)
    float targetX = 0.0f;
    float targetY = 0.0f;
    float targetZ = 0.0f;

    // Calcul matrice vue
    lookAt(view,
        camPosX, camPosY, camPosZ,
        targetX, targetY, targetZ,
        0.0f, 1.0f, 0.0f);

    float projection[16];
    float aspect = (float)width / (float)height;
    perspective(projection, 45.f, aspect, 0.1f, 100.f);

    glUseProgram(program);
    float camPos[3] = {0.0f, 5.0f, 5.0f};
    float target[3] = {0.0f, 0.0f, 0.0f};
    float lightDir[3] = {
        target[0] - camPos[0],
        target[1] - camPos[1],
        target[2] - camPos[2]
    };

    // Normaliser lightDir
    float len = sqrtf(lightDir[0]*lightDir[0] + lightDir[1]*lightDir[1] + lightDir[2]*lightDir[2]);
    lightDir[0] /= len;
    lightDir[1] /= len;
    lightDir[2] /= len;

    // Envoyer la direction lumière (depuis la caméra)
    glUniform3fv(glGetUniformLocation(program, "uLightDir"), 1, lightDir);
    glUniformMatrix4fv(uProjectionLoc, 1, GL_FALSE, projection);
    glUniformMatrix4fv(uViewLoc, 1, GL_FALSE, view);
    glUniform1f(uAngleLoc, angle);
    glUniform1f(uLvlSeaLoc, LVLSEA);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)sphereIndices.size(), GL_UNSIGNED_INT, 0);

}


int main() {
    init();
    emscripten_set_main_loop(render, 0, true);
    return 0;
}
