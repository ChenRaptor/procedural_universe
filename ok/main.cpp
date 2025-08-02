#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES2/gl2.h>
#include <cmath>
#include <cstdio>

// Shaders
const char* vertexShaderSource = R"(
attribute vec3 aPosition;
uniform mat4 uMVPMatrix;
void main() {
    gl_Position = uMVPMatrix * vec4(aPosition, 1.0);
}
)";

const char* fragmentShaderSource = R"(
precision mediump float;
void main() {
    gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0); // Vert
}
)";

// Variables globales
GLuint shaderProgram;
GLuint VBO, VAO;
GLint mvpLocation;
float rotationAngle = 0.0f;

// Vertices du cube
float vertices[] = {
    // Face avant
    -0.5f, -0.5f,  0.5f,
     0.5f, -0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
    
    // Face arrière
    -0.5f, -0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
    -0.5f,  0.5f, -0.5f
};

unsigned int indices[] = {
    // Face avant
    0, 1, 2, 2, 3, 0,
    // Face droite
    1, 5, 6, 6, 2, 1,
    // Face arrière
    7, 6, 5, 5, 4, 7,
    // Face gauche
    4, 0, 3, 3, 7, 4,
    // Face haut
    3, 2, 6, 6, 7, 3,
    // Face bas
    4, 5, 1, 1, 0, 4
};

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        printf("Erreur de compilation du shader: %s\n", infoLog);
    }
    
    return shader;
}

void multiplyMatrix(float* result, const float* a, const float* b) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            result[i * 4 + j] = 0;
            for (int k = 0; k < 4; k++) {
                result[i * 4 + j] += a[i * 4 + k] * b[k * 4 + j];
            }
        }
    }
}

void createPerspectiveMatrix(float* matrix, float fov, float aspect, float near, float far) {
    float f = 1.0f / tan(fov * 0.5f);
    
    for (int i = 0; i < 16; i++) matrix[i] = 0.0f;
    
    matrix[0] = f / aspect;
    matrix[5] = f;
    matrix[10] = (far + near) / (near - far);
    matrix[11] = -1.0f;
    matrix[14] = (2.0f * far * near) / (near - far);
}

void createRotationMatrix(float* matrix, float angle) {
    float c = cos(angle);
    float s = sin(angle);
    
    for (int i = 0; i < 16; i++) matrix[i] = 0.0f;
    
    matrix[0] = c; matrix[2] = s;
    matrix[5] = 1.0f;
    matrix[8] = -s; matrix[10] = c;
    matrix[15] = 1.0f;
}

void createTranslationMatrix(float* matrix, float x, float y, float z) {
    for (int i = 0; i < 16; i++) matrix[i] = 0.0f;
    
    matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.0f;
    matrix[12] = x;
    matrix[13] = y;
    matrix[14] = z;
}

void init() {
    // Compilation des shaders
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    
    // Création du programme
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    
    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        printf("Erreur de liaison du programme: %s\n", infoLog);
    }
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    // Configuration des buffers
    GLuint EBO;
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    
    GLint positionLocation = glGetAttribLocation(shaderProgram, "aPosition");
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(positionLocation);
    
    mvpLocation = glGetUniformLocation(shaderProgram, "uMVPMatrix");
    
    // Configuration WebGL
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
}

void render() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glUseProgram(shaderProgram);
    
    // Matrices de transformation
    float projection[16], rotation[16], translation[16], model[16], mvp[16];
    
    createPerspectiveMatrix(projection, 3.14159f / 4.0f, 800.0f / 600.0f, 0.1f, 100.0f);
    createRotationMatrix(rotation, rotationAngle);
    createTranslationMatrix(translation, 0.0f, 0.0f, -3.0f);
    
    multiplyMatrix(model, translation, rotation);
    multiplyMatrix(mvp, projection, model);
    
    glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, mvp);
    
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    GLint positionLocation = glGetAttribLocation(shaderProgram, "aPosition");
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(positionLocation);
    
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    
    rotationAngle += 0.02f;
}

void mainLoop() {
    render();
}

int main() {
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.majorVersion = 2;
    attrs.minorVersion = 0;
    
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context = emscripten_webgl_create_context("#canvas", &attrs);
    emscripten_webgl_make_context_current(context);
    
    init();
    
    emscripten_set_main_loop(mainLoop, 60, 1);
    
    return 0;
}