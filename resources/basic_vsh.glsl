#version 430

layout(location = 0) in vec3 pos; // z is weight

uniform mat4 projection; // Converts to NDC

out float weight;

void main()
{
    weight = pos.z;
	gl_Position = projection * vec4(pos.x, pos.y, 0.0f, 1.0f);
}
