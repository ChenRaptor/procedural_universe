#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#include <cstdio>
#include <cmath>

// Shaders GLSL ES 3.0
const char* vertexShaderSrc = R"(#version 300 es
precision mediump float;

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

uniform float uAngle;
uniform mat4 uProjection;
uniform mat4 uView;

out vec3 vColor;

void main() {
    float c = cos(uAngle);
    float s = sin(uAngle);
    vec3 pos = aPos;

    // Appliquer rotation 2D autour de Z
    float x = pos.x * c - pos.y * s;
    float y = pos.x * s + pos.y * c;
    vec4 rotatedPos = vec4(x, y, pos.z, 1.0);

    // Appliquer View et Projection
    gl_Position = uProjection * uView * rotatedPos;

    vColor = aColor;
}
)";

const char* fragmentShaderSrc = R"(#version 300 es
precision mediump float;
in vec3 vColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(vColor, 1.0);
}
)";

// Sommets : positions + couleurs (8 sommets pour le cube)
const float vertices[] = {
    // positions        // couleurs
    -1.0f, -1.0f, -1.0f,  0, 0, 0,
    -1.0f, -1.0f,  1.0f,  0, 0, 1,
    -1.0f,  1.0f, -1.0f,  0, 1, 0,
    -1.0f,  1.0f,  1.0f,  0, 1, 1,
     1.0f, -1.0f, -1.0f,  1, 0, 0,
     1.0f, -1.0f,  1.0f,  1, 0, 1,
     1.0f,  1.0f, -1.0f,  1, 1, 0,
     1.0f,  1.0f,  1.0f,  1, 1, 1,
};

// Indices des triangles (12 triangles, 36 indices)
const unsigned short indices[] = {
    0,1,2,  1,3,2,    // face gauche
    4,6,5,  5,6,7,    // face droite
    0,4,1,  1,4,5,    // face bas
    2,3,6,  3,7,6,    // face haut
    0,2,4,  2,6,4,    // face dos
    1,5,3,  3,5,7     // face avant
};

GLuint program, vao, vbo, ebo;

// Emplacements des uniforms à stocker après récupération dans init()
GLint uProjectionLoc = -1;
GLint uViewLoc = -1;
GLint uAngleLoc = -1;

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

    GLuint vert = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);

    program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    glDeleteShader(vert);
    glDeleteShader(frag);

    // Récupération des emplacements des uniforms une fois
    uProjectionLoc = glGetUniformLocation(program, "uProjection");
    uViewLoc = glGetUniformLocation(program, "uView");
    uAngleLoc = glGetUniformLocation(program, "uAngle");

    // Setup du VAO
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // VBO sommets
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // EBO indices
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position (location=0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // couleur (location=1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
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
    angle += 0.01f; // incrément de rotation

    emscripten_get_canvas_element_size("#canvas", &width, &height);
    glViewport(0, 0, width, height);

    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float view[16];
    lookAt(view, 0, 5, 5,    // position caméra (z=3)
                0, 0, 0,    // regarde origine
                0, 1, 0);   // up = axe Y

    float projection[16];
    float aspect = (float)width / (float)height;
    perspective(projection, 45.f, aspect, 0.1f, 100.f);

    glUseProgram(program);

    glUniformMatrix4fv(uProjectionLoc, 1, GL_FALSE, projection);
    glUniformMatrix4fv(uViewLoc, 1, GL_FALSE, view);
    glUniform1f(uAngleLoc, angle);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0);
}

int main() {
    init();
    emscripten_set_main_loop(render, 0, true);
    return 0;
}
