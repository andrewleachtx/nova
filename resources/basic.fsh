#version 430

in float weight;

out vec4 fragColor;

void main()
{	
    vec3 rgb;
    if (weight < 0) {
        rgb = vec3(0.25f, 0.25f, 0.25f) * weight; 
        /** 
        *   0.25 is mostly arbitrary
        *   Experimentally saw that often there are more negative events than
        *   positive events. However, we likely want to look more at positive
        *   events, so the negative event carries less weight to balance this.
        *   This could be made controllable in the GUI and with the set value
        *   passed in as a uniform.
        **/
    }
    else{
        rgb = vec3(1.0f, 1.0f, 1.0f) * weight;
    }
    fragColor = vec4(rgb, 1.0f);
}
