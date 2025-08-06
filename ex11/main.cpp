#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#include <cstdio>
#include <cmath>

// Vertex shader simple (avec position, couleur uniforme, rotation, offset 2D)
const char* vertexShaderSrc = R"(#version 300 es
precision mediump float;
layout(location = 0) in vec3 aPos;
uniform float uAngle;
uniform mat4 uProjection;
uniform mat4 uView;
uniform vec2 uOffset;  // offset X et Y
out vec4 vColor;
uniform vec4 uColor;  // couleur RGBA uniforme
void main() {
    float c = cos(uAngle);
    float s = sin(uAngle);
    vec3 pos = aPos;
    float x = pos.x * c - pos.y * s + uOffset.x;
    float y = pos.x * s + pos.y * c + uOffset.y;
    vec4 rotatedPos = vec4(x, y, pos.z, 1.0);
    gl_Position = uProjection * uView * rotatedPos;
    vColor = uColor;
}
)";

// Fragment shader simple (sortie couleur attribuée)
const char* fragmentShaderSrc = R"(#version 300 es
precision mediump float;
in vec4 vColor;
out vec4 FragColor;
void main() {
    FragColor = vColor;
}
)";

// Triangle positions (x,y,z)
const float vertices[] = {
     0.0f,  0.577f, 0.0f,
    -0.5f, -0.288f, 0.0f,
     0.5f, -0.288f, 0.0f,
};

GLuint program;
GLuint vaoTriangle, vboTriangle;
int canvasWidth = 800, canvasHeight = 600;

GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint status; glGetShaderiv(s, GL_COMPILE_STATUS, &status);
    if (!status) {
        char buf[512];
        glGetShaderInfoLog(s, 512, NULL, buf);
        printf("Shader error: %s\n", buf);
        return 0;
    }
    return s;
}

GLuint linkProgram(const char* vsSrc, const char* fsSrc) {
    GLuint v = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!v || !f) return 0;
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    GLint status; glGetProgramiv(p, GL_LINK_STATUS, &status);
    if (!status) {
        char buf[512];
        glGetProgramInfoLog(p, 512, NULL, buf);
        printf("Program link error: %s\n", buf);
        return 0;
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

void init_webgl_context() {
    EmscriptenWebGLContextAttributes attr;
    emscripten_webgl_init_context_attributes(&attr);
    attr.majorVersion = 2;
    attr.minorVersion = 0;
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context("#canvas", &attr);
    if (ctx <= 0) { printf("Failed to create GL context\n"); exit(1); }
    emscripten_webgl_make_context_current(ctx);
}

void perspective(float* m, float fovy, float aspect, float near, float far) {
    float f = 1.f / tanf(fovy * 3.14159265f / 360.f);
    m[0] = f / aspect; m[1] = 0; m[2] = 0; m[3] = 0;
    m[4] = 0; m[5] = f; m[6] = 0; m[7] = 0;
    m[8] = 0; m[9] = 0; m[10] = (far+near)/(near - far); m[11] = -1;
    m[12] = 0; m[13] = 0; m[14] = (2*far*near)/(near - far); m[15] = 0;
}

void lookAt(float* m, float eyeX, float eyeY, float eyeZ,
            float centerX, float centerY, float centerZ,
            float upX, float upY, float upZ){
    float f[3] = {centerX - eyeX, centerY - eyeY, centerZ - eyeZ};
    float flen = sqrtf(f[0]*f[0]+f[1]*f[1]+f[2]*f[2]);
    if (flen > 0.00001f) {f[0]/=flen; f[1]/=flen; f[2]/=flen;}
    float up[3] = {upX, upY, upZ};
    float s[3] = {f[1]*up[2]-f[2]*up[1], f[2]*up[0]-f[0]*up[2], f[0]*up[1]-f[1]*up[0]};
    float slen = sqrtf(s[0]*s[0]+s[1]*s[1]+s[2]*s[2]);
    if (slen > 0.00001f) {s[0]/=slen;s[1]/=slen;s[2]/=slen;}
    float u[3] = {s[1]*f[2]-s[2]*f[1], s[2]*f[0]-s[0]*f[2], s[0]*f[1]-s[1]*f[0]};
    m[0]=s[0]; m[4]=s[1]; m[8]=s[2]; m[12]=-(s[0]*eyeX+s[1]*eyeY+s[2]*eyeZ);
    m[1]=u[0]; m[5]=u[1]; m[9]=u[2]; m[13]=-(u[0]*eyeX+u[1]*eyeY+u[2]*eyeZ);
    m[2]=-f[0]; m[6]=-f[1]; m[10]=-f[2]; m[14]=(f[0]*eyeX+f[1]*eyeY+f[2]*eyeZ);
    m[3]=0; m[7]=0; m[11]=0; m[15]=1;
}

void init(){
    init_webgl_context();
            double width, height;
    emscripten_get_element_css_size("#canvas", &width, &height);
    int dpr = (int)emscripten_get_device_pixel_ratio();
    int canvasWidth = (int)(width * dpr);
    int canvasHeight = (int)(height * dpr);
    emscripten_set_canvas_element_size("#canvas", canvasWidth, canvasHeight);
    program = linkProgram(vertexShaderSrc, fragmentShaderSrc);

    glGenVertexArrays(1,&vaoTriangle);
    glBindVertexArray(vaoTriangle);
    glGenBuffers(1,&vboTriangle);
    glBindBuffer(GL_ARRAY_BUFFER,vboTriangle);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void render(){
    static float angle = 0.0f;
    //angle += 0.01f;

    emscripten_get_canvas_element_size("#canvas",&canvasWidth,&canvasHeight);
    glViewport(0,0,canvasWidth,canvasHeight);

    glClearColor(0.0,0.0,0.0,1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(program);

    int projLoc = glGetUniformLocation(program,"uProjection");
    int viewLoc = glGetUniformLocation(program,"uView");
    int angleLoc = glGetUniformLocation(program,"uAngle");
    int offsetLoc = glGetUniformLocation(program,"uOffset");
    int colorLoc = glGetUniformLocation(program,"uColor");

    float aspect = float(canvasWidth)/float(canvasHeight);
    float projection[16];
    perspective(projection,45.0f,aspect,0.1f,100.0f);
    float view[16];
    lookAt(view,0,0,3,0,0,0,0,1,0);

    glUniformMatrix4fv(projLoc,1,GL_FALSE,projection);
    glUniformMatrix4fv(viewLoc,1,GL_FALSE,view);
    glUniform1f(angleLoc,angle);

    glBindVertexArray(vaoTriangle);

    struct Vec2 {float x,y;};
    struct Vec4 {float r,g,b,a;};
    Vec2 offsets[3] = { {0.0f,-0.3f}, {0.0f,0.0f}, {0.0f,0.3f}};
    Vec4 colors[3] = {
        {1.f,0.f,0.f,0.5f},
        {0.f,1.f,0.f,0.5f},
        {0.f,0.f,1.f,0.5f}
    };

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //for(int i=3;i>0;i--){
    //    glUniform2f(offsetLoc, offsets[i].x, offsets[i].y);
    //    glUniform4f(colorLoc, colors[i].r, colors[i].g, colors[i].b, colors[i].a);
    //    glDrawArrays(GL_TRIANGLES,0,3);
    //}

    glUniform2f(offsetLoc, offsets[2].x, offsets[2].y);
    glUniform4f(colorLoc, colors[2].r, colors[2].g, colors[2].b, colors[2].a);
    glDrawArrays(GL_TRIANGLES,0,3);

    glUniform2f(offsetLoc, offsets[1].x, offsets[1].y);
    glUniform4f(colorLoc, colors[1].r, colors[1].g, colors[1].b, colors[1].a);
    glDrawArrays(GL_TRIANGLES,0,3);

    glUniform2f(offsetLoc, offsets[0].x, offsets[0].y);
    glUniform4f(colorLoc, colors[0].r, colors[0].g, colors[0].b, colors[0].a);
    glDrawArrays(GL_TRIANGLES,0,3);


    //glUniform2f(offsetLoc, offsets[0].x, offsets[0].y);
    //glUniform4f(colorLoc, colors[0].r, colors[0].g, colors[0].b, colors[0].a);
    //glDrawArrays(GL_TRIANGLES,0,3);

    //glUniform2f(offsetLoc, offsets[1].x, offsets[1].y);
    //glUniform4f(colorLoc, colors[1].r, colors[1].g, colors[1].b, colors[1].a);
    //glDrawArrays(GL_TRIANGLES,0,3);

    //glUniform2f(offsetLoc, offsets[2].x, offsets[2].y);
    //glUniform4f(colorLoc, colors[2].r, colors[2].g, colors[2].b, colors[2].a);
    //glDrawArrays(GL_TRIANGLES,0,3);

    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

int main(){
    init();
    emscripten_set_main_loop(render,0,true);
    return 0;
}
