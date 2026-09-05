// Ported from ogles_ps_tex0 (jay.brown, 12-12-2010).
// Source: _IDA_OUT/gunbros_3.6.0_IOS.c:28597
#version 330 core

uniform sampler2D tex0;

in vec4 texcoord0;

out vec4 fragColor;

void main()
{
    fragColor = texture(tex0, texcoord0.xy);
}
