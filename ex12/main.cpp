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
float radius = 2.8f;      // distance caméra <-> cible (zoom)
float cameraYaw = 0.0f;   // angle horizontal (azimut)
float cameraPitch = 0.0f; // angle vertical (élévation), limité pour éviter flip
bool isDragging = false;
double lastMouseX = 0;
double lastMouseY = 0;

std::vector<ColorPoint> biomeColors = {
    {0.0f, {0.1f, 0.2f, 0.3f}}, // Couleur pour bruit = 0.0
    {0.3f, {0.4f, 0.6f, 0.1f}}, // Couleur pour bruit = 0.3
    {0.7f, {0.7f, 0.4f, 0.2f}}, // Couleur pour bruit = 0.7
    {1.0f, {0.9f, 0.9f, 0.9f}}  // Couleur pour bruit = 1.0
};

EM_BOOL mouse_down_callback(int eventType, const EmscriptenMouseEvent *e, void *userData)
{
    if (eventType == EMSCRIPTEN_EVENT_MOUSEDOWN)
    {
        isDragging = true;
        lastMouseX = e->clientX;
        lastMouseY = e->clientY;
    }
    else if (eventType == EMSCRIPTEN_EVENT_MOUSEUP)
    {
        isDragging = false;
    }
    return true;
}

EM_BOOL mouse_move_callback(int eventType, const EmscriptenMouseEvent *e, void *userData)
{
    if (isDragging)
    {
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

GLuint programAccum, programComposite;
GLuint vaoTriangle, vboTriangle;
GLuint vaoQuad, vboQuad;
GLuint fbo = 0, texAccum = 0, texReveal = 0;

// Emplacements des uniforms à stocker après récupération dans init()
GLint uProjectionLoc = -1;
GLint uModelLoc = -1;
GLint uViewLoc = -1;
GLint uAngleLoc = -1;
GLint uLvlSeaLoc = -1;
GLint uProjectionLoc2 = -1;
GLint uModelLoc2 = -1;
GLint uViewLoc2 = -1;
GLint uTimeLoc = -1;

// Fonction pour compiler un shader
GLuint compileShader(GLenum type, const char *src)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
    {
        char buffer[512];
        glGetShaderInfoLog(shader, 512, nullptr, buffer);
        printf("Erreur shader: %s\n", buffer);
    }
    return shader;
}

GLuint linkProgram(const char *vsSrc, const char *fsSrc)
{
    GLuint vert = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!vert || !frag)
        return 0;
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    GLint status;
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (!status)
    {
        char buf[512];
        glGetProgramInfoLog(prog, 512, NULL, buf);
        printf("Program Link Error: %s\n", buf);
        return 0;
    }
    glDeleteShader(vert);
    glDeleteShader(frag);
    return prog;
}

// void create_oit_framebuffer(int width, int height) {
//     glGenFramebuffers(1, &fbo);
//     glBindFramebuffer(GL_FRAMEBUFFER, fbo);
//     glGenTextures(1, &texAccum);
//     glBindTexture(GL_TEXTURE_2D, texAccum);
//     glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//     glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texAccum, 0);
//     glGenTextures(1, &texReveal);
//     glBindTexture(GL_TEXTURE_2D, texReveal);
//     glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, width, height, 0, GL_RED, GL_FLOAT, NULL);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//     glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, texReveal, 0);
//     GLenum drawBuffers[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
//     glDrawBuffers(2, drawBuffers);
//     if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
//         printf("Incomplete framebuffer\n");
//         exit(1);
//     }
//     glBindFramebuffer(GL_FRAMEBUFFER, 0);
// }

GLuint depthBuffer; // déclarer variable globale

void create_oit_framebuffer(int width, int height)
{
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Couleurs comme avant
    glGenTextures(1, &texAccum);
    glBindTexture(GL_TEXTURE_2D, texAccum);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texAccum, 0);

    glGenTextures(1, &texReveal);
    glBindTexture(GL_TEXTURE_2D, texReveal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, width, height, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, texReveal, 0);

    // ATTACHER UN RENDERBUFFER PROFONDEUR
    glGenRenderbuffers(1, &depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);

    GLenum drawBuffers[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, drawBuffers);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        printf("Incomplete framebuffer\n");
        exit(1);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void init_webgl_context()
{
    EmscriptenWebGLContextAttributes attr;
    emscripten_webgl_init_context_attributes(&attr);
    attr.majorVersion = 2; // WebGL2
    attr.minorVersion = 0;
    attr.alpha = false;
    attr.depth = true; // Important : activer tampon de profondeur
    attr.stencil = false;
    attr.antialias = true;
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context("#canvas", &attr);
    if (ctx <= 0)
    {
        printf("Échec création contexte WebGL2\n");
        exit(1);
    }
    emscripten_webgl_make_context_current(ctx);
    glEnable(GL_DEPTH_TEST); // Activer test de profondeur APRES contexte actif
}

