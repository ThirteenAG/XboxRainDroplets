
#version 120

uniform mat4 projection;

attribute vec3 position;
attribute vec4 color;
attribute vec2 atlas;
attribute vec2 scene;

varying vec4 vColor;
varying vec2 vAtlas;
varying vec2 vScene;

void main()
{
    gl_Position = projection * vec4(position, 1.0);
    vColor = color;
    vAtlas = atlas;
    vScene = scene;
}
