// Shaders GLSL ES 3.0
const char* vertexShaderSrc = R"(#version 300 es
precision mediump float;

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out float vHeight;
out vec3 vColor;
out vec3 vNormal;
out vec3 vFragPos;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vFragPos = worldPos.xyz;

    vNormal = normalize(mat3(uModel) * aNormal);
    vColor = aColor;
    vHeight = length(aPos);

    gl_Position = uProjection * uView * worldPos;
}
)";

const char* fragmentShaderSrc = R"(#version 300 es
precision mediump float;

uniform float uLvlSea;
uniform vec3 uLightDir;
uniform vec3 uCamPos;

in float vHeight;
in vec3 vColor;
in vec3 vNormal;
in vec3 vFragPos;

out vec4 FragColor;

void main() {
    //FragColor = vec4(color, 1.0);
    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(uLightDir);
    vec3 viewDir = normalize(uCamPos - vFragPos);  // direction vers la caméra

    float diff = max(dot(normal, lightDir), 0.0);

    vec3 reflectDir = reflect(-lightDir, normal);
    // Calcul du facteur spéculaire (exposant shininess = 64 par exemple)
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64.0);

    vec3 ambient = 0.01 * vColor;
    vec3 diffuse = diff * vColor;
    vec3 specular = vec3(spec); // vous pouvez multiplier par la couleur ou intensité de la lumière

    vec3 color = ambient + diffuse;
    if (vHeight - uLvlSea <= 0.0001) {
        color += specular * 0.4;
    }
    else {
        color += specular * 0.05; // Couleur normale si pas dans l'eau
    }
    
    // Pour debug, afficher uniquement diffuse
    FragColor = vec4(color, 1.0);
}
)";


