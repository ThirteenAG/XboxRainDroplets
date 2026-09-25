
#version 120

uniform sampler2D sceneTexture;
uniform sampler2D maskTexture;
uniform vec2 uvOffset;
uniform vec2 uvScale;
uniform float sceneComplement;

varying vec4 vColor;
varying vec2 vAtlas;
varying vec2 vScene;

void main()
{
    vec4 mask = texture2D(maskTexture, vAtlas);
    vec4 scene = texture2D(sceneTexture, vScene * uvScale + uvOffset);

    vec4 color = vColor * mask;
    vec3 backdrop = sceneComplement > 0.5 ? (vec3(1.0) - scene.rgb) : scene.rgb;

    color.rgb *= backdrop;
    gl_FragColor = color;
}
