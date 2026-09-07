// Ported from ogles_vs_mvp_tex0 (jay.brown, 12-12-2010).
// Source: _IDA_OUT/gunbros_3.6.0_IOS.c:28601
// Note the hardwired texture scale of 1:4096 -- art data stores UVs as
// integers, so this constant must stay exactly as the original wrote it.
#version 330 core

in vec4 Position;
in vec4 TexCoord;
in float Alpha;

uniform mat4 mvp;

out vec4 texcoord0;
out float opacity;

void main()
{
    gl_Position = mvp * Position;

    texcoord0 = TexCoord * 0.0002441406255;
    opacity = Alpha;
}
