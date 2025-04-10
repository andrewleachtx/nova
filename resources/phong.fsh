#version 430

// For the sun
uniform vec3 lightPos;
uniform vec3 lightCol;

uniform vec3 ka;
uniform vec3 kd;
uniform vec3 ks;
uniform float s;

in vec3 vPos;
in vec3 vNor;

out vec4 fragColor;

void main()
{	
	vec3 n_nc = normalize(vNor);
	vec3 e_nc = normalize(vec3(0.0, 0.0, 0.0) - vPos); // assume camera at (0, 0, 0)

	// Ambient Color l_ac
	vec3 l_ac = ka;

	// Diffused Color l_dc
	vec3 l_nc = normalize(lightPos - vPos);
	vec3 l_dc = kd * max(0.0, dot(n_nc, l_nc));

	// Specular Color l_sc
	vec3 l_hc = normalize(l_nc + e_nc);
	vec3 l_sc = ks * pow(max(0.0, dot(l_hc, n_nc)), s);

    vec3 color = (l_ac + l_dc + l_sc);

    vec3 overall = vec3(0.0, 0.0, 0.0);
    overall += lightCol * color;

    fragColor = vec4(overall, 1.0f);
}
