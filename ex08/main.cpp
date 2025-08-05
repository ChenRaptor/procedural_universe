#include <map>
#include <array>
#include <vector>
#include <cmath>
#include <cstdio>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#include "main.h"
#include "IcoSphere.hpp"
#include "Planet.hpp"
#include "color.h"
#include <chrono>
#include "Shader.hpp"

const float LVLSEA = 0.998; // Niveau de la mer
const int SUBDIVISION_ISO = 9;
float radius = 2.8f;         // distance caméra <-> cible (zoom)
float cameraYaw = 0.0f;      // angle horizontal (azimut)
float cameraPitch = 0.0f;    // angle vertical (élévation), limité pour éviter flip
bool isDragging = false;
double lastMouseX = 0;
double lastMouseY = 0;

std::vector<ColorPoint> biomeColors = {
    {0.0f, {0.1f, 0.2f, 0.3f}},  // Couleur pour bruit = 0.0
    {0.3f, {0.4f, 0.6f, 0.1f}},  // Couleur pour bruit = 0.3
    {0.7f, {0.7f, 0.4f, 0.2f}},  // Couleur pour bruit = 0.7
    {1.0f, {0.9f, 0.9f, 0.9f}}   // Couleur pour bruit = 1.0
};


EM_BOOL mouse_down_callback(int eventType, const EmscriptenMouseEvent* e, void* userData) {
    if (eventType == EMSCRIPTEN_EVENT_MOUSEDOWN) {
        isDragging = true;
        lastMouseX = e->clientX;
        lastMouseY = e->clientY;
    } else if (eventType == EMSCRIPTEN_EVENT_MOUSEUP) {
        isDragging = false;
    }
    return true;
}

EM_BOOL mouse_move_callback(int eventType, const EmscriptenMouseEvent* e, void* userData) {
    if (isDragging) {
        double deltaX = e->clientX - lastMouseX;
        double deltaY = e->clientY - lastMouseY;
        lastMouseX = e->clientX;
        lastMouseY = e->clientY;
        cameraYaw += deltaX * 0.05f;
        cameraPitch += deltaY * 0.05f;
    }
    return true;
}

GLuint vao, vbo, ebo, atmosphereVao;

// Emplacements des uniforms à stocker après récupération dans init()
GLint uProjectionLoc = -1;
GLint uModelLoc = -1;
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

Planet* planet = nullptr;
Shader* planetShader = nullptr;
Shader* atmosphereShader = nullptr;

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

    PlanetConfig cfg;
    cfg.subdivisions = SUBDIVISION_ISO;
    cfg.lvlSea = LVLSEA;
    cfg.biomeNoiseScale = 5.f;
    auto start = std::chrono::high_resolution_clock::now();
    planet = new Planet(cfg);
    planet->setShowEquator(true);
    planet->generate();
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = end - start;
    printf("Temps de génération de l'icosphère : %.6f secondes\n", elapsed.count());

    planetShader = new Shader(vertexShaderSrc, fragmentShaderSrc);
    //atmosphereShader = new Shader(atmosphereVertexShaderSrc, atmosphereFragmentShaderSrc);

    uProjectionLoc  = glGetUniformLocation(planetShader->ID, "uProjection");
    uModelLoc       = glGetUniformLocation(planetShader->ID, "uModel");
    uViewLoc        = glGetUniformLocation(planetShader->ID, "uView");
    uAngleLoc       = glGetUniformLocation(planetShader->ID, "uAngle");
    uLvlSeaLoc      = glGetUniformLocation(planetShader->ID, "uLvlSea");

    planet->prepare_render();
}

void computeModelMatrix(float* matrix, float angleRadians) {
    float c = cosf(angleRadians);
    float s = sinf(angleRadians);

    matrix[0]  = c;    matrix[4]  = 0.f; matrix[8]  = -s;   matrix[12] = 0.f;
    matrix[1]  = 0.f;  matrix[5]  = 1.f; matrix[9]  = 0.f;  matrix[13] = 0.f;
    matrix[2]  = s;    matrix[6]  = 0.f; matrix[10] = c;    matrix[14] = 0.f;
    matrix[3]  = 0.f;  matrix[7]  = 0.f; matrix[11] = 0.f;   matrix[15] = 1.f;
}

void multiplyVectorByMatrix(const float mat[16], const float vec[4], float out[4]) {
    for (int i = 0; i < 4; ++i) {
        out[i] = mat[i] * vec[0] + mat[4 + i] * vec[1] + mat[8 + i] * vec[2] + mat[12 + i] * vec[3];
    }
}


void render() {
    int width, height;
    emscripten_get_canvas_element_size("#canvas", &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.1f, 0.1f, 0.2f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    static float angle = 0.f;
    angle += 0.001f;

    float model[16];
    computeModelMatrix(model, angle);

    float view[16];
    // Position caméra fixe
    //float camPosX = 0.f, camPosY = 3.f, camPosZ = 1.f;

    float camPosX = radius * cosf(cameraPitch) * sinf(cameraYaw);
    float camPosY = radius * sinf(cameraPitch);
    float camPosZ = radius * cosf(cameraPitch) * cosf(cameraYaw);

    lookAt(view, camPosX, camPosY, camPosZ, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f);

    float projection[16];
    float aspect = (float)width / height;
    perspective(projection, 45.f, aspect, 0.1f, 100.f);

    planetShader->use();

    // Direction lumière pointant vers origine (même que caméra ici)
    float lightDir[3] = {0.f, 0.f, 1.f};

    glUniform3fv(glGetUniformLocation(planetShader->ID, "uLightDir"), 1, lightDir);
    glUniform3f(glGetUniformLocation(planetShader->ID, "uCamPos"), camPosX, camPosY, camPosZ);
    glUniformMatrix4fv(uModelLoc, 1, GL_FALSE, model);
    glUniformMatrix4fv(uViewLoc, 1, GL_FALSE, view);
    glUniformMatrix4fv(uProjectionLoc, 1, GL_FALSE, projection);
    glUniform1f(uLvlSeaLoc, LVLSEA);

    planet->render();
}

int main() {
    init();
    emscripten_set_main_loop(render, 0, true);
    return 0;
}
