#version 430

layout(location = 0) in vec2 pos;

uniform mat4 projection; // Converts to NDC

void main()
{
	gl_Position = projection * vec4(pos, 0.0f, 1.0f);
    //gl_PointSize = 100.0;
}
