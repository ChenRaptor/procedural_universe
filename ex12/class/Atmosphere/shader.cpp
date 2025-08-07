//const char* vertexShaderAtmosphere = R"(#version 300 es
//precision highp float;

//layout(location = 0) in vec3 aPos;
//layout(location = 1) in vec3 aColor;
//layout(location = 2) in vec3 aNormal;

//uniform mat4 uModel;
//uniform mat4 uView;
//uniform mat4 uProjection;

//out vec3 vNormal;
//out vec3 vFragPos;
//out vec3 vColor;

//void main() {
//    vec4 worldPos = uModel * vec4(aPos, 1.0);
//    vFragPos = worldPos.xyz;
//    vNormal = normalize(mat3(uModel) * aNormal);
//    //vNormal = normalize(mat3(uModel) * aNormal);
//    vColor = aColor;
//    gl_Position = uProjection * uView * worldPos;
//}
//)";

//    //vec4 worldPos = uModel * vec4(aPos, 1.0);
//    //vFragPos = worldPos.xyz;

//    //vNormal = normalize(mat3(uModel) * aNormal);
//    //vColor = aColor;
//    //vHeight = length(aPos);

//    //gl_Position = uProjection * uView * worldPos;

////const char* vertexShaderAtmosphere = R"(#version 300 es
////precision mediump float;

////layout(location = 0) in vec3 aPos;
////layout(location = 1) in vec3 aColor;
////layout(location = 2) in vec3 aNormal;

////uniform mat4 uModel;
////uniform mat4 uView;
////uniform mat4 uProjection;

////out vec3 vWorldPos;
////out vec3 vNormal;

////void main() {
////    vec4 worldPos = uModel * vec4(aPos, 1.0);
////    vWorldPos = worldPos.xyz;
////    vNormal = normalize(mat3(uModel) * aNormal);
////    gl_Position = uProjection * uView * worldPos;
////}
////)";

//const char* fragmentShaderAtmosphere = R"(#version 300 es
//precision highp float;

//in vec3 vNormal;
//in vec3 vFragPos;
//in vec3 vColor;

//uniform vec3 uCamPos;
////uniform float uTime;

//out vec4 fragColor;

////// Fonction fade de Ken Perlin
////float fade(float t) {
////    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
////}

////// Fonction de gradient pseudo-aléatoire
////float grad(int hash, vec3 p) {
////    int h = hash & 15;
////    float u = h < 8 ? p.x : p.y;
////    float v = h < 4 ? p.y : (h == 12 || h == 14 ? p.x : p.z);
////    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
////}

////// Table de permutation simple (répétée)
////int perm[256] = int[256](151,160,137,91,90,15, //... et 250 valeurs connues à définir)
////// Pour simplifier ici on simule avec un hash fonctionnel
////;

////// Perlin Noise 3D (simplifié sans permutation table complète, à améliorer)
////float perlinNoise(vec3 p) {
////    vec3 pi = floor(p);
////    vec3 pf = fract(p);
////    vec3 w = vec3(fade(pf.x), fade(pf.y), fade(pf.z));

////    // Hash corners (simplifié pour l'exemple) :
////    int A = int(mod(pi.x, 256.0));
////    int AA = int(mod(A + pi.y, 256.0));
////    int AB = int(mod(A + pi.y + 1.0, 256.0));
////    int B = int(mod(pi.x + 1.0, 256.0));
////    int BA = int(mod(B + pi.y, 256.0));
////    int BB = int(mod(B + pi.y + 1.0, 256.0));

////    float lerpX1 = mix(grad(AA, pf), grad(BA, pf - vec3(1,0,0)), w.x);
////    float lerpX2 = mix(grad(AB, pf - vec3(0,1,0)), grad(BB, pf - vec3(1,1,0)), w.x);
////    float lerpY1 = mix(lerpX1, lerpX2, w.y);

////    // On ignore Z ici pour simplifier, tu peux compléter pour la vraie 3D

////    return lerpY1;
////}

//void main() {
//    // Calcul simple : l’intensité dépend de l’angle vue/normale (effet enveloppe/halo)
//    vec3 viewDir = normalize(uCamPos - vFragPos);
//    vec3 normal = faceforward(vNormal, normalize(vFragPos - uCamPos), vNormal);
//    float facing = dot(viewDir, vNormal);

//    float intensity = smoothstep(0.2, 1.0, 1.0 - facing); // Plus brillant sur la tranche

//    //intensity = clamp(intensity, 0.0, 1.0); // Assure que l’intensité est entre 0 et 1

//    //float alpha = 0.18 + 0.25 * intensity; // Transparence adaptée

//    //alpha = clamp(alpha, 0.0, 1.0); // Assure que alpha est entre 0 et 1

//    //vec3 color = mix(vec3(0.3, 0.6, 1.0), vec3(0.1, 0.2, 0.9), intensity); // Bleu-gris subtil

//    //float noiseVal = perlinNoise(vec3(p.xy, uTime * 0.1));

//    //fragColor = vec4(intensity, intensity, intensity, 1.0);
//    //FragColor = vec4(vec3((facing + 1.0) * 0.5), 1.0); // pour voir les variations [-1, 1]
//    fragColor = vec4(vNormal, 0.4); // Couleur de l’atmosphère avec alpha fixe
//}
//)";


//const char* fragmentShaderAtmosphere = R"(#version 300 es
//precision mediump float;

//in vec3 vWorldPos;
//in vec3 vNormal;

//uniform float uTime;
//uniform vec3 uCamPos;

//out vec4 fragColor;

// Fonction fade de Ken Perlin
//float fade(float t) {
//    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
//}

//// Fonction de gradient pseudo-aléatoire
//float grad(int hash, vec3 p) {
//    int h = hash & 15;
//    float u = h < 8 ? p.x : p.y;
//    float v = h < 4 ? p.y : (h == 12 || h == 14 ? p.x : p.z);
//    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
//}

//// Table de permutation simple (répétée)
//int perm[256] = int[256](151,160,137,91,90,15, //... et 250 valeurs connues à définir)
//// Pour simplifier ici on simule avec un hash fonctionnel
//;

//// Perlin Noise 3D (simplifié sans permutation table complète, à améliorer)
//float perlinNoise(vec3 p) {
//    vec3 pi = floor(p);
//    vec3 pf = fract(p);
//    vec3 w = vec3(fade(pf.x), fade(pf.y), fade(pf.z));

//    // Hash corners (simplifié pour l'exemple) :
//    int A = int(mod(pi.x, 256.0));
//    int AA = int(mod(A + pi.y, 256.0));
//    int AB = int(mod(A + pi.y + 1.0, 256.0));
//    int B = int(mod(pi.x + 1.0, 256.0));
//    int BA = int(mod(B + pi.y, 256.0));
//    int BB = int(mod(B + pi.y + 1.0, 256.0));

//    float lerpX1 = mix(grad(AA, pf), grad(BA, pf - vec3(1,0,0)), w.x);
//    float lerpX2 = mix(grad(AB, pf - vec3(0,1,0)), grad(BB, pf - vec3(1,1,0)), w.x);
//    float lerpY1 = mix(lerpX1, lerpX2, w.y);

//    // On ignore Z ici pour simplifier, tu peux compléter pour la vraie 3D

//    return lerpY1;
//}

//void main() {
//    // Coordonnées du nuage sur la sphère, multipliées par échelle
//    //vec3 p = vWorldPos * 3.0;

//    //// Intégrer le temps à la 4ème dimension pour animer le bruit
//    //float noiseVal = perlinNoise(vec3(p.xy, uTime * 0.1));

//    //// Seuil pour les nuages : lisse la transition
//    //float cloud = smoothstep(0.5, 0.7, noiseVal);

//    //// Couleur de base de l’atmosphère (un bleu clair)
//    //vec3 baseColor = vec3(0.3, 0.5, 1.0);

//    //// Couleur des nuages (blanc casse)
//    //vec3 cloudColor = vec3(1.0, 0.95, 0.85);

//    //// Mélange couleur nuage et base selon la densité cloud
//    //vec3 color = mix(baseColor, cloudColor, cloud);

//    //// Alpha dépend aussi de la densité + un léger fade pour bords
//    //float alpha = cloud * 0.5;

//    fragColor = vec4(color, 0.4);
//}
//)";


// Vertex shader commun (position + couleur RGBA)
const char* vertexShaderAtmosphere = R"(#version 300 es
precision mediump float;
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec4 vColor;
void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    //vFragPos = worldPos.xyz;
    //vNormal = normalize(mat3(uModel) * aNormal);
    vColor = vec4(aColor, 0.4); // Couleur RGBA
    gl_Position = uProjection * uView * worldPos;
}
)";

// Fragment shader d'accumulation weighted blended OIT
const char* fragmentShaderAtmosphere = R"(#version 300 es
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