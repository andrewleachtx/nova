#version 430

in float weight;

out vec4 fragColor;

void main()
{	
    fragColor = vec4(1.0f, 1.0f, 1.0f, weight);
}
