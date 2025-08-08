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

vec4 permute(vec4 x) {
    return mod(((x * 34.0) + 1.0) * x, 289.0);
}

float grad(vec3 ip, vec3 fp) {
    vec4 p = permute(permute(permute(
        ip.x + vec4(0.0, 1.0, 0.0, 1.0))
        + ip.y + vec4(0.0, 0.0, 1.0, 1.0))
        + ip.z);
    
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

float cnoise(vec3 P) {
    vec3 Pi0 = floor(P);
    vec3 Pf0 = fract(P);
    vec3 Pf1 = Pf0 - 1.0;
    vec3 fade_xyz = fade(Pf0);
    
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
    
    return mix(nxy0, nxy1, fade_xyz.z);
}

// Fonction FBM (Fractal Brownian Motion)
float fbm(vec3 pos, float scale, float persistence, int octaves) {
    float value = 0.0;
    float amplitude = 1.0;
    float frequency = scale;
    float maxValue = 0.0;
    
    for (int i = 0; i < 8; i++) { // Maximum 8 octaves pour éviter les boucles infinies
        if (i >= octaves) break;
        
        value += amplitude * cnoise(pos * frequency);
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= 2.0;
    }
    
    return value / maxValue; // Normalise entre [-1, 1]
}

void main() {
    // Coordonnée bruit dans l'espace 3D, on applique translation temporelle pour animer
    vec3 posNoise = vPosWorld * 3.0 + vec3(uTime * 0.1, uTime * 0.1, uTime * 0.1);
    
    float noiseVal1 = fbm(posNoise, 1.0, 0.6, 4);
    
    float noise01 = noiseVal1 * 0.5 + 0.5;

    float baseAlpha = 0.8;
    float alpha = baseAlpha * smoothstep(0.0, 1.0, noise01);

    //float alpha = vColor.a;
    float weight = clamp(pow(min(1.0, alpha * 10.0) + 0.01, 3.0), 0.01, 1.0);

    outAccum = vec4(vColor.rgb * alpha, alpha) * weight;
    outReveal = (1.0 - alpha) * weight;
}
)";
