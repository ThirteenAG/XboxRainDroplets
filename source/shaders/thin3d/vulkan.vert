
#version 450

layout(binding = 0, std140) uniform XrdConstants
{
    mat4 projection;
    vec4 uvOffset;
    vec4 uvScale;
    vec4 sceneComplement;
};

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color0;
layout(location = 3) in vec4 TexCoord0;

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vAtlas;
layout(location = 2) out vec2 vScene;

void main()
{
    gl_Position = projection * vec4(Position, 1.0);
    vColor = Color0;
    vAtlas = TexCoord0.xy;
    vScene = TexCoord0.zw * uvScale.xy + uvOffset.xy;
}
