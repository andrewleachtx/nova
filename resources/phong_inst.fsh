#version 430

// particles_vsh.glsl
in vec3 vPos;
in vec3 vNor;
in vec3 vKa;

out vec4 fragColor;

void main() {
    fragColor = vec4(vKa, 1.0);
}