Planet *planet = nullptr;
Shader *planetShader = nullptr;
// Shader* atmosphereShader = nullptr;

// Atmosphere* atmosphere = nullptr;

void init()
{
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

    planetShader = new Shader(vertexShaderPlanet, fragmentShaderPlanet);

    programAccum = linkProgram(vertexShaderAtmosphere, fragmentShaderAtmosphere);
    programComposite = linkProgram(vertexShaderQuad, fragmentShaderComposite);
    // atmosphereShader = new Shader(vertexShaderAtmosphere, fragmentShaderAtmosphere);

    uProjectionLoc = glGetUniformLocation(planetShader->ID, "uProjection");
    uModelLoc = glGetUniformLocation(planetShader->ID, "uModel");
    uViewLoc = glGetUniformLocation(planetShader->ID, "uView");
    uAngleLoc = glGetUniformLocation(planetShader->ID, "uAngle");
    uLvlSeaLoc = glGetUniformLocation(planetShader->ID, "uLvlSea");

    uProjectionLoc2 = glGetUniformLocation(programAccum, "uProjection");
    uModelLoc2 = glGetUniformLocation(programAccum, "uModel");
    uViewLoc2 = glGetUniformLocation(programAccum, "uView");
    uTimeLoc = glGetUniformLocation(programAccum, "uTime");

    planet->prepare_render();
    // atmosphere = new Atmosphere(9, 1.2f, Color{1.0f, 0.0f, 0.0f});
    // atmosphere->generate();
    // atmosphere->prepare_render();
    create_oit_framebuffer(canvasWidth, canvasHeight);
}

void computeModelMatrix(float *matrix, float angleRadians)
{
    float c = cosf(angleRadians);
    float s = sinf(angleRadians);

    matrix[0] = c;
    matrix[4] = 0.f;
    matrix[8] = -s;
    matrix[12] = 0.f;
    matrix[1] = 0.f;
    matrix[5] = 1.f;
    matrix[9] = 0.f;
    matrix[13] = 0.f;
    matrix[2] = s;
    matrix[6] = 0.f;
    matrix[10] = c;
    matrix[14] = 0.f;
    matrix[3] = 0.f;
    matrix[7] = 0.f;
    matrix[11] = 0.f;
    matrix[15] = 1.f;
}

void multiplyVectorByMatrix(const float mat[16], const float vec[4], float out[4])
{
    for (int i = 0; i < 4; ++i)
    {
        out[i] = mat[i] * vec[0] + mat[4 + i] * vec[1] + mat[8 + i] * vec[2] + mat[12 + i] * vec[3];
    }
}

void render()
{
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
    // float camPosX = 0.f, camPosY = 3.f, camPosZ = 1.f;

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
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);  // Éliminer les faces arrière
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    planet->render();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);   // source = framebuffer écran
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo); // destination = framebuffer OIT

    glBlitFramebuffer(
        0, 0, width, height, // source rectangle
        0, 0, width, height, // dest rectangle
        GL_DEPTH_BUFFER_BIT, // bit à copier : profondeur
        GL_NEAREST           // filtrage
    );

// PASS 1 : accumulation dans framebuffer
glBindFramebuffer(GL_FRAMEBUFFER, fbo);
glClearBufferfv(GL_COLOR, 0, (const GLfloat[]){0, 0, 0, 0});
glClearBufferfv(GL_COLOR, 1, (const GLfloat[]){1.0f}); // important reveal buffer à 1 !

glEnable(GL_BLEND);
glBlendFunc(GL_ONE, GL_ONE);

// Test de profondeur activé pour lire mais pas écrire
glEnable(GL_DEPTH_TEST);
glDepthFunc(GL_LEQUAL); // Utilise la profondeur de la planète
glDepthMask(GL_FALSE);

// Ajouter un offset de polygone pour pousser les fragments transparents vers l'arrière

glUseProgram(programAccum);

glUniform3f(glGetUniformLocation(programAccum, "uCamPos"), camPosX, camPosY, camPosZ);
glUniform1f(uTimeLoc, angle); // Utiliser l'angle pour animer l'atmosphère
glUniformMatrix4fv(uModelLoc2, 1, GL_FALSE, model);
glUniformMatrix4fv(uViewLoc2, 1, GL_FALSE, view);
glUniformMatrix4fv(uProjectionLoc2, 1, GL_FALSE, projection);

planet->getAtmosphere()->render();

// Désactiver l'offset et restaurer les paramètres
glDisable(GL_BLEND);
glDepthMask(GL_TRUE);
glDepthFunc(GL_LESS); // Rétablir la fonction de profondeur par défaut
glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // PASS 2 : composition finale
    glUseProgram(programComposite);
    glBindVertexArray(vaoQuad);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texAccum);
    glUniform1i(glGetUniformLocation(programComposite, "uAccumTex"), 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texReveal);
    glUniform1i(glGetUniformLocation(programComposite, "uRevealTex"), 1);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

int main()
{
    init();
    emscripten_set_main_loop(render, 0, true);
    return 0;
}
