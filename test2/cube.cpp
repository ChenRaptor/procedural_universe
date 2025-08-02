#include <emscripten/emscripten.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <cmath>

// Sommaire:
// - initialise WebGL (shader, buffers)
// - boucle de rendu dessine un cube vert

// vertex shader GLSL (simple, juste position)
const char* vertexShaderSource = R"(
  attribute vec3 position;
  uniform mat4 mvpMatrix;
  void main() {
    gl_Position = mvpMatrix * vec4(position, 1.0);
  }
)";

// fragment shader GLSL (couleur verte unie)
const char* fragmentShaderSource = R"(
  precision mediump float;
  void main() {
    gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);  // vert opaque
  }
)";

GLuint program;
GLuint positionAttribLoc;
GLuint mvpMatrixUniformLoc;

GLuint vertexBuffer;
GLuint indexBuffer;

float mvpMatrix[16];

// Cube vertices (8 sommets)
const GLfloat vertices[] = {
  -0.5f, -0.5f, -0.5f, // 0
   0.5f, -0.5f, -0.5f, // 1
   0.5f,  0.5f, -0.5f, // 2
  -0.5f,  0.5f, -0.5f, // 3
  -0.5f, -0.5f,  0.5f, // 4
   0.5f, -0.5f,  0.5f, // 5
   0.5f,  0.5f,  0.5f, // 6
  -0.5f,  0.5f,  0.5f  // 7
};

// Indices pour 12 triangles du cube (2 par face)
const GLushort indices[] = {
  0,1,2, 2,3,0,   // face arrière
  4,5,6, 6,7,4,   // face avant
  0,4,7, 7,3,0,   // face gauche
  1,5,6, 6,2,1,   // face droite
  3,2,6, 6,7,3,   // face haut
  0,1,5, 5,4,0    // face bas
};

void loadIdentity(float* m) {
  for (int i = 0; i < 16; ++i) m[i] = 0.f;
  m[0] = m[5] = m[10] = m[15] = 1.f;
}

void perspective(float* m, float fov, float aspect, float near, float far) {
  float f = 1.f / tanf(fov * 0.5f);
  loadIdentity(m);
  m[0] = f / aspect;
  m[5] = f;
  m[10] = (far + near) / (near - far);
  m[11] = -1.f;
  m[14] = (2.f * far * near) / (near - far);
  m[15] = 0.f;
}

void translate(float* m, float x, float y, float z) {
  m[12] += x;
  m[13] += y;
  m[14] += z;
}

GLuint compileShader(GLenum type, const char* src) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, NULL);
  glCompileShader(shader);
  GLint compiled;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (!compiled) {
    char log[512];
    glGetShaderInfoLog(shader, 512, NULL, log);
    printf("Shader compile error: %s\n", log);
  }
  return shader;
}

GLuint createProgram(const char* vertSrc, const char* fragSrc) {
  GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc);
  GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc);
  GLuint prog = glCreateProgram();
  glAttachShader(prog, vert);
  glAttachShader(prog, frag);
  glLinkProgram(prog);
  GLint linked;
  glGetProgramiv(prog, GL_LINK_STATUS, &linked);
  if (!linked) {
    char log[512];
    glGetProgramInfoLog(prog, 512, NULL, log);
    printf("Program link error: %s\n", log);
  }
  glDeleteShader(vert);
  glDeleteShader(frag);
  return prog;
}

void render() {
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);

  glUseProgram(program);

  // Envoi matrice MVP
  glUniformMatrix4fv(mvpMatrixUniformLoc, 1, GL_FALSE, mvpMatrix);

  // Configure vertex buffer
  glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
  glEnableVertexAttribArray(positionAttribLoc);
  glVertexAttribPointer(positionAttribLoc, 3, GL_FLOAT, GL_FALSE, 0, 0);

  // Configure index buffer
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);

  // Draw cube (12 triangles, 36 indices)
  glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0);

  glDisableVertexAttribArray(positionAttribLoc);
}

void main_loop() {
  render();
}

int main() {
  program = createProgram(vertexShaderSource, fragmentShaderSource);
  positionAttribLoc = glGetAttribLocation(program, "position");
  mvpMatrixUniformLoc = glGetUniformLocation(program, "mvpMatrix");

  glGenBuffers(1, &vertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glGenBuffers(1, &indexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

  // Setup MVP matrice (projection + translation simple)
  perspective(mvpMatrix, 3.14159f/4.f, 1.f, 0.1f, 10.f);
  translate(mvpMatrix, 0.f, 0.f, -2.f);

  emscripten_set_main_loop(main_loop, 0, 1);

  return 0;
}
