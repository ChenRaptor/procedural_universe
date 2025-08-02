#include <GLES2/gl2.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <iostream>
#include <cmath>
#include <vector>
#include <string>

class Matrix4 {
private:
    float m[16];

public:
    Matrix4() {
        identity();
    }
    
    void identity() {
        for(int i = 0; i < 16; i++) {
            m[i] = 0.0f;
        }
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }
    
    void translate(float x, float y, float z) {
        m[12] += x;
        m[13] += y;
        m[14] += z;
    }
    
    void rotateX(float angle) {
        float cos_a = cos(angle);
        float sin_a = sin(angle);
        float temp[16];
        
        for(int i = 0; i < 16; i++) temp[i] = m[i];
        
        m[4] = temp[4] * cos_a + temp[8] * sin_a;
        m[5] = temp[5] * cos_a + temp[9] * sin_a;
        m[6] = temp[6] * cos_a + temp[10] * sin_a;
        m[7] = temp[7] * cos_a + temp[11] * sin_a;
        m[8] = temp[8] * cos_a - temp[4] * sin_a;
        m[9] = temp[9] * cos_a - temp[5] * sin_a;
        m[10] = temp[10] * cos_a - temp[6] * sin_a;
        m[11] = temp[11] * cos_a - temp[7] * sin_a;
    }
    
    void rotateY(float angle) {
        float cos_a = cos(angle);
        float sin_a = sin(angle);
        float temp[16];
        
        for(int i = 0; i < 16; i++) temp[i] = m[i];
        
        m[0] = temp[0] * cos_a - temp[8] * sin_a;
        m[1] = temp[1] * cos_a - temp[9] * sin_a;
        m[2] = temp[2] * cos_a - temp[10] * sin_a;
        m[3] = temp[3] * cos_a - temp[11] * sin_a;
        m[8] = temp[0] * sin_a + temp[8] * cos_a;
        m[9] = temp[1] * sin_a + temp[9] * cos_a;
        m[10] = temp[2] * sin_a + temp[10] * cos_a;
        m[11] = temp[3] * sin_a + temp[11] * cos_a;
    }
    
    void perspective(float fovy, float aspect, float near, float far) {
        identity();
        float f = 1.0f / tan(fovy / 2.0f);
        float range_inv = 1.0f / (near - far);
        
        m[0] = f / aspect;
        m[5] = f;
        m[10] = (near + far) * range_inv;
        m[11] = -1.0f;
        m[14] = near * far * range_inv * 2.0f;
        m[15] = 0.0f;
    }
    
    const float* data() const { return m; }
};

class CubeRenderer {
private:
    GLuint shaderProgram;
    GLuint positionBuffer;
    GLuint indexBuffer;
    GLint positionLocation;
    GLint projectionMatrixLocation;
    GLint modelViewMatrixLocation;
    GLint colorLocation;
    
    float rotationX;
    float rotationY;
    bool isRotating;
    int colorIndex;
    
    std::vector<std::vector<float>> colors;
    
    // Vertices du cube
    std::vector<float> positions = {
        // Face avant
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        
        // Face arrière
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        
        // Face haut
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
        
        // Face bas
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,
        
        // Face droite
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        
        // Face gauche
        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,
    };
    
    // Indices pour les triangles
    std::vector<GLushort> indices = {
         0,  1,  2,    0,  2,  3,    // avant
         4,  5,  6,    4,  6,  7,    // arrière
         8,  9, 10,    8, 10, 11,    // haut
        12, 13, 14,   12, 14, 15,    // bas
        16, 17, 18,   16, 18, 19,    // droite
        20, 21, 22,   20, 22, 23,    // gauche
    };

public:
    CubeRenderer() : rotationX(0.0f), rotationY(0.0f), isRotating(true), colorIndex(0) {
        // Initialisation des couleurs
        colors = {
            {0.0f, 1.0f, 0.0f, 1.0f}, // Vert
            {0.0f, 0.8f, 1.0f, 1.0f}, // Cyan
            {1.0f, 0.3f, 0.8f, 1.0f}, // Rose
            {1.0f, 0.8f, 0.0f, 1.0f}, // Jaune
            {0.8f, 0.0f, 1.0f, 1.0f}  // Violet
        };
    }
    
