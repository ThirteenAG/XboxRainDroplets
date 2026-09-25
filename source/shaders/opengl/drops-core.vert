
#version 150

uniform mat4 projection;

in vec3 position;
in vec4 color;
in vec2 atlas;
in vec2 scene;

out vec4 vColor;
out vec2 vAtlas;
out vec2 vScene;

void main()
{
    gl_Position = projection * vec4(position, 1.0);
    vColor = color;
    vAtlas = atlas;
    vScene = scene;
}
