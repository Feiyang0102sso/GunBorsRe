// Ported from ogles_ps_texenv0 (jay.brown, 12-12-2010).
// Source: _IDA_OUT/gunbros_3.6.0_IOS.c:28598
// texEnv0 selects the blend: 0 = vertex colour only, 3 = colour x texture,
// anything else = texture only.
#version 330 core

uniform sampler2D tex0;
uniform int texEnv0;

in vec4 texcoord0;
in vec4 outcolor;

out vec4 fragColor;

void main()
{
    vec4 color;

    if (texEnv0 != 0)
    {
        if (texEnv0 == 3)
            color = outcolor * texture(tex0, texcoord0.xy);
        else
            color = texture(tex0, texcoord0.xy);
    }
    else
        color = outcolor;

    fragColor = color;
}
