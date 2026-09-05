// Ported from ogles_ps_constcolor (jay.brown, 12-12-2010).
// Source: _IDA_OUT/gunbros_3.6.0_IOS.c:28327
// GL 3.3 changes: varying -> in, gl_FragColor -> an explicit out.
#version 330 core

in vec4 outcolor;

out vec4 fragColor;

void main()
{
    fragColor = outcolor;
}
