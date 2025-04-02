#version 430

uniform mat4 P;
uniform mat4 MV;
uniform mat4 MV_it;
uniform float timeWindow_L;
uniform float timeWindow_R;

layout(location = 0) in vec4 aPos;
layout(location = 1) in vec3 aNor;
layout(location = 3) in vec4 aInstancePos;  // x, y, z, polarity

out vec3 vPos;
out vec3 vNor;
out float vAlpha;
out float vPolarity;

void main()
{
    mat4 instanceMV = MV;
    
    instanceMV[3][0] += aInstancePos.x;
    instanceMV[3][1] += aInstancePos.y;
    instanceMV[3][2] += aInstancePos.z;
    
    // Transform vertex position with the instance-specific matrix
    gl_Position = P * instanceMV * aPos;
    
    // Calculate view space position for lighting
    vPos = (instanceMV * aPos).xyz;
    
    // Pass normal through
    vNor = normalize((MV_it * vec4(aNor, 0.0)).xyz);
    
    // Store the polarity for use in fragment shader
    vPolarity = aInstancePos.w;
    
    if (aInstancePos.z < timeWindow_L || aInstancePos.z > timeWindow_R) {
        // Semi-transparent for events outside time window
        vAlpha = 0.3;
    }
    else {
        // Fully opaque for events inside time window
        vAlpha = 1.0;
    }
}