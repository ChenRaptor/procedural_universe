// Vertex shader commun (position + couleur RGBA)
const char* vertexShaderAtmosphere = R"(#version 300 es
precision mediump float;
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform vec3 uCamPos;

out vec3 vPosWorld;   // position monde du fragment
out vec4 vColor;
out vec3 vNormal;    // normal du fragment
out vec3 vViewDir;
void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vPosWorld = worldPos.xyz;
    vNormal = normalize(mat3(uModel) * aNormal);
    vColor = vec4(aColor, 0.4); // Couleur RGBA
    vViewDir = uCamPos - worldPos.xyz;
    gl_Position = uProjection * uView * worldPos;
}
)";

// Fragment shader d'accumulation weighted blended OIT
const char* fragmentShaderAtmosphere = R"(#version 300 es
precision mediump float;
in vec3 vPosWorld;
in vec4 vColor;
in vec3 vNormal;
in vec3 vViewDir;

uniform vec3 uLightDir;
uniform float uTime;
uniform vec3 uCamPos;

layout(location = 0) out vec4 outAccum;
layout(location = 1) out float outReveal;

// === PERLIN NOISE ===
vec3 fade(vec3 t) {
    return t*t*t*(t*(t*6.0 - 15.0) + 10.0);
}

vec4 permute(vec4 x) {
    return mod(((x * 34.0) + 1.0) * x, 289.0);
}

float cnoise2(vec3 P) {
    vec3 Pi0 = floor(P);
    vec3 Pf0 = fract(P);
    vec3 Pf1 = Pf0 - 1.0;
    vec3 fade_xyz = fade(Pf0);
    
	vec4 ix = Pi0.x + vec4(0.0, 1.0, 0.0, 1.0);
	vec4 iy = Pi0.y + vec4(0.0, 0.0, 1.0, 1.0);
	vec4 iz0 = vec4(Pi0.z);
	vec4 iz1 = vec4(Pi0.z + 1.0);
    
    vec4 ixy = permute(permute(ix) + iy);
    vec4 ixy0 = permute(ixy + iz0);
    vec4 ixy1 = permute(ixy + iz1);
    
    vec4 gx0 = ixy0 * (1.0 / 7.0);
    vec4 gy0 = fract(floor(gx0) * (1.0 / 7.0)) - 0.5;
    gx0 = fract(gx0);
    vec4 gz0 = vec4(0.5) - abs(gx0) - abs(gy0);
    vec4 sz0 = step(gz0, vec4(0.0));
    gx0 -= sz0 * (step(0.0, gx0) - 0.5);
    gy0 -= sz0 * (step(0.0, gy0) - 0.5);
    
    vec4 gx1 = ixy1 * (1.0 / 7.0);
    vec4 gy1 = fract(floor(gx1) * (1.0 / 7.0)) - 0.5;
    gx1 = fract(gx1);
    vec4 gz1 = vec4(0.5) - abs(gx1) - abs(gy1);
    vec4 sz1 = step(gz1, vec4(0.0));
    gx1 -= sz1 * (step(0.0, gx1) - 0.5);
    gy1 -= sz1 * (step(0.0, gy1) - 0.5);
    
    vec3 g000 = vec3(gx0.x, gy0.x, gz0.x);
    vec3 g100 = vec3(gx0.y, gy0.y, gz0.y);
    vec3 g010 = vec3(gx0.z, gy0.z, gz0.z);
    vec3 g110 = vec3(gx0.w, gy0.w, gz0.w);
    vec3 g001 = vec3(gx1.x, gy1.x, gz1.x);
    vec3 g101 = vec3(gx1.y, gy1.y, gz1.y);
    vec3 g011 = vec3(gx1.z, gy1.z, gz1.z);
    vec3 g111 = vec3(gx1.w, gy1.w, gz1.w);
    
    vec4 norm0 = 1.79284291400159 - 0.85373472095314 * vec4(dot(g000, g000), dot(g010, g010), dot(g100, g100), dot(g110, g110));
    vec4 norm1 = 1.79284291400159 - 0.85373472095314 * vec4(dot(g001, g001), dot(g011, g011), dot(g101, g101), dot(g111, g111));
    
    g000 *= norm0.x;
    g010 *= norm0.y;
    g100 *= norm0.z;
    g110 *= norm0.w;
    g001 *= norm1.x;
    g011 *= norm1.y;
    g101 *= norm1.z;
    g111 *= norm1.w;
    
    float n000 = dot(g000, Pf0);
    float n100 = dot(g100, vec3(Pf1.x, Pf0.y, Pf0.z));
    float n010 = dot(g010, vec3(Pf0.x, Pf1.y, Pf0.z));
    float n110 = dot(g110, vec3(Pf1.x, Pf1.y, Pf0.z));
    float n001 = dot(g001, vec3(Pf0.x, Pf0.y, Pf1.z));
    float n101 = dot(g101, vec3(Pf1.x, Pf0.y, Pf1.z));
    float n011 = dot(g011, vec3(Pf0.x, Pf1.y, Pf1.z));
    float n111 = dot(g111, Pf1);

    vec4 fade_xyxy = vec4(fade_xyz.x, fade_xyz.y, fade_xyz.x, fade_xyz.y);
    vec2 fade_zw = vec2(fade_xyz.z, fade_xyz.z);
    
    vec4 n_x = mix(vec4(n000, n001, n010, n011), vec4(n100, n101, n110, n111), fade_xyxy.x);
    vec2 n_y = mix(n_x.xy, n_x.zw, fade_xyxy.y);
    float n = mix(n_y.x, n_y.y, fade_zw.x);
    return 2.2 * n;
}

