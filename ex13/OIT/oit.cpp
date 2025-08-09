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

    //FragColor = vec4(accum.rgb, 1.0);
    
    if (reveal > 0.0) {
        // Zone transparente : formule OIT normale
        FragColor = vec4(accum.rgb / reveal, 1.0 - reveal);
    } else {
        FragColor = vec4(0.0);
    }
}
)";