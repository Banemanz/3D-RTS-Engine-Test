#pragma once

#include "GLCore.hpp"
#include <stdexcept>
#include <string>

inline const char* TERRAIN_VERTEX_SHADER = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in float aMaterial;
layout (location = 2) in float aBlocked;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
out vec3 vWorldPos;
out float vMaterial;
out float vBlocked;
void main() {
    vec4 worldPosition = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPosition.xyz;
    vMaterial = aMaterial;
    vBlocked = aBlocked;
    gl_Position = uProjection * uView * worldPosition;
}
)";

inline const char* TERRAIN_FRAGMENT_SHADER = R"(
#version 330 core
in vec3 vWorldPos;
in float vMaterial;
in float vBlocked;
out vec4 FragColor;

uniform float uShowPathing;
uniform float uShowGrid;
uniform vec4 uFogSettings; // minX, minZ, cellSpacing, enabled
uniform vec4 uFogSize; // number of terrain cells X, Z
uniform sampler2D uFogMap;
uniform vec3 uClickMarker;
uniform float uMarkerPulse;
uniform float uHasMarker;

float gridFactor(vec2 coord, float scale, float lineWidth) {
    vec2 grid = abs(fract(coord / scale - 0.5) - 0.5) / fwidth(coord / scale);
    float line = min(grid.x, grid.y);
    return 1.0 - min(line / lineWidth, 1.0);
}

void main() {
    vec3 grassDark  = vec3(0.13, 0.17, 0.15);
    vec3 grassLight = vec3(0.17, 0.22, 0.19);

    vec2 tile = floor(vWorldPos.xz / 4.0);
    float checker = mod(tile.x + tile.y, 2.0);
    vec3 baseColor = mix(grassDark, grassLight, checker * 0.45);
    // Authorable material IDs 0..7; the 5th mesh component carries pathing.
    // This is a procedural fallback palette until texture layers are implemented.
    int material = clamp(int(vMaterial + 0.5),0,7);
    if(material == 1)baseColor=vec3(.32,.24,.17);       // dirt
    else if(material == 2)baseColor=vec3(.35,.39,.42);  // rock
    else if(material == 3)baseColor=vec3(.57,.49,.30);  // sand
    else if(material == 4)baseColor=vec3(.25,.34,.17);  // scrub
    else if(material == 5)baseColor=vec3(.10,.25,.34);  // water placeholder
    else if(material == 6)baseColor=vec3(.77,.81,.85);  // snow
    else if(material == 7)baseColor=vec3(.20,.19,.23);  // ash

    float subGrid   = gridFactor(vWorldPos.xz, 1.0, 1.0) * 0.14;
    float majorGrid = gridFactor(vWorldPos.xz, 5.0, 1.5) * 0.32;

    float xAxis = 1.0 - smoothstep(0.0, 0.08, abs(vWorldPos.z));
    float zAxis = 1.0 - smoothstep(0.0, 0.08, abs(vWorldPos.x));

    vec3 color = baseColor;
    color = mix(color, vec3(0.35, 0.45, 0.40), subGrid*uShowGrid);
    color = mix(color, vec3(0.45, 0.60, 0.52), majorGrid*uShowGrid);
    if(uShowPathing > 0.5 && vBlocked > 0.5)
        color=mix(color,vec3(.90,.19,.16),.66);
    color = mix(color, vec3(0.85, 0.25, 0.25), xAxis * 0.5);
    color = mix(color, vec3(0.25, 0.45, 0.90), zAxis * 0.5);

    // Borders are determined by the authored terrain dimensions, not +/-48.
    vec2 worldMin=uFogSettings.xy;
    vec2 worldMax=worldMin+max(uFogSize.xy-vec2(1.0),vec2(0.0))*uFogSettings.z;
    vec2 border=min(vWorldPos.xz-worldMin,worldMax-vWorldPos.xz);
    if(min(border.x,border.y)<.40)
        color=mix(color,vec3(.34,.20,.17),.25);

    if (uHasMarker > 0.5) {
        float distToClick = length(vWorldPos.xz - uClickMarker.xz);
        float ringRadius = 0.65 + sin(uMarkerPulse * 6.0) * 0.12;
        float ring = 1.0 - smoothstep(0.0, 0.09, abs(distToClick - ringRadius));
        float centerDot = 1.0 - smoothstep(0.0, 0.18, distToClick);
        color += vec3(0.15, 0.95, 0.45) * (ring * 0.9 + centerDot * 0.7);
    }

    float distFromOrigin = length(vWorldPos.xz);
    float fogFactor = smoothstep(55.0, 115.0, distFromOrigin);
    vec3 skyColor = vec3(0.08, 0.11, 0.15);
    color = mix(color, skyColor, fogFactor);

    if(uFogSettings.w>0.5) {
        // Center-aligned lookup into a single R8 texel per terrain cell.
        vec2 cell=(vWorldPos.xz-uFogSettings.xy)/uFogSettings.z;
        vec2 uv=(cell+vec2(0.5))/uFogSize.xy;
        float state=texture(uFogMap,clamp(uv,vec2(0),vec2(1))).r;
        float brightness=state>.8?1.0:state>.18?.39:.035;
        color=mix(vec3(.010,.014,.020),color,brightness);
    }
    FragColor = vec4(color, 1.0);
}
)";

inline const char* UNIT_VERTEX_SHADER = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vNormal;

void main() {
    vNormal = mat3(uModel) * aNormal;
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
}
)";

inline const char* UNIT_FRAGMENT_SHADER = R"(
#version 330 core
in vec3 vNormal;
out vec4 FragColor;

uniform vec3 uColor;
uniform float uSelected;
uniform float uOpacity;

void main() {
    vec3 lightDir = normalize(vec3(0.45, 0.85, 0.35));
    float diff = max(dot(normalize(vNormal), lightDir), 0.0);
    float ambient = 0.38;
    vec3 litColor = uColor * (ambient + diff * 0.72);
    if (uSelected > 0.5) {
        litColor += vec3(0.12, 0.28, 0.14);
    }
    FragColor = vec4(litColor, uOpacity);
}
)";

inline const char* RING_VERTEX_SHADER = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
out vec2 vLocalXZ;
void main() {
    vLocalXZ = aPos.xz;
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
}
)";

inline const char* RING_FRAGMENT_SHADER = R"(
#version 330 core
in vec2 vLocalXZ;
out vec4 FragColor;
uniform vec4 uColor;
void main() {
    float r = length(vLocalXZ);
    if (r > 1.0 || r < 0.76) discard;
    FragColor = uColor;
}
)";

inline const char* HUD_BOX_VERTEX_SHADER = R"(
#version 330 core
layout (location = 0) in vec2 aNDC;
void main() {
    gl_Position = vec4(aNDC, 0.0, 1.0);
}
)";

inline const char* HUD_BOX_FRAGMENT_SHADER = R"(
#version 330 core
out vec4 FragColor;
uniform vec4 uColor;
void main() {
    FragColor = uColor;
}
)";

inline const char* LINE_VERTEX_SHADER = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 uView;
uniform mat4 uProjection;
void main() {
    gl_Position = uProjection * uView * vec4(aPos, 1.0);
}
)";

inline GLuint compileShaderProgram(const char* vertSrc, const char* fragSrc) {
    auto compile = [](GLenum type, const char* src) -> GLuint {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);
        GLint success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char log[512];
            glGetShaderInfoLog(shader, 512, nullptr, log);
            throw std::runtime_error(std::string("Shader Compilation Error: ") + log);
        }
        return shader;
    };

    GLuint vs = compile(GL_VERTEX_SHADER, vertSrc);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fragSrc);

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, 512, nullptr, log);
        throw std::runtime_error(std::string("Program Link Error: ") + log);
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}