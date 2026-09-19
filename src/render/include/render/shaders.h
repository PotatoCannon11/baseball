#pragma once

// Small, embedded GLSL sources (spec: "Small shaders"). GLSL 410 core to
// match the GL 4.1 core context. Not loaded from disk / hot-reloadable --
// unlike textures, the spec doesn't call for shader hot-reload, and
// compiling these in keeps the milestone-3 scene self-contained.
namespace render::shaders {

inline constexpr const char* kMainVertex = R"GLSL(
#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uLightSpace;

out vec3 vNormal;
out vec2 vUV;
out vec4 vLightSpacePos;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vNormal = mat3(uModel) * aNormal;
    vUV = aUV;
    vLightSpacePos = uLightSpace * worldPos;
    gl_Position = uProj * uView * worldPos;
}
)GLSL";

inline constexpr const char* kMainFragment = R"GLSL(
#version 410 core
in vec3 vNormal;
in vec2 vUV;
in vec4 vLightSpacePos;

uniform vec3 uLightDir;   // points TOWARD the light
uniform vec3 uBaseColor;
uniform sampler2D uShadowMap;
uniform int uUseShadow;

out vec4 FragColor;

float shadow_factor() {
    vec3 proj = vLightSpacePos.xyz / vLightSpacePos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0) return 1.0;
    float closestDepth = texture(uShadowMap, proj.xy).r;
    float bias = 0.0025;
    return (proj.z - bias > closestDepth) ? 0.55 : 1.0;
}

void main() {
    vec3 n = normalize(vNormal);
    float diff = max(dot(n, normalize(uLightDir)), 0.0);
    float shadow = (uUseShadow != 0) ? shadow_factor() : 1.0;
    vec3 ambient = 0.35 * uBaseColor;
    vec3 diffuse = 0.65 * diff * shadow * uBaseColor;
    FragColor = vec4(ambient + diffuse, 1.0);
}
)GLSL";

inline constexpr const char* kShadowVertex = R"GLSL(
#version 410 core
layout(location = 0) in vec3 aPos;
uniform mat4 uLightSpace;
uniform mat4 uModel;
void main() {
    gl_Position = uLightSpace * uModel * vec4(aPos, 1.0);
}
)GLSL";

inline constexpr const char* kShadowFragment = R"GLSL(
#version 410 core
void main() { }
)GLSL";

// Flat-shaded, unlit 2D overlay quads (debug text glyphs / HUD panels).
inline constexpr const char* kOverlayVertex = R"GLSL(
#version 410 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
uniform vec2 uScreenSize;
out vec2 vUV;
void main() {
    vec2 ndc = vec2(aPos.x / uScreenSize.x, aPos.y / uScreenSize.y) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    gl_Position = vec4(ndc, 0.0, 1.0);
    vUV = aUV;
}
)GLSL";

inline constexpr const char* kOverlayFragment = R"GLSL(
#version 410 core
in vec2 vUV;
uniform sampler2D uFontAtlas;
uniform vec3 uColor;
out vec4 FragColor;
void main() {
    float a = texture(uFontAtlas, vUV).r;
    if (a < 0.5) discard;
    FragColor = vec4(uColor, 1.0);
}
)GLSL";

}  // namespace render::shaders
