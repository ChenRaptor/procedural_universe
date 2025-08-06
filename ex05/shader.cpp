// Shaders GLSL ES 3.0
const char* vertexShaderPlanet = R"(#version 300 es
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
out vec3 vFragPos;

void main() {
    float c = cos(uAngle);
    float s = sin(uAngle);
    vec3 pos = aPos;

    float x = pos.x * c + pos.z * s;
    float y = pos.y;
    float z = -pos.x * s + pos.z * c;
    vec3 rotatedPos = vec3(x, y, z);
    vFragPos = rotatedPos;

    gl_Position = uProjection * uView * vec4(rotatedPos, 1.0);

    vHeight = length(pos);
    vColor = aColor;

    // Appliquer la même rotation sur la normale
    float nx = aNormal.x * c - aNormal.z * s;
    float ny = aNormal.y;
    float nz = -aNormal.x * s + aNormal.z * c;

    vNormal = normalize(vec3(nx, ny, nz));

    //vNormal = aNormal;
}
)";

const char* fragmentShaderPlanet = R"(#version 300 es
precision mediump float;

uniform float uLvlSea;
uniform vec3 uLightDir;  // direction lumière normalisée
uniform vec3 uCamPos;   // position de la caméra

in float vHeight;
in vec3 vColor;
in vec3 vNormal;
out vec4 FragColor;
in vec3 vFragPos;

void main() {

    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(uLightDir);

    float diff = max(dot(normal, lightDir), 0.0);

    // Couleur éclairée diffuse + un ambiant faible
    vec3 ambient = 0.3 * vColor;
    vec3 diffuse = diff * vColor;

    vec3 color = ambient + diffuse;

    // Calcul du spéculaire simple (vue caméra = -position, à adapter)
    vec3 fragToCamera = normalize(uCamPos - vFragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(fragToCamera, reflectDir), 0.0), 64.0);

    // Amplifie le spéculaire pour l'océan
    if (vHeight - uLvlSea <= 0.0001) {
        color += vec3(spec) * 1.2;
    }


    //FragColor = vec4(normalize(vNormal) * 0.5 + 0.5, 1.0);
    FragColor = vec4(color, 1.0);
}
)";