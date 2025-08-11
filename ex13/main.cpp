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
#include <algorithm> // pour std::max, std::min
#include "Shader.hpp"


#include "glm/glm/glm.hpp" // Types de base (vec3, mat4, etc.)
#include "glm/glm/gtc/matrix_transform.hpp" // Transformations (lookAt, perspective, etc.)
#include "glm/glm/gtc/type_ptr.hpp" // Conversion glm::mat4 en pointeur pour OpenGL

const float LVLSEA = 0.998; // Niveau de la mer
const int SUBDIVISION_ISO = 9;
float radius = 3.0f;      // distance caméra <-> cible (zoom)
float cameraYaw = 0.0f;   // angle horizontal (azimut)
float cameraPitch = 0.0f; // angle vertical (élévation), limité pour éviter flip
bool isDragging = false;
double lastMouseX = 0;
double lastMouseY = 0;
static float angle = 0.f;

float targetRadius = 2.8f;
float zoomSmoothness = 0.1f;

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

//EM_BOOL wheel_callback(int eventType, const EmscriptenWheelEvent *e, void *userData)
//{
//    if (eventType == EMSCRIPTEN_EVENT_WHEEL)
//    {
//        // Détecter si Ctrl est pressé pour un zoom plus fin
//        float zoomSpeed = e->mouse.ctrlKey ? 0.05f : 0.2f;
        
//        // Zoom logarithmique pour plus de naturel
//        float factor = e->deltaY > 0 ? 1.1f : 0.9f;
//        radius *= factor;
        
//        // Limites dynamiques
//        radius = std::max(1.2f, std::min(radius, 20.0f));
        
//        printf("Zoom: %.2f\n", radius);
//        return EM_TRUE;
//    }
//    return EM_FALSE;
//}

EM_BOOL wheel_callback(int eventType, const EmscriptenWheelEvent *e, void *userData)
{
    if (eventType == EMSCRIPTEN_EVENT_WHEEL)
    {
        float zoomSpeed = 0.02f;
        radius += e->deltaY * zoomSpeed;
        
        // Limiter directement
        radius = std::max(3.0f, radius);
        
        return EM_TRUE;
    }
    return EM_FALSE;
}


// Fonction utilitaire pour récupérer les emplacements des uniformes
void getUniformLocations(GLuint programID, const std::vector<std::string>& uniformNames, std::map<std::string, GLint>& uniformMap) {
    for (const auto& name : uniformNames) {
        uniformMap[name] = glGetUniformLocation(programID, name.c_str());
    }
}

// Fonction utilitaire pour configurer une matrice uniforme
void setMatrixUniform(GLint location, const glm::mat4& matrix) {
	if (location != -1) {
		glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
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
    const char* version = (const char*)glGetString(GL_VERSION);
    const char* glsl_version = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
    printf("OpenGL version: %s\n", version);
    printf("GLSL version: %s\n", glsl_version);
    glEnable(GL_DEPTH_TEST); // Activer test de profondeur APRES contexte actif
	glCullFace(GL_BACK);  // Éliminer les faces arrière
	glEnable(GL_CULL_FACE);
}

Planet *planet = nullptr;
Shader *planetShader = nullptr;
Shader *programAccum = nullptr;
Shader* programComposite = nullptr;

std::map<std::string, GLint> planetUniformLocations;
std::map<std::string, GLint> accumUniformLocations;

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
    emscripten_set_wheel_callback("#canvas", nullptr, true, wheel_callback);

    initPermutation();

    PlanetConfig cfg;
    cfg.subdivisions = SUBDIVISION_ISO;
    cfg.lvlSea = LVLSEA;
    cfg.biomeNoiseScale = 5.f;
    planet = &(new Planet(cfg))->generateAllLODs();
    planet->generateAtmosphere();
    planet->setLODSelected(9);

    planetShader		= new Shader(vertexShaderPlanet, fragmentShaderPlanet);
    programAccum		= new Shader(vertexShaderAtmosphere, fragmentShaderAtmosphere);
    programComposite	= new Shader(vertexShaderQuad, fragmentShaderComposite);

    // Récupération des emplacements des uniformes pour planetShader
    getUniformLocations(planetShader->ID, {"uProjection", "uModel", "uView", "uAngle", "uLvlSea", "uLightDir", "uCamPos"}, planetUniformLocations);

    // Récupération des emplacements des uniformes pour programAccum
    getUniformLocations(programAccum->ID, {"uProjection", "uModel", "uView", "uTime", "uCamPos", "uLightDir"}, accumUniformLocations);

}

