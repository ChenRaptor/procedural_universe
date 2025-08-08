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
static float angle = 0.f;
float model[16];
float view[16];

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

// Fonction utilitaire pour récupérer les emplacements des uniformes
void getUniformLocations(GLuint programID, const std::vector<std::string>& uniformNames, std::map<std::string, GLint>& uniformMap) {
    for (const auto& name : uniformNames) {
        uniformMap[name] = glGetUniformLocation(programID, name.c_str());
    }
}

// Fonction utilitaire pour configurer une matrice uniforme
void setMatrixUniform(GLint location, const float* matrix) {
    if (location != -1) {
        glUniformMatrix4fv(location, 1, GL_FALSE, matrix);
    }
}

// Fonction utilitaire pour configurer un vecteur uniforme
void setVectorUniform(GLint location, const float* vector, int size) {
    if (location != -1) {
        if (size == 3) glUniform3fv(location, 1, vector);
        else if (size == 4) glUniform4fv(location, 1, vector);
    }
}

// Fonction utilitaire pour configurer un float uniforme
void setFloatUniform(GLint location, float value) {
    if (location != -1) {
        glUniform1f(location, value);
    }
}

GLuint vao, vbo, ebo, atmosphereVao;

//GLuint programAccum, programComposite;
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
Shader *programAccum = nullptr;
Shader* programComposite = nullptr;

double width, height;

void init()
{
    init_webgl_context();

    emscripten_get_element_css_size("#canvas", &width, &height);
    int dpr = (int)emscripten_get_device_pixel_ratio();
    int canvasWidth = (int)(width * dpr);
    int canvasHeight = (int)(height * dpr);
    emscripten_set_canvas_element_size("#canvas", canvasWidth, canvasHeight);
    create_oit_framebuffer(canvasWidth, canvasHeight);

    emscripten_set_mousedown_callback("#canvas", nullptr, true, mouse_down_callback);
    emscripten_set_mouseup_callback("#canvas", nullptr, true, mouse_down_callback);
    emscripten_set_mousemove_callback("#canvas", nullptr, true, mouse_move_callback);

    initPermutation();

    PlanetConfig cfg;
    cfg.subdivisions = SUBDIVISION_ISO;
    cfg.lvlSea = LVLSEA;
    cfg.biomeNoiseScale = 5.f;
    planet = &(new Planet(cfg))->generate();
    planet->prepare_render();

    planetShader = new Shader(vertexShaderPlanet, fragmentShaderPlanet);
    programAccum = new Shader(vertexShaderAtmosphere, fragmentShaderAtmosphere);
    programComposite = new Shader(vertexShaderQuad, fragmentShaderComposite);

    uProjectionLoc = glGetUniformLocation(planetShader->ID, "uProjection");
    uModelLoc = glGetUniformLocation(planetShader->ID, "uModel");
    uViewLoc = glGetUniformLocation(planetShader->ID, "uView");
    uAngleLoc = glGetUniformLocation(planetShader->ID, "uAngle");
    uLvlSeaLoc = glGetUniformLocation(planetShader->ID, "uLvlSea");


    //std::vector<std::string> planetUniforms = {"uProjection", "uModel", "uView", "uAngle", "uLvlSea"};
    //std::map<std::string, GLint> planetUniformLocations;
    //getUniformLocations(planetShader->ID, planetUniforms, planetUniformLocations);

    uProjectionLoc2 = glGetUniformLocation(programAccum->ID, "uProjection");
    uModelLoc2 = glGetUniformLocation(programAccum->ID, "uModel");
    uViewLoc2 = glGetUniformLocation(programAccum->ID, "uView");
    uTimeLoc = glGetUniformLocation(programAccum->ID, "uTime");

        // Récupération des emplacements des uniformes pour planetShader
    std::vector<std::string> planetUniforms = {"uProjection", "uModel", "uView", "uAngle", "uLvlSea"};
    std::map<std::string, GLint> planetUniformLocations;
    getUniformLocations(planetShader->ID, planetUniforms, planetUniformLocations);

    // Récupération des emplacements des uniformes pour programAccum
    std::vector<std::string> accumUniforms = {"uProjection", "uModel", "uView", "uTime"};
    std::map<std::string, GLint> accumUniformLocations;
    getUniformLocations(programAccum->ID, accumUniforms, accumUniformLocations);

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

void setup()
{
    glViewport(0, 0, width, height);
    glClearColor(0.1f, 0.1f, 0.2f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    angle += 0.001f;
    computeModelMatrix(model, angle);
}


void render()
{
    setup();

    float view[16];
    float camPosX = radius * cosf(cameraPitch) * sinf(cameraYaw);
    float camPosY = radius * sinf(cameraPitch);
    float camPosZ = radius * cosf(cameraPitch) * cosf(cameraYaw);
    lookAt(view, camPosX, camPosY, camPosZ, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f);
    float projection[16];
    float aspect = (float)width / height;
    perspective(projection, 45.f, aspect, 0.1f, 100.f);
    planetShader->use();

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

    glDisable(GL_DEPTH_TEST);

    glUseProgram(programAccum->ID);

    glUniform3f(glGetUniformLocation(programAccum->ID, "uCamPos"), camPosX, camPosY, camPosZ);
    glUniform1f(uTimeLoc, angle);
    glUniformMatrix4fv(uModelLoc2, 1, GL_FALSE, model);
    glUniformMatrix4fv(uViewLoc2, 1, GL_FALSE, view);
    glUniformMatrix4fv(uProjectionLoc2, 1, GL_FALSE, projection);

    //planet->getAtmosphere()->render();

    // Désactiver l'offset et restaurer les paramètres
    glDisable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // PASS 2 : composition finale
    glUseProgram(programComposite->ID);
    glBindVertexArray(vaoQuad);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texAccum);
    glUniform1i(glGetUniformLocation(programComposite->ID, "uAccumTex"), 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texReveal);
    glUniform1i(glGetUniformLocation(programComposite->ID, "uRevealTex"), 1);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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
