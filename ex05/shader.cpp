// Shaders GLSL ES 3.0
const char* vertexShaderSrc = R"(#version 300 es
precision mediump float;

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec3 aNormal;    // <-- ajout

uniform float uAngle;
uniform mat4 uProjection;
uniform mat4 uView;

out float vHeight;
out vec3 vColor;
out vec3 vNormal;                        // <-- ajout

void main() {
    float c = cos(uAngle);
    float s = sin(uAngle);
    vec3 pos = aPos;

    float x = pos.x * c - pos.y * s;
    float y = pos.x * s + pos.y * c;
    vec3 rotatedPos = vec3(x, y, pos.z);

    gl_Position = uProjection * uView * vec4(rotatedPos, 1.0);

    vHeight = length(pos);
    vColor = aColor;

    // Appliquer la même rotation sur la normale
    float nx = aNormal.x * c - aNormal.y * s;
    float ny = aNormal.x * s + aNormal.y * c;
    float nz = aNormal.z;

    vNormal = normalize(vec3(nx, ny, nz));

    //vNormal = aNormal;
}
)";

const char* fragmentShaderSrc = R"(#version 300 es
precision mediump float;

uniform float uLvlSea;
uniform vec3 uLightDir;  // direction lumière normalisée

in float vHeight;
in vec3 vColor;
in vec3 vNormal;
out vec4 FragColor;

void main() {

    // Seuils définissant les zones de hauteur (modifiable selon ta scène)
    float mountainStart = 1.01;
    float mountainPeak = 1.04;

    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(uLightDir);

    float diff = max(dot(normal, lightDir), 0.0);

    // Couleur éclairée diffuse + un ambiant faible
    vec3 ambient = 0.3 * vColor;
    vec3 diffuse = diff * vColor;

    vec3 color = ambient + diffuse;

    if (vHeight < uLvlSea + 0.01) {
        color = vec3(0.0, 0.0, 0.3 + 0.2 * (uLvlSea + 0.01 - vHeight) * 50.0);
    }

    //FragColor = vec4(normalize(vNormal) * 0.5 + 0.5, 1.0);
    FragColor = vec4(color, 1.0);
}
)";