glm::mat4 computeModelMatrix(float angleRadians)
{
    float c = cosf(angleRadians);
    float s = sinf(angleRadians);

    glm::mat4 model = glm::mat4(1.0f); // Matrice identité
    model[0][0] = c;
    model[0][2] = -s;
    model[2][0] = s;
    model[2][2] = c;

    return model;
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
    angle += 0.0001f;
}


void render()
{
    setup();

    //radius += (targetRadius - radius) * zoomSmoothness;
    //float view[16];
    if (radius < 4.0f)
        planet->setLODSelected(9);
    else if (radius < 5.0f)
        planet->setLODSelected(8);
    else if (radius < 6.0f)
        planet->setLODSelected(7);
    else if (radius < 20.0f)
        planet->setLODSelected(6);
    else if (radius < 100.0f)
        planet->setLODSelected(5);

    float camPosX = radius * cosf(cameraPitch) * sinf(cameraYaw);
    float camPosY = radius * sinf(cameraPitch);
    float camPosZ = radius * cosf(cameraPitch) * cosf(cameraYaw);

	glm::vec3 cameraPosition(camPosX, camPosY, camPosZ);
	glm::vec3 target(0.f, 0.f, 0.f);
	glm::vec3 up(0.f, 1.f, 0.f);
	glm::mat4 view = glm::lookAt(cameraPosition, target, up);


    float aspect = (float)width / height;
	glm::mat4 projection = glm::perspective(glm::radians(45.f), aspect, 0.1f, 100.f);

	glm::mat4 model = computeModelMatrix(angle);
	glm::vec3 lightDir(0.f, 0.f, 1.f);

    planetShader->use();
    glUniform3fv(planetUniformLocations["uLightDir"]		, 1, glm::value_ptr(lightDir)				);
    glUniform3fv(planetUniformLocations["uCamPos"]			, 1, glm::value_ptr(cameraPosition)			);
    glUniformMatrix4fv(planetUniformLocations["uProjection"], 1, GL_FALSE, glm::value_ptr(projection)	);
    glUniformMatrix4fv(planetUniformLocations["uModel"]		, 1, GL_FALSE, glm::value_ptr(model)		);
    glUniformMatrix4fv(planetUniformLocations["uView"]		, 1, GL_FALSE, glm::value_ptr(view)			);
    glUniform1f(planetUniformLocations["uLvlSea"]			, LVLSEA									);

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

    glDisable(GL_DEPTH_TEST);
    glUseProgram(programAccum->ID);
    glUniform3fv(accumUniformLocations["uLightDir"]		    , 1, glm::value_ptr(lightDir)				);
    glUniform3fv(accumUniformLocations["uCamPos"]			, 1, glm::value_ptr(cameraPosition)			);
    glUniformMatrix4fv(accumUniformLocations["uProjection"]	, 1, GL_FALSE, glm::value_ptr(projection)	);
    glUniformMatrix4fv(accumUniformLocations["uModel"]		, 1, GL_FALSE, glm::value_ptr(model)		);
    glUniformMatrix4fv(accumUniformLocations["uView"]		, 1, GL_FALSE, glm::value_ptr(view)			);
    glUniform1f(accumUniformLocations["uTime"]				, angle										);
    planet->getAtmosphere()->render();

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
    //glBlendFunc(GL_ONE, GL_ONE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

int currentFPS = 60;

void setTargetFPS(int fps) {
    currentFPS = fps;
    
    // ARRÊTER la boucle actuelle
    emscripten_cancel_main_loop();
    
    // REDÉMARRER avec nouveau timing
    emscripten_set_main_loop(render, currentFPS, true);
    
    printf("FPS changed to %d (interval: %dms)\n", fps, currentFPS);
}

int main()
{
    init();
    setTargetFPS(60);
    //emscripten_set_main_loop(render, 0, true);
    return 0;
}
