#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#include <cmath>
#include <cstdio>

void init_webgl_context() {
    EmscriptenWebGLContextAttributes attr;
    emscripten_webgl_init_context_attributes(&attr);
    attr.alpha = false;
    attr.depth = true;
    attr.stencil = false;
    attr.antialias = true;
    attr.majorVersion = 2;   // WebGL2
    attr.minorVersion = 0;

    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context("#canvas", &attr);
    if (ctx <= 0) {
        printf("Failed to create WebGL2 context!\n");
        exit(1);
    }
    emscripten_webgl_make_context_current(ctx);

    const char* glslVersion = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
    printf("GLSL Version: %s\n", glslVersion);
}

// Vertex shader GLSL (WebGL2 / OpenGL ES 3.0)
const char* vertexShaderSrc = R"(#version 300 es
void main() {
    gl_Position = vec4(aPos, 1.0);
    vColor = aColor;
}

)";

// Fragment shader GLSL
const char* fragmentShaderSrc = R"(#version 300 es
precision mediump float;

in vec3 vColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(vColor, 1.0);
}
)";

//// Cube vertices (positions + colors)
//const float vertices[] = {
//    // positions         // colors
//    -1,-1,-1,   0,0,0,
//    -1,-1, 1,   0,0,1,
//    -1, 1, 1,   0,1,1,
//    -1, 1,-1,   0,1,0,

//     1,-1,-1,   1,0,0,
//     1,-1, 1,   1,0,1,
//     1, 1, 1,   1,1,1,
//     1, 1,-1,   1,1,0
//};

//// Indices for the cube (6 faces * 2 triangles * 3 vertices)
//const unsigned short indices[] = {
//    0,1,2,  2,3,0,    // left
//    4,5,6,  6,7,4,    // right
//    0,1,5,  5,4,0,    // bottom
//    3,2,6,  6,7,3,    // top
//    1,2,6,  6,5,1,    // front
//    0,3,7,  7,4,0     // back
//};


// Triangle (positions + couleurs intercalées)
const float vertices[] = {
     0.0f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f,   // sommet haut, rouge
    -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,   // sommet bas gauche, vert
     0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f    // sommet bas droit, bleu
};

const unsigned short indices[] = {
    0, 1, 2
};

GLuint program;
GLuint vao, vbo, ebo;
float angle = 0.f;

// Simple 4x4 matrix multiply (C = A * B)
void mat4_mul(float* C, const float* A, const float* B) {
    for(int i=0; i<4; i++) {
        for(int j=0; j<4; j++) {
            C[i*4+j] = 0.f;
            for(int k=0; k<4; k++)
                C[i*4+j] += A[i*4+k]*B[k*4+j];
        }
    }
}

// Simple perspective matrix (fovy in degrees, aspect ratio, near, far)
void perspective(float* m, float fovy, float aspect, float near, float far) {
    float f = 1.0f / tanf(fovy * 3.14159265f / 360.0f);
    m[0] = f / aspect; m[1] = 0; m[2] = 0;                          m[3] = 0;
    m[4] = 0;          m[5] = f; m[6] = 0;                          m[7] = 0;
    m[8] = 0;          m[9] = 0; m[10] = (far + near) / (near - far); m[11] = -1;
    m[12] = 0;         m[13] = 0; m[14] = (2*far*near) / (near - far); m[15] = 0;
}

// Simple lookAt matrix
void lookAt(float* m, float eyeX, float eyeY, float eyeZ,
                    float centerX, float centerY, float centerZ,
                    float upX, float upY, float upZ) {
    float fx = centerX - eyeX;
    float fy = centerY - eyeY;
    float fz = centerZ - eyeZ;
    // normalize f
    float rlf = 1.0f / sqrtf(fx*fx + fy*fy + fz*fz);
    fx *= rlf; fy *= rlf; fz *= rlf;

    // up normalized
    float rlu = 1.0f / sqrtf(upX*upX + upY*upY + upZ*upZ);
    upX *= rlu; upY *= rlu; upZ *= rlu;

    // s = f x up
    float sx = fy * upZ - fz * upY;
    float sy = fz * upX - fx * upZ;
    float sz = fx * upY - fy * upX;
    // normalize s
    float rls = 1.0f / sqrtf(sx*sx + sy*sy + sz*sz);
    sx *= rls; sy *= rls; sz *= rls;

    // u = s x f
    float ux = sy * fz - sz * fy;
    float uy = sz * fx - sx * fz;
    float uz = sx * fy - sy * fx;

    m[0] = sx;  m[1] = ux;  m[2] = -fx; m[3] = 0;
    m[4] = sy;  m[5] = uy;  m[6] = -fy; m[7] = 0;
    m[8] = sz;  m[9] = uz;  m[10] = -fz;m[11] = 0;
    m[12] = 0;  m[13] = 0;  m[14] = 0;  m[15] = 1;

    // translation
    float t[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        -eyeX,-eyeY,-eyeZ,1
    };

    mat4_mul(m, m, t);
}

// Simple rotation matrix around Y axis
void rotationY(float* m, float angleDeg) {
    float rad = angleDeg * 3.14159265f / 180.f;
    float c = cosf(rad);
    float s = sinf(rad);

    m[0] = c;   m[1] = 0; m[2] = s;  m[3] = 0;
    m[4] = 0;   m[5] = 1; m[6] = 0;  m[7] = 0;
    m[8] = -s;  m[9] = 0; m[10] = c; m[11] = 0;
    m[12] = 0;  m[13] = 0;m[14] = 0;  m[15] = 1;
}

GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if(status != GL_TRUE) {
        char buffer[512];
        glGetShaderInfoLog(shader, 512, nullptr, buffer);
        printf("Shader compile error: %s\n", buffer);
    }
    return shader;
}

void init() {

    init_webgl_context();
    GLuint vert = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);

    program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if(status != GL_TRUE) {
        char buffer[512];
        glGetProgramInfoLog(program, 512, nullptr, buffer);
        printf("Program link error: %s\n", buffer);
    }

    glDeleteShader(vert);
    glDeleteShader(frag);

    // Setup buffers
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // VBO for vertices and colors interleaved
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // EBO for indices
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Position attribute (location=0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Color attribute (location=1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void render() {
    // Récupérer la taille du canvas (800x600 est fixe dans la perspective mais le canvas peut avoir une autre taille)
    glDisable(GL_CULL_FACE);
    int width, height;
    emscripten_get_canvas_element_size("#canvas", &width, &height);

    glViewport(0, 0, width, height);  // définir le viewport à la taille réelle

    glEnable(GL_DEPTH_TEST);          // activer le test de profondeur AVANT le clear

    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(program);

    // Matrices
    float proj[16];
    float view[16];
    float model[16];
    float mv[16];
    float mvp[16];

    perspective(proj, 45.f, (float)width / (float)height, 0.1f, 100.f);
    lookAt(view, 3, 3, 3, 0, 0, 0, 0, 1, 0);
    rotationY(model, angle);

    mat4_mul(mv, view, model);
    mat4_mul(mvp, proj, mv);

    GLint loc = glGetUniformLocation(program, "uMVP");
    glUniformMatrix4fv(loc, 1, GL_FALSE, mvp);

    glBindVertexArray(vao);
    //glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0);
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, 0);

    angle += 0.5f;  // rotation
}


int main() {
    init();

    emscripten_set_main_loop(render, 0, true);
    return 0;
}
