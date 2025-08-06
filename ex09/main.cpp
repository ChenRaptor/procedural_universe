#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#include <cstdio>
#include <cmath>

const int WIDTH = 800;
const int HEIGHT = 600;

GLuint fbo;
GLuint accumTex, revealageTex, depthBuffer;
GLuint shaderOpaque, shaderTransparent, shaderComposite;
GLuint vaoOpaque, vboOpaque, vaoTri, vboTri, vaoQuad, vboQuad;

float identity[16];
float proj[16];
float view[16];
float mvp[16];

void perspective(float* m, float fovy, float aspect, float near, float far) {
    float f = 1.0f / tanf(fovy * 3.14159265f / 360.0f);
    m[0] = f / aspect; m[1] = 0; m[2] = 0; m[3] = 0;
    m[4] = 0; m[5] = f; m[6] = 0; m[7] = 0;
    m[8] = 0; m[9] = 0; m[10] = (far + near) / (near - far); m[11] = -1;
    m[12] = 0; m[13] = 0; m[14] = (2 * far * near) / (near - far); m[15] = 0;
}

void normalize(float* v) {
    float len = sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    v[0] /= len; v[1] /= len; v[2] /= len;
}

void cross(const float* a, const float* b, float* r) {
    r[0] = a[1]*b[2] - a[2]*b[1];
    r[1] = a[2]*b[0] - a[0]*b[2];
    r[2] = a[0]*b[1] - a[1]*b[0];
}

void lookAt(float* m,
            float eyeX, float eyeY, float eyeZ,
            float centerX, float centerY, float centerZ,
            float upX, float upY, float upZ) {
    float f[3] = {centerX - eyeX, centerY - eyeY, centerZ - eyeZ};
    normalize(f);
    float up[3] = {upX, upY, upZ};
    normalize(up);
    float s[3];
    cross(f, up, s);
    normalize(s);
    float u[3];
    cross(s, f, u);

    m[0] = s[0]; m[4] = s[1]; m[8]  = s[2];  m[12] = 0;
    m[1] = u[0]; m[5] = u[1]; m[9]  = u[2];  m[13] = 0;
    m[2] = -f[0];m[6] = -f[1];m[10] = -f[2]; m[14] = 0;
    m[3] = 0;    m[7] = 0;    m[11] = 0;     m[15] = 1;

    float t[16] = {
        1,0,0,-eyeX,
        0,1,0,-eyeY,
        0,0,1,-eyeZ,
        0,0,0,1
    };

    float res[16];
    for (int i=0; i<4; ++i) {
        for (int j=0; j<4; ++j) {
            res[i*4 + j] = 0;
            for (int k=0; k<4; ++k)
                res[i*4 + j] += m[i*4 + k] * t[k*4 + j];
        }
    }
    for(int i=0;i<16;++i) m[i] = res[i];
}

void mat4_mul(float* dest, const float* a, const float* b) {
    for(int i=0;i<4;++i) {
        for(int j=0;j<4;++j) {
            dest[i*4+j] = 0;
            for(int k=0;k<4;++k)
                dest[i*4+j] += a[i*4+k]*b[k*4+j];
        }
    }
}

const char* vertexShaderOpaqueSrc = R"(#version 300 es
precision mediump float;
layout(location=0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

const char* fragmentShaderOpaqueSrc = R"(#version 300 es
precision mediump float;
out vec4 FragColor;
void main() {
    FragColor = vec4(0.8, 0.3, 0.3, 1.0);
}
)";

const char* vertexShaderTransparentSrc = R"(#version 300 es
precision mediump float;
layout(location=0) in vec3 aPos;
layout(location=1) in vec4 aColor;
out vec4 vColor;
uniform mat4 uMVP;
void main() {
    vColor = aColor;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

const char* fragmentShaderTransparentSrc = R"(#version 300 es
precision mediump float;
in vec4 vColor;
layout(location=0) out vec4 FragColor0;
layout(location=1) out float FragColor1;
void main() {
    float alpha = vColor.a;
    float weight = max(0.01, alpha);
    vec3 accum = vColor.rgb * alpha * weight;
    float revealage = alpha * weight;
    FragColor0 = vec4(accum, revealage);
    FragColor1 = revealage;
}
)";

const char* vertexShaderFullScreenSrc = R"(#version 300 es
precision mediump float;
layout(location=0) in vec2 aPos;
out vec2 uv;
void main() {
    uv = aPos * 0.5 + 0.5;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char* fragmentShaderCompositeSrc = R"(#version 300 es
precision mediump float;
in vec2 uv;
uniform sampler2D accumTex;
uniform sampler2D revealageTex;
out vec4 FragColor;
void main() {
    vec4 accum = texture(accumTex, uv);
    float reveal = texture(revealageTex, uv).r;
    vec3 color = vec3(0.0);
    if (reveal > 0.001) {
        color = accum.rgb / reveal;
    }
    FragColor = vec4(color, 1.0);
}
)";

GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader,1,&src,nullptr);
    glCompileShader(shader);
    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if(status != GL_TRUE) {
        char buf[512];
        glGetShaderInfoLog(shader,512,nullptr,buf);
        printf("Shader compile error: %s\n", buf);
    }
    return shader;
}

GLuint linkProgram(GLuint vert, GLuint frag) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    GLint status =0;
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if(status!=GL_TRUE) {
        char buf[512];
        glGetProgramInfoLog(prog,512,nullptr,buf);
        printf("Program link error: %s\n",buf);
    }
    return prog;
}

void initFBO() {
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &accumTex);
    glBindTexture(GL_TEXTURE_2D, accumTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, WIDTH, HEIGHT, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, accumTex, 0);

    glGenTextures(1, &revealageTex);
    glBindTexture(GL_TEXTURE_2D, revealageTex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_R16F,WIDTH,HEIGHT,0,GL_RED,GL_FLOAT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, revealageTex,0);

    glGenRenderbuffers(1,&depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, WIDTH, HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);

    GLenum bufs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, bufs);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("Framebuffer not complete!\n");
        exit(1);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void initFullScreenQuad() {
    float quadVertices[] = {
        -1.0f, -1.0f, // Bottom-left
         1.0f, -1.0f, // Bottom-right
        -1.0f,  1.0f, // Top-left
         1.0f,  1.0f  // Top-right
    };

    glGenVertexArrays(1, &vaoQuad);
    glBindVertexArray(vaoQuad);

    glGenBuffers(1, &vboQuad);
    glBindBuffer(GL_ARRAY_BUFFER, vboQuad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,              // attribute index 0
        2,              // size: 2 floats per vertex (x, y)
        GL_FLOAT,       // type
        GL_FALSE,       // normalized?
        2 * sizeof(float), // stride: 2 floats per vertex
        (void*)0        // array buffer offset
    );

    glBindVertexArray(0);
}


void initOpaqueTriangle() {
    float verts[] = {
        0.f,0.7f,0.f,
       -0.7f,-0.7f,0.f,
        0.7f,-0.7f,0.f,
    };
    glGenVertexArrays(1, &vaoOpaque);
    glBindVertexArray(vaoOpaque);
    glGenBuffers(1, &vboOpaque);
    glBindBuffer(GL_ARRAY_BUFFER, vboOpaque);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,0);
    glBindVertexArray(0);
}

void initTransparentTriangle() {
    float verts[] = {
        0.f,0.5f,0.f, 1,0,0,0.5f,
       -0.5f,-0.5f,0.f, 0,1,0,0.5f,
        0.5f,-0.5f,0.f, 0,0,1,0.5f,
    };
    glGenVertexArrays(1, &vaoTri);
    glBindVertexArray(vaoTri);
    glGenBuffers(1, &vboTri);
    glBindBuffer(GL_ARRAY_BUFFER, vboTri);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,7*sizeof(float),(void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,7*sizeof(float),(void*)(3*sizeof(float)));
    glBindVertexArray(0);
}

void renderOpaque() {
    glUseProgram(shaderOpaque);
    GLint loc = glGetUniformLocation(shaderOpaque, "uMVP");
    glUniformMatrix4fv(loc,1,GL_FALSE,mvp);
    glBindVertexArray(vaoOpaque);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void renderTransparent() {
    glUseProgram(shaderTransparent);
    GLint loc = glGetUniformLocation(shaderTransparent, "uMVP");
    glUniformMatrix4fv(loc,1,GL_FALSE,mvp);
    glBindVertexArray(vaoTri);
    glDrawArrays(GL_TRIANGLES,0,3);
    glBindVertexArray(0);
}

void renderComposite() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glUseProgram(shaderComposite);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, accumTex);
    glUniform1i(glGetUniformLocation(shaderComposite, "accumTex"), 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, revealageTex);
    glUniform1i(glGetUniformLocation(shaderComposite, "revealageTex"), 1);
    glBindVertexArray(vaoQuad);
    glDrawArrays(GL_TRIANGLE_STRIP,0,4);
    glBindVertexArray(0);
}


void mainLoop() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, WIDTH, HEIGHT);
    glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    //renderOpaque();

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, WIDTH, HEIGHT);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    renderTransparent();

    glDisable(GL_BLEND);

    renderComposite();
}

int main() {
    for(int i=0;i<16;i++) identity[i] = 0.f;
    identity[0] = identity[5] = identity[10] = identity[15] = 1.f;

    perspective(proj, 45.f, (float)WIDTH/(float)HEIGHT, 0.1f, 100.f);
    lookAt(view, 0, 0, 10, 0, 0, 0, 0, 1, 0);
    float mv[16];
    mat4_mul(mv, view, identity);
    mat4_mul(mvp, proj, mv);

    EmscriptenWebGLContextAttributes attr;
    emscripten_webgl_init_context_attributes(&attr);
    attr.alpha = true;
    attr.depth = true;
    attr.stencil = false;
    attr.antialias = true;
    attr.majorVersion = 2;
    attr.minorVersion = 0;

    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context("#canvas", &attr);
    if (ctx <= 0) {
        printf("Failed to create WebGL2 context!\n");
        return 1;
    }
    emscripten_webgl_make_context_current(ctx);

    initFBO();
    initFullScreenQuad();
    initOpaqueTriangle();
    initTransparentTriangle();

    GLuint vO = compileShader(GL_VERTEX_SHADER, vertexShaderOpaqueSrc);
    GLuint fO = compileShader(GL_FRAGMENT_SHADER, fragmentShaderOpaqueSrc);
    shaderOpaque = linkProgram(vO, fO);
    glDeleteShader(vO);
    glDeleteShader(fO);

    GLuint vT = compileShader(GL_VERTEX_SHADER, vertexShaderTransparentSrc);
    GLuint fT = compileShader(GL_FRAGMENT_SHADER, fragmentShaderTransparentSrc);
    shaderTransparent = linkProgram(vT, fT);
    glDeleteShader(vT);
    glDeleteShader(fT);

    GLuint vQ = compileShader(GL_VERTEX_SHADER, vertexShaderFullScreenSrc);
    GLuint fQ = compileShader(GL_FRAGMENT_SHADER, fragmentShaderCompositeSrc);
    shaderComposite = linkProgram(vQ, fQ);
    glDeleteShader(vQ);
    glDeleteShader(fQ);

    emscripten_set_main_loop(mainLoop, 0, true);
    return 0;
}
