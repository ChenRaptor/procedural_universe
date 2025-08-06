#include <map>
#include <array>
#include <vector>
#include <cmath>
#include <cstdio>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#include "main.h"
#include "Icosphere.hpp"
#include "color.h"
#include <chrono>

const float LVLSEA = 0.998; // Niveau de la mer
const int SUBDIVISION_ISO = 9;
float radius = 3.0f;         // distance caméra <-> cible (zoom)
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

IcoSphere* planet = nullptr;

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
    planet = new IcoSphere(cfg);
    //planet->setShowEquator(true);
    planet->generate();
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = end - start;
    printf("Temps de génération de l'icosphère : %.6f secondes\n", elapsed.count());

    GLuint vert = compileShader(GL_VERTEX_SHADER, vertexShaderPlanet);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragmentShaderPlanet);

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
    glBufferData(GL_ARRAY_BUFFER, planet->getVertices().size() * sizeof(float), planet->getVertices().data(), GL_STATIC_DRAW);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, planet->getIndices().size() * sizeof(unsigned int), planet->getIndices().data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
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
    float camPosX = radius * cosf(cameraPitch) * sinf(cameraYaw);
    float camPosY = radius * sinf(cameraPitch);
    float camPosZ = radius * cosf(cameraPitch) * cosf(cameraYaw);
    float targetX = 0.0f;
    float targetY = 0.0f;
    float targetZ = 0.0f;
    float campos2[3] = {camPosX, camPosY, camPosZ};
    lookAt(view, camPosX, camPosY, camPosZ, targetX, targetY, targetZ, 0.0f, 1.0f, 0.0f);

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

    float len = sqrtf(lightDir[0]*lightDir[0] + lightDir[1]*lightDir[1] + lightDir[2]*lightDir[2]);
    lightDir[0] /= len;
    lightDir[1] /= len;
    lightDir[2] /= len;

    glUniform3fv(glGetUniformLocation(program, "uLightDir"), 1, lightDir);
    glUniform3fv(glGetUniformLocation(program, "uCamPos"), 1, campos2);
    glUniformMatrix4fv(uProjectionLoc, 1, GL_FALSE, projection);
    glUniformMatrix4fv(uViewLoc, 1, GL_FALSE, view);
    glUniform1f(uAngleLoc, angle);
    glUniform1f(uLvlSeaLoc, LVLSEA);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)planet->getIndices().size(), GL_UNSIGNED_INT, 0);
}


int main() {
    init();
    emscripten_set_main_loop(render, 0, true);
    return 0;
}
