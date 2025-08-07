// Vertex shader commun (position + couleur RGBA)
const char* vertexShaderAtmosphere = R"(#version 300 es
precision mediump float;
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vPosWorld;   // position monde du fragment
out vec4 vColor;
void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vPosWorld = worldPos.xyz;
    //vNormal = normalize(mat3(uModel) * aNormal);
    vColor = vec4(aColor, 0.4); // Couleur RGBA
    gl_Position = uProjection * uView * worldPos;
}
)";



// Fragment shader d'accumulation weighted blended OIT
const char* fragmentShaderAtmosphere = R"(#version 300 es
precision mediump float;
in vec3 vPosWorld;
in vec4 vColor;

uniform float uTime;

layout(location = 0) out vec4 outAccum;
layout(location = 1) out float outReveal;

vec3 fade(vec3 t) {
    return t*t*t*(t*(t*6.0 - 15.0) + 10.0);
}

// Permutation pseudo aléatoire
vec4 permute(vec4 x) {
    return mod(((x * 34.0) + 1.0) * x, 289.0);
}

// Inverse sqrt pour normalisation
vec4 taylorInvSqrt(vec4 r) {
    return 1.79284291400159 - 0.85373472095314 * r;
}

// Gradients 3D
float grad(vec3 ip, vec3 fp) {
    vec4 p = permute(permute(permute(
        ip.x + vec4(0.0, 1.0, 0.0, 1.0))
        + ip.y + vec4(0.0, 0.0, 1.0, 1.0))
        + ip.z);
    
    // Transforme les hashes en vecteurs grad (voir Perlin noise standard)
    // Implementation simplifiée:
    vec4 gx = fract(p * (1.0 / 41.0)) * 2.0 - 1.0;
    vec4 gy = abs(gx) - 0.5;
    vec4 tx = floor(gx + 0.5);
    gx = gx - tx;
    
    vec3 g0 = vec3(gx.x, gy.x, gx.y);
    vec3 g1 = vec3(gx.z, gy.z, gx.w);
    
    float d0 = dot(g0, fp);
    float d1 = dot(g1, fp);
    
    return mix(d0, d1, 0.5);
}

// Perlin noise 3D de base (simplifié pour cette illustration)
float cnoise(vec3 P) {
    vec3 Pi0 = floor(P); // Partie entière (entiers du lattice)
    vec3 Pf0 = fract(P); // Partie fractionnelle
    vec3 Pf1 = Pf0 - 1.0;
    vec3 fade_xyz = fade(Pf0);
    // Calcule les gradients et interpolations
    float n000 = grad(Pi0, Pf0);
    float n100 = grad(Pi0 + vec3(1.0, 0.0, 0.0), vec3(Pf1.x, Pf0.y, Pf0.z));
    float n010 = grad(Pi0 + vec3(0.0, 1.0, 0.0), vec3(Pf0.x, Pf1.y, Pf0.z));
    float n110 = grad(Pi0 + vec3(1.0, 1.0, 0.0), vec3(Pf1.x, Pf1.y, Pf0.z));
    float n001 = grad(Pi0 + vec3(0.0, 0.0, 1.0), vec3(Pf0.x, Pf0.y, Pf1.z));
    float n101 = grad(Pi0 + vec3(1.0, 0.0, 1.0), vec3(Pf1.x, Pf0.y, Pf1.z));
    float n011 = grad(Pi0 + vec3(0.0, 1.0, 1.0), vec3(Pf0.x, Pf1.y, Pf1.z));
    float n111 = grad(Pi0 + vec3(1.0, 1.0, 1.0), Pf1);
    
    float nx00 = mix(n000, n100, fade_xyz.x);
    float nx01 = mix(n001, n101, fade_xyz.x);
    float nx10 = mix(n010, n110, fade_xyz.x);
    float nx11 = mix(n011, n111, fade_xyz.x);
    
    float nxy0 = mix(nx00, nx10, fade_xyz.y);
    float nxy1 = mix(nx01, nx11, fade_xyz.y);
    
    float nxyz = mix(nxy0, nxy1, fade_xyz.z);
    
    return nxyz;
}

void main() {
    // Coordonnée bruit dans l'espace 3D, on applique translation temporelle pour animer
    vec3 posNoise = vPosWorld * 3.0 + vec3(uTime * 10.0, uTime * 0.1, uTime * 0.1);
    
    // Calcule de la valeur de bruit Perlin dans [-1,1]
    float noiseVal = cnoise(posNoise);
    
    // Map noiseVal en [0,1]
    float noise01 = noiseVal * 0.5 + 0.5;
    
    // Modulate alpha (transparence) par le bruit (plus dense dans les zones hautes)
    float baseAlpha = 0.4;
    float alpha = baseAlpha * smoothstep(0.4, 0.7, noise01); 

    //float alpha = vColor.a;
    float weight = clamp(pow(min(1.0, alpha * 10.0) + 0.01, 3.0), 0.01, 1.0);

    outAccum = vec4(vColor.rgb * alpha, alpha) * weight;
    outReveal = (1.0 - alpha) * weight;
}
)";
