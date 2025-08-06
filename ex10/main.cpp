#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#include <cstdio>
#include <cmath>

// Vertex shader commun (position + couleur RGBA)
const char* vertexShaderSrc = R"(#version 300 es
precision mediump float;
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
uniform float uAngle;
uniform mat4 uProjection;
uniform mat4 uView;
uniform vec2 uOffset;  // x = décalage X (souvent 0), y = décalage Y
uniform vec4 uColor; // Couleur uniforme pour les triangles

out vec4 vColor;
void main() {
    float c = cos(uAngle);
    float s = sin(uAngle);
    vec3 pos = aPos;
    float x = pos.x * c - pos.y * s;
    float y = pos.x * s + pos.y * c;
    //vec4 rotatedPos = vec4(x + uOffset.x, y + uOffset.y, pos.z, 1.0);
    vec4 rotatedPos = vec4(x, y + uOffset.y, pos.z + uOffset.x, 1.0);
    gl_Position = uProjection * uView * rotatedPos;
    vColor = uColor;
}
)";

// Fragment shader d'accumulation weighted blended OIT
const char* fragmentShaderAccum = R"(#version 300 es
precision mediump float;
in vec4 vColor;
layout(location = 0) out vec4 outAccum;
layout(location = 1) out float outReveal;
void main() {
    float alpha = vColor.a;
    float weight = clamp(pow(min(1.0, alpha * 10.0) + 0.01, 3.0), 0.01, 1.0);

    outAccum = vec4(vColor.rgb * alpha, alpha) * weight;
    outReveal = (1.0 - alpha) * weight;
}
)";

// Vertex shader quad fullscreen (gl_VertexID)
const char* vertexShaderQuad = R"(#version 300 es
precision mediump float;
const vec2 quadVertices[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0, -1.0),
    vec2( 1.0,  1.0)
);
out vec2 vUV;
void main() {
    vec2 pos = quadVertices[gl_VertexID];
    gl_Position = vec4(pos, 0.0, 1.0);
    vUV = pos * 0.5 + 0.5;
}
)";

// Fragment shader composition finale
const char* fragmentShaderComposite = R"(#version 300 es
precision mediump float;
uniform sampler2D uAccumTex;
uniform sampler2D uRevealTex;
in vec2 vUV;
out vec4 FragColor;
void main() {
    vec4 accum = texture(uAccumTex, vUV);
    float reveal = texture(uRevealTex, vUV).r;
    if (reveal > 0.0) {
        FragColor = vec4(accum.rgb / reveal, 1.0 - reveal);
    } else {
        FragColor = vec4(0.0);
    }
}
)";

// Triangle RGBA
const float vertices[] = {
     0.0f,  0.577f, 0.0f,  1, 0, 0, 0.5f,
    -0.5f, -0.288f, 0.0f,  1, 0, 0, 0.5f,
     0.5f, -0.288f, 0.0f,  1, 0, 0, 0.5f,
};

// Globals
GLuint programAccum, programComposite;
GLuint vaoTriangle, vboTriangle;
GLuint vaoQuad, vboQuad;
GLuint fbo = 0, texAccum = 0, texReveal = 0;
int canvasWidth = 800, canvasHeight = 600;

GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        char buf[512];
        glGetShaderInfoLog(shader, 512, NULL, buf);
        printf("Shader Error: %s\n", buf);
        return 0;
    }
    return shader;
}

GLuint linkProgram(const char* vsSrc, const char* fsSrc) {
    GLuint vert = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!vert || !frag) return 0;
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    GLint status;
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (!status) {
        char buf[512];
        glGetProgramInfoLog(prog, 512, NULL, buf);
        printf("Program Link Error: %s\n", buf);
        return 0;
    }
    glDeleteShader(vert);
    glDeleteShader(frag);
    return prog;
}

void init_webgl_context() {
    EmscriptenWebGLContextAttributes attr;
    emscripten_webgl_init_context_attributes(&attr);
    attr.majorVersion = 2;
    attr.minorVersion = 0;
    attr.alpha = false;
    attr.depth = true;
    attr.stencil = false;
    attr.antialias = true;
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context("#canvas", &attr);
    if (ctx <= 0) {
        printf("Failed to create WebGL2 context\n");
        exit(1);
    }
    emscripten_webgl_make_context_current(ctx);
}

void create_oit_framebuffer(int width, int height) {
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
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
    GLenum drawBuffers[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, drawBuffers);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("Incomplete framebuffer\n");
        exit(1);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void init() {
    init_webgl_context();
        double width, height;
    emscripten_get_element_css_size("#canvas", &width, &height);
    int dpr = (int)emscripten_get_device_pixel_ratio();
    int canvasWidth = (int)(width * dpr);
    int canvasHeight = (int)(height * dpr);
    emscripten_set_canvas_element_size("#canvas", canvasWidth, canvasHeight);

    programAccum = linkProgram(vertexShaderSrc, fragmentShaderAccum);
    programComposite = linkProgram(vertexShaderQuad, fragmentShaderComposite);

    glGenVertexArrays(1, &vaoTriangle);
    glBindVertexArray(vaoTriangle);
    glGenBuffers(1, &vboTriangle);
    glBindBuffer(GL_ARRAY_BUFFER, vboTriangle);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // Quad VAO + dummy VBO for gl_VertexID
    glGenVertexArrays(1, &vaoQuad);
    glBindVertexArray(vaoQuad);
    static float dummy = 0.0f;
    glGenBuffers(1, &vboQuad);
    glBindBuffer(GL_ARRAY_BUFFER, vboQuad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(dummy), &dummy, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    emscripten_get_canvas_element_size("#canvas", &canvasWidth, &canvasHeight);
    create_oit_framebuffer(canvasWidth, canvasHeight);
}

void perspective(float* m, float fovy, float aspect, float near, float far) {
    float f = 1.0f / tanf(fovy * 3.14159265f / 360.0f);
    m[0] = f / aspect; m[1] = 0; m[2] = 0; m[3] = 0;
    m[4] = 0; m[5] = f; m[6] = 0; m[7] = 0;
    m[8] = 0; m[9] = 0; m[10] = (far + near) / (near - far); m[11] = -1;
    m[12] = 0; m[13] = 0; m[14] = (2*far*near) / (near - far); m[15] = 0;
}

void lookAt(float* m,
            float eyeX, float eyeY, float eyeZ,
            float centerX, float centerY, float centerZ,
            float upX, float upY, float upZ) {
    float f[3] = {centerX - eyeX, centerY - eyeY, centerZ - eyeZ};
    float flen = sqrtf(f[0]*f[0] + f[1]*f[1] + f[2]*f[2]);
    if (flen > 0.00001f) { f[0] /= flen; f[1] /= flen; f[2] /= flen; }
    float up[3] = {upX, upY, upZ};
    float s[3] = {f[1]*up[2] - f[2]*up[1],
                  f[2]*up[0] - f[0]*up[2],
                  f[0]*up[1] - f[1]*up[0]};
    float slen = sqrtf(s[0]*s[0] + s[1]*s[1] + s[2]*s[2]);
    if (slen > 0.00001f) { s[0] /= slen; s[1] /= slen; s[2] /= slen; }
    float u[3] = {s[1]*f[2] - s[2]*f[1],
                  s[2]*f[0] - s[0]*f[2],
                  s[0]*f[1] - s[1]*f[0]};
    m[0] = s[0]; m[4] = s[1]; m[8] = s[2]; m[12] = - (s[0]*eyeX + s[1]*eyeY + s[2]*eyeZ);
    m[1] = u[0]; m[5] = u[1]; m[9] = u[2]; m[13] = - (u[0]*eyeX + u[1]*eyeY + u[2]*eyeZ);
    m[2] = -f[0]; m[6] = -f[1]; m[10] = -f[2]; m[14] = (f[0]*eyeX + f[1]*eyeY + f[2]*eyeZ);
    m[3] = 0; m[7] = 0; m[11] = 0; m[15] = 1;
}

void render() {
    static float angle = 0.0f;
    //angle += 0.01f;
    emscripten_get_canvas_element_size("#canvas", &canvasWidth, &canvasHeight);
    glViewport(0, 0, canvasWidth, canvasHeight);

    // PASS 1 : accumulation dans framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glClearBufferfv(GL_COLOR, 0, (const GLfloat[]){0,0,0,0});
    glClearBufferfv(GL_COLOR, 1, (const GLfloat[]){1.0f}); // important reveal buffer à 1 !

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    glUseProgram(programAccum);
    int projLoc = glGetUniformLocation(programAccum, "uProjection");
    int viewLoc = glGetUniformLocation(programAccum, "uView");
    int angleLoc = glGetUniformLocation(programAccum, "uAngle");
    //int uOffsetLoc = glGetUniformLocation(programAccum, "uOffset");
    float aspect = float(canvasWidth)/float(canvasHeight);
    float projection[16];
    perspective(projection, 45.0f, aspect, 0.1f, 100.0f);
    float view[16];
    lookAt(view, 0, 0, 3, 0, 0, 0, 0, 1, 0);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection);
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, view);
    glUniform1f(angleLoc, angle);

    glBindVertexArray(vaoTriangle);

    //float zOffsets[3] = {-0.4f, 0.0f, 0.4f};

    //for (int i=0; i<3; ++i) {
    //    glUniform1f(zOffsetLoc, zOffsets[i]);
    //    glDrawArrays(GL_TRIANGLES, 0, 3);
    //}

    GLint offsetLoc = glGetUniformLocation(programAccum, "uOffset");
    GLint colorLoc = glGetUniformLocation(programAccum, "uColor");
    struct Vec4 { float r,g,b,a; };
    Vec4 colors[3] = {
        {1.f, 0.f, 0.f, 0.5f},  // rouge transparent
        {0.f, 1.f, 0.f, 0.5f},  // vert transparent
        {0.f, 0.f, 1.f, 0.5f}   // bleu transparent
    };

    // Pour dessiner 3 triangles

    struct Vec2 { float x, y; };
    Vec2 offsets[3] = { {0.0f, -0.3f}, {0.0f, 0.0f}, {0.0f, 0.3f} };

    for (int i = 0; i < 3; i++) {
        glUniform4f(colorLoc, colors[i].r, colors[i].g, colors[i].b, colors[i].a);
        glUniform2f(offsetLoc, offsets[i].x, offsets[i].y);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    //glUniform2f(offsetLoc, offsets[2].x, offsets[2].y);
    //glUniform4f(colorLoc, colors[2].r, colors[2].g, colors[2].b, colors[2].a);
    //glDrawArrays(GL_TRIANGLES,0,3);

    //glUniform2f(offsetLoc, offsets[1].x, offsets[1].y);
    //glUniform4f(colorLoc, colors[1].r, colors[1].g, colors[1].b, colors[1].a);
    //glDrawArrays(GL_TRIANGLES,0,3);

    //glUniform2f(offsetLoc, offsets[0].x, offsets[0].y);
    //glUniform4f(colorLoc, colors[0].r, colors[0].g, colors[0].b, colors[0].a);
    //glDrawArrays(GL_TRIANGLES,0,3);


    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // PASS 2 : composition finale
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(programComposite);
    glBindVertexArray(vaoQuad);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texAccum);
    glUniform1i(glGetUniformLocation(programComposite, "uAccumTex"), 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texReveal);
    glUniform1i(glGetUniformLocation(programComposite, "uRevealTex"), 1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

int main() {
    init();
    emscripten_set_main_loop(render, 0, true);
    return 0;
}
