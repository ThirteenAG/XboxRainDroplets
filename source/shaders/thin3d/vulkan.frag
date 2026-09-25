
#version 450

layout(binding = 0, std140) uniform XrdConstants
{
    mat4 projection;
    vec4 uvOffset;
    vec4 uvScale;
    vec4 sceneComplement;
};

layout(binding = 1) uniform sampler2D sceneTexture;
layout(binding = 2) uniform sampler2D maskTexture;

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vAtlas;
layout(location = 2) in vec2 vScene;

layout(location = 0) out vec4 fragColor0;

void main()
{
    vec4 mask = texture(maskTexture, vAtlas);
    vec4 scene = texture(sceneTexture, vScene);

    vec4 color = vColor * mask;

    // sceneComplement.x picks the complement, .y turns the sampling off
    vec3 backdrop = vec3(1.0);
    if (sceneComplement.y > 0.5)
        backdrop = sceneComplement.x > 0.5 ? (vec3(1.0) - scene.rgb) : scene.rgb;

    color.rgb *= backdrop;
    fragColor0 = color;
}
