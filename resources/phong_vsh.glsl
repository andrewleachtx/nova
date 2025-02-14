#version 430

uniform mat4 P;
uniform mat4 MV;
uniform mat4 MV_it;

layout(location = 0) in vec4 aPos;
layout(location = 1) in vec3 aNor;

out vec3 vPos;
out vec3 vNor;

void main()
{
	gl_Position = P * MV * aPos;

	vPos = (MV * aPos).xyz;
	vNor = normalize((MV_it * vec4(aNor, 0.0)).xyz);
}
