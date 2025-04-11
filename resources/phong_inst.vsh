#version 430

uniform mat4 P;
uniform mat4 MV;
uniform mat4 MV_it;

uniform float particleScale;
uniform vec3 lightPos;
uniform vec3 lightCol;

in vec3 aPos;
in vec3 aNor;
in vec4 aInstPos; // this is the position we have to shift to

out vec3 vPos;
out vec3 vNor;
out vec3 vKa; // we don't really need Blinn-Phong shading, just color

void main() {
    // TODO: Use an SSBO to store particle scale, and the color per particle
    // Abstract idea that we can grab the particle "idx" as the instanceID
    // int idx = gl_InstanceID;

    // if (idx >= maxParticles) {
    //     return;
    // }

    mat4 transform = mat4(1.0);
    transform[3] = aInstPos; // the current instance position

    // scale
    transform[0][0] = particleScale;
    transform[1][1] = particleScale;
    transform[2][2] = particleScale;

    // xyza -> for now if + green - red
    if (aInstPos.a > 1e-5) {
        vKa = vec3(0.0, 1.0, 0.0);
    }
    else {
        vKa = vec3(1.0, 0.0, 0.0);
    }

    mat4 MV_transf = MV * transform;

    gl_Position = P * MV_transf * vec4(aPos, 1.0);
    vPos = (MV_transf * vec4(aPos, 1.0)).xyz;

    vNor = normalize(MV_it * vec4(aNor, 0.0)).xyz;
}