    bool initialize() {
        if (!initShaders()) {
            std::cerr << "Erreur lors de l'initialisation des shaders" << std::endl;
            return false;
        }
        
        initBuffers();
        setupGL();
        return true;
    }
    
private:
    GLuint loadShader(GLenum type, const std::string& source) {
        GLuint shader = glCreateShader(type);
        const char* sourceCStr = source.c_str();
        glShaderSource(shader, 1, &sourceCStr, nullptr);
        glCompileShader(shader);
        
        GLint compiled;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen > 1) {
                char* infoLog = new char[infoLen];
                glGetShaderInfoLog(shader, infoLen, nullptr, infoLog);
                std::cerr << "Erreur compilation shader: " << infoLog << std::endl;
                delete[] infoLog;
            }
            glDeleteShader(shader);
            return 0;
        }
        
        return shader;
    }
    
    bool initShaders() {
        std::string vertexShaderSource = R"(
            attribute vec4 aVertexPosition;
            uniform mat4 uModelViewMatrix;
            uniform mat4 uProjectionMatrix;
            varying vec3 vPosition;
            
            void main(void) {
                gl_Position = uProjectionMatrix * uModelViewMatrix * aVertexPosition;
                vPosition = aVertexPosition.xyz;
            }
        )";
        
        std::string fragmentShaderSource = R"(
            precision mediump float;
            uniform vec4 uColor;
            varying vec3 vPosition;
            
            void main(void) {
                float intensity = 0.7 + 0.3 * sin(vPosition.x * 3.0) * cos(vPosition.y * 3.0);
                gl_FragColor = vec4(uColor.rgb * intensity, uColor.a);
            }
        )";
        
        GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vertexShaderSource);
        GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
        
        if (vertexShader == 0 || fragmentShader == 0) {
            return false;
        }
        
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);
        
        GLint linked;
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linked);
        if (!linked) {
            GLint infoLen = 0;
            glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen > 1) {
                char* infoLog = new char[infoLen];
                glGetProgramInfoLog(shaderProgram, infoLen, nullptr, infoLog);
                std::cerr << "Erreur link program: " << infoLog << std::endl;
                delete[] infoLog;
            }
            glDeleteProgram(shaderProgram);
            return false;
        }
        
        // Récupération des locations
        positionLocation = glGetAttribLocation(shaderProgram, "aVertexPosition");
        projectionMatrixLocation = glGetUniformLocation(shaderProgram, "uProjectionMatrix");
        modelViewMatrixLocation = glGetUniformLocation(shaderProgram, "uModelViewMatrix");
        colorLocation = glGetUniformLocation(shaderProgram, "uColor");
        
        // Nettoyage
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        
        return true;
    }
    
    void initBuffers() {
        // Buffer des positions
        glGenBuffers(1, &positionBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
        glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(float), positions.data(), GL_STATIC_DRAW);
        
        // Buffer des indices
        glGenBuffers(1, &indexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLushort), indices.data(), GL_STATIC_DRAW);
    }
    
    void setupGL() {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClearDepthf(1.0f);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        
        // Configuration du viewport
        int width, height;
        emscripten_get_canvas_element_size("#canvas", &width, &height);
        glViewport(0, 0, width, height);
    }

public:
    void render() {
        if (isRotating) {
            rotationX += 0.01f;
            rotationY += 0.015f;
        }
        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Matrice de projection
        Matrix4 projectionMatrix;
        int width, height;
        emscripten_get_canvas_element_size("#canvas", &width, &height);
        float aspect = (float)width / (float)height;
        projectionMatrix.perspective(45.0f * M_PI / 180.0f, aspect, 0.1f, 100.0f);
        
        // Matrice model-view
        Matrix4 modelViewMatrix;
        modelViewMatrix.translate(0.0f, 0.0f, -6.0f);
        modelViewMatrix.rotateX(rotationX);
        modelViewMatrix.rotateY(rotationY);
        
        // Configuration des buffers
        glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
        glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(positionLocation);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
        
        // Utilisation du programme shader
        glUseProgram(shaderProgram);
        
        // Envoi des uniforms
        glUniformMatrix4fv(projectionMatrixLocation, 1, GL_FALSE, projectionMatrix.data());
        glUniformMatrix4fv(modelViewMatrixLocation, 1, GL_FALSE, modelViewMatrix.data());
        glUniform4fv(colorLocation, 1, colors[colorIndex].data());
        
        // Rendu du cube
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0);
    }
    
    void toggleRotation() {
        isRotating = !isRotating;
    }
    
    void changeColor() {
        colorIndex = (colorIndex + 1) % colors.size();
    }
    
    void resetView() {
        rotationX = 0.0f;
        rotationY = 0.0f;
        colorIndex = 0;
        isRotating = true;
    }
};

// Instance globale du renderer
CubeRenderer* renderer = nullptr;

// Fonction de boucle principale
void mainLoop() {
    if (renderer) {
        renderer->render();
    }
}

// Fonctions exportées pour JavaScript
extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void toggleRotation() {
        if (renderer) {
            renderer->toggleRotation();
        }
    }
    
    EMSCRIPTEN_KEEPALIVE
    void changeColor() {
        if (renderer) {
            renderer->changeColor();
        }
    }
    
    EMSCRIPTEN_KEEPALIVE
    void resetView() {
        if (renderer) {
            renderer->resetView();
        }
    }
}

int main() {
    std::cout << "Initialisation du Cube Renderer WebGL" << std::endl;
    
    // Création et initialisation du renderer
    renderer = new CubeRenderer();
    
    if (!renderer->initialize()) {
        std::cerr << "Erreur lors de l'initialisation du renderer" << std::endl;
        return -1;
    }
    
    std::cout << "Renderer initialisé avec succès" << std::endl;
    
    // Démarrage de la boucle de rendu
    emscripten_set_main_loop(mainLoop, 60, 1);
    
    return 0;
}