// === WORLEY NOISE ===
vec3 hash(vec3 p) {
    p = vec3(dot(p, vec3(127.1, 311.7, 74.7)),
             dot(p, vec3(269.5, 183.3, 246.1)),
             dot(p, vec3(113.5, 271.9, 124.6)));
    return fract(sin(p) * 43758.5453123);
}

float worley(vec3 p) {
    vec3 id = floor(p);
    vec3 f = fract(p);
    
    float minDist = 1.0;
    
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            for (int z = -1; z <= 1; z++) {
                vec3 neighbor = vec3(float(x), float(y), float(z));
                vec3 point = hash(id + neighbor);
                vec3 diff = neighbor + point - f;
                float dist = length(diff);
                minDist = min(minDist, dist);
            }
        }
    }
    
    return minDist;
}

// === SIMPLEX NOISE ===
float simplex(vec3 v) {
    const vec2 C = vec2(1.0/6.0, 1.0/3.0);
    const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);
    
    vec3 i = floor(v + dot(v, C.yyy));
    vec3 x0 = v - i + dot(i, C.xxx);
    
    vec3 g = step(x0.yzx, x0.xyz);
    vec3 l = 1.0 - g;
    vec3 i1 = min(g.xyz, l.zxy);
    vec3 i2 = max(g.xyz, l.zxy);
    
    vec3 x1 = x0 - i1 + C.xxx;
    vec3 x2 = x0 - i2 + C.yyy;
    vec3 x3 = x0 - D.yyy;
    
    i = mod(i, 289.0);
    vec4 p = permute(permute(permute(
        i.z + vec4(0.0, i1.z, i2.z, 1.0))
        + i.y + vec4(0.0, i1.y, i2.y, 1.0))
        + i.x + vec4(0.0, i1.x, i2.x, 1.0));
    
    float n_ = 0.142857142857;
    vec3 ns = n_ * D.wyz - D.xzx;
    
    vec4 j = p - 49.0 * floor(p * ns.z * ns.z);
    
    vec4 x_ = floor(j * ns.z);
    vec4 y_ = floor(j - 7.0 * x_);
    
    vec4 x = x_ * ns.x + ns.yyyy;
    vec4 y = y_ * ns.x + ns.yyyy;
    vec4 h = 1.0 - abs(x) - abs(y);
    
    vec4 b0 = vec4(x.xy, y.xy);
    vec4 b1 = vec4(x.zw, y.zw);
    
    vec4 s0 = floor(b0) * 2.0 + 1.0;
    vec4 s1 = floor(b1) * 2.0 + 1.0;
    vec4 sh = -step(h, vec4(0.0));
    
    vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
    vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;
    
    vec3 p0 = vec3(a0.xy, h.x);
    vec3 p1 = vec3(a0.zw, h.y);
    vec3 p2 = vec3(a1.xy, h.z);
    vec3 p3 = vec3(a1.zw, h.w);
    
    vec4 norm = 1.79284291400159 - 0.85373472095314 * vec4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3));
    p0 *= norm.x;
    p1 *= norm.y;
    p2 *= norm.z;
    p3 *= norm.w;
    
    vec4 m = max(0.6 - vec4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
    m = m * m;
    return 42.0 * dot(m * m, vec4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3)));
}

// === FBM FUNCTIONS ===
float fbmPerlin(vec3 p, int octaves) {
    float value = 0.0;
    float amplitude = 1.0;
    for (int i = 0; i < octaves; i++) {
        value += amplitude * cnoise2(p);
        p *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

float fbmWorley(vec3 p, int octaves) {
    float value = 0.0;
    float amplitude = 0.4;
    for (int i = 0; i < octaves; i++) {
        value += amplitude * worley(p);
        p *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

float cloudNoise(vec3 pos) {
    float angle = uTime * 0.1;
    mat3 rotation = mat3(
        cos(angle), -sin(angle), 0.0,
        sin(angle), cos(angle), 0.0,
        0.0, 0.0, 1.0
    );
    vec3 rotatedPos = rotation * pos;

    // Animation temporelle
    vec3 timeOffset = vec3(uTime * 0.2, uTime * 0.15, uTime * 0.1);
    vec3 timeOffset2 = vec3(uTime * 0.3, uTime * 0.2, uTime * 0.25);
    vec3 timeOffset3 = vec3(uTime * 0.5, uTime * 0.4, uTime * 0.3);
    
    // GRANDS NUAGES (échelle faible)
    float largeNoise = fbmPerlin(rotatedPos * 4.0 + timeOffset, 4);
    float largeWorley = fbmWorley(rotatedPos * 8.0 + timeOffset2, 3);
    float largePattern = largeNoise * 0.7 + (1.0 - largeWorley) * 0.3;
    
    // PETITS NUAGES (échelle élevée)
    float smallNoise = fbmPerlin(rotatedPos * 24.0 + timeOffset, 4);
    float smallWorley = fbmWorley(rotatedPos * 48.0 + timeOffset2, 3);
    float smallPattern = smallNoise * 0.6 + (1.0 - smallWorley) * 0.4;
    
    // DÉTAILS FINS
    float detailNoise = simplex(rotatedPos * 32.0 + timeOffset3) * 0.25;
    
    // COMBINER les deux échelles
    // Les grands nuages dominent, les petits ajoutent de la variété
    float cloudShape = smallPattern * 0.3 + largePattern * 0.6 + detailNoise * 0.1;
    
    // Normaliser vers [0,1]
    cloudShape = cloudShape * 0.5 + 0.3;
    
    //return clamp(cloudShape, 0.0, 1.0);

    return cloudShape;
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
    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(uLightDir);
    vec3 viewDir = normalize(uCamPos - vPosWorld);
    float diff = max(dot(normal, lightDir), 0.0);

    // === CALCUL SPÉCULAIRE ===
    //vec3 reflectDir = reflect(-lightDir, normal);
    //float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0); // 32 = shininess
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
    
    // === CALCUL DE L'ALTITUDE ===
    float planetRadius = 1.0; // Rayon de la planète (ajustez selon votre échelle)
    float currentRadius = length(vPosWorld); // Distance du centre
    float altitude = (currentRadius - planetRadius) / planetRadius; // Altitude normalisée
    
    // Facteur d'atténuation atmosphérique (décroissance exponentielle)
    float atmosphereFalloff = exp(-altitude * 8.0); // Plus l'altitude est haute, plus c'est transparent
    
    // === NUAGES ===
    float cloudDensity = cloudNoise(vPosWorld);
    float densityThreshold = 0.4;
    float cloudAlpha = 0.0;
    
    if (cloudDensity > densityThreshold) {
        float normalizedDensity = (cloudDensity - densityThreshold) / (1.0 - densityThreshold);
        cloudAlpha = pow(normalizedDensity, 0.7) * 1.0;
        cloudAlpha *= mix(0.00, 1.0, diff);
        
        // Appliquer l'atténuation atmosphérique aux nuages
        cloudAlpha *= atmosphereFalloff;
    }
    
    // === AURA ATMOSPHÉRIQUE ===
    //float rim = 1.0 - max(dot(viewDir, normal), 0.0);
    //rim = pow(rim, 0.5);
    
    float lightIntensity = max(dot(normal, lightDir), 0.0);
    float atmosphereAlpha = 0.6 * (0.2 + 0.8 * lightIntensity);
    
    // Appliquer l'atténuation atmosphérique à l'aura
    atmosphereAlpha *= atmosphereFalloff;
    
    vec3 cloudColor = vec3(0.9, 0.95, 1.0);
    vec3 atmosphereColor = vec3(0.8, 0.6, 1.0);
    
    // Ajouter spéculaire aux nuages (effet de brillance sur les gouttelettes)
    //vec3 specularColor = vec3(1.0, 1.0, 1.0) * specBlinn * 0.3;
    //cloudColor += specularColor;
    
    // Spéculaire atmosphérique plus subtil (diffusion)
    vec3 atmosphereSpecular = vec3(spec);
    //atmosphereColor += atmosphereSpecular;

    // === COMBINAISON ===
    // Mélanger nuages et atmosphère
    float totalAlpha = max(cloudAlpha, atmosphereAlpha);
    vec3 finalColor;
    
    if (cloudAlpha > 0.1) {
        // Zone avec nuages : mélanger couleurs
        float mixFactor = cloudAlpha / (cloudAlpha + atmosphereAlpha + 0.001);
        finalColor = mix(atmosphereColor, cloudColor, mixFactor);
    } else {
        // Zone sans nuages : seulement atmosphère
        finalColor = atmosphereColor;
        totalAlpha = atmosphereAlpha;
    }

    finalColor += atmosphereSpecular * 0.4;
    totalAlpha += atmosphereSpecular.r * 0.1; // Ajouter un peu de transparence pour l'effet spéculaire
    
    float weight = clamp(pow(min(1.0, totalAlpha * 10.0) + 0.01, 3.0), 0.01, 1.0);
    
    if (totalAlpha < 0.01) {
        discard;
    }
    
    outAccum = vec4(finalColor, totalAlpha) * weight;
    outReveal = (1.0 - totalAlpha) * weight;
}
)";