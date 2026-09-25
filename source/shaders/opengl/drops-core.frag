
#version 150

uniform sampler2D sceneTexture;
uniform sampler2D maskTexture;
uniform vec2 uvOffset;
uniform vec2 uvScale;
uniform float sceneComplement;

in vec4 vColor;
in vec2 vAtlas;
in vec2 vScene;

out vec4 fragColor;

void main()
{
    vec4 mask = texture(maskTexture, vAtlas);
    vec4 scene = texture(sceneTexture, vScene * uvScale + uvOffset);

    vec4 color = vColor * mask;
    vec3 backdrop = sceneComplement > 0.5 ? (vec3(1.0) - scene.rgb) : scene.rgb;

    color.rgb *= backdrop;
    fragColor = color;
}
