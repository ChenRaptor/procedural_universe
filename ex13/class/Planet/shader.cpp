// Shaders GLSL ES 3.0
const char* vertexShaderPlanet = R"(#version 300 es
precision mediump float;

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform vec3 uCamPos;
uniform float uLvlSea;

out vec3 vColor;
out vec3 vNormal;
out vec3 vViewVector;
flat out int vIsWater; // booléen passé en flat = pas d'interpolation

void main() {
    mat3 normalMatrix = mat3(uModel);
    vNormal = normalize(normalMatrix * aNormal);
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vViewVector = uCamPos - worldPos.xyz;

    vColor = aColor;

    // Calculé côté vertex, puis passé en flat pour éviter toute interpolation
    float height = length(aPos);
    vIsWater = int(height <= uLvlSea + 0.0001);

    gl_Position = uProjection * uView * worldPos;
}


)";

const char* fragmentShaderPlanet = R"(#version 300 es
precision mediump float;

in vec3 vColor;
in vec3 vNormal;
in vec3 vViewVector;
flat in int vIsWater; // récupéré tel quel

uniform vec3 uLightDir;

out vec4 FragColor;

void main() {
    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(uLightDir);
    vec3 viewDir = normalize(vViewVector);

    float diff = max(dot(normal, lightDir), 0.0);
    //// OPTIMISATION: Early exit si pas de lumière
    //if (diff <= 0.2) {
    //    FragColor = vec4(0.0, 0.0, 0.0, 1.0); // Noir complet
    //    return; // Évite tous les calculs suivants
    //}

    float spec = max(dot(viewDir, reflect(-lightDir, normal)), 0.0);
    spec = spec * spec;
    spec = spec * spec;
    spec = spec * spec;
    spec = spec * spec;

    // Plus besoin de if ou mix, car le flat permet l'utilisation directe de la condition binaire
    float specStrength = vIsWater == 1 ? 1.0 : 0.05;

    vec3 diffuse = diff * vColor;
    vec3 specular = specStrength * spec * vColor;

    vec3 color = diffuse + specular;
    FragColor = vec4(color, 1.0);
}


)";




//const char* fragmentShaderPlanet = R"(#version 300 es
//precision mediump float;

//uniform float uLvlSea;
//uniform vec3 uLightDir;
//uniform vec3 uCamPos;

//in float vHeight;
//in vec3 vColor;
//in vec3 vNormal;
//in vec3 vFragPos;

//out vec4 FragColor;

//// Fonction de hash pour générer des couleurs pseudo-aléatoires
//vec3 hash(vec3 p) {
//    p = vec3(dot(p, vec3(127.1, 311.7, 74.7)),
//             dot(p, vec3(269.5, 183.3, 246.1)),
//             dot(p, vec3(113.5, 271.9, 124.6)));
//    return fract(sin(p) * 43758.5453123);
//}

//void main() {
//    // Calculer l'ID du triangle basé sur la position du fragment
//    // Utiliser les dérivées pour identifier les triangles
//    vec3 dFdxPos = dFdx(vFragPos);
//    vec3 dFdyPos = dFdy(vFragPos);
    
//    // Créer un ID unique par triangle
//    vec3 triangleCenter = vFragPos - mod(vFragPos, 0.1); // Quantifier la position
    
//    // Générer une couleur unique pour ce triangle
//    vec3 triangleColor = hash(triangleCenter);
    
//    // Ajouter un peu d'éclairage pour voir la forme
//    vec3 normal = normalize(vNormal);
//    vec3 lightDir = normalize(uLightDir);
//    float diff = max(dot(normal, lightDir), 0.3); // Minimum d'éclairage ambiant
    
//    // Appliquer l'éclairage à la couleur du triangle
//    vec3 finalColor = triangleColor * diff;
    
//    // Ajouter des bordures noires pour mieux voir les triangles
//    vec2 baryCoord = fract(gl_FragCoord.xy * 0.1);
//    float edge = step(0.95, max(baryCoord.x, baryCoord.y)) + 
//                 step(baryCoord.x, 0.05) + 
//                 step(baryCoord.y, 0.05);
    
//    if (edge > 0.5) {
//        finalColor = vec3(0.0, 0.0, 0.0); // Bordures noires
//    }
    
//    FragColor = vec4(finalColor, 1.0);
//}
//)";

//const char* fragmentShaderPlanet = R"(#version 300 es
//precision mediump float;

//uniform float uLvlSea;
//uniform vec3 uLightDir;
//uniform vec3 uCamPos;

//in float vHeight;
//in vec3 vColor;
//in vec3 vNormal;
//in vec3 vFragPos;

//out vec4 FragColor;

//void main() {
//    // Calculer les dérivées de la position dans l'espace écran
//    vec3 dFdxPos = dFdx(vFragPos);
//    vec3 dFdyPos = dFdy(vFragPos);
    
//    // Calculer la taille approximative du triangle en pixels
//    float triangleSize = length(dFdxPos) + length(dFdyPos);
    
//    // Classifier la taille
//    vec3 color;
    
//    if (triangleSize < 0.001) {
//        // Triangle TRÈS petit (sub-pixel)
//        color = vec3(1.0, 0.0, 0.0); // Rouge = trop petit !
//    }
//    else if (triangleSize < 0.01) {
//        // Triangle petit (1-2 pixels)
//        color = vec3(1.0, 0.5, 0.0); // Orange = petit
//    }
//    else if (triangleSize < 0.1) {
//        // Triangle taille normale (5-20 pixels)
//        color = vec3(0.0, 1.0, 0.0); // Vert = optimal
//    }
//    else if (triangleSize < 0.5) {
//        // Triangle moyen (20-100 pixels)
//        color = vec3(0.0, 0.0, 1.0); // Bleu = moyen
//    }
//    else {
//        // Triangle très grand (100+ pixels)
//        color = vec3(1.0, 1.0, 0.0); // Jaune = très grand
//    }
    
//    // Ajouter un peu d'éclairage pour voir la forme
//    vec3 normal = normalize(vNormal);
//    vec3 lightDir = normalize(uLightDir);
//    float diff = max(dot(normal, lightDir), 0.3);
    
//    FragColor = vec4(color * diff, 1.0);
//}
//)";