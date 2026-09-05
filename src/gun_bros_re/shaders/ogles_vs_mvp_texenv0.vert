// Ported from ogles_vs_mvp_texenv0 (jay.brown, 12-12-2010).
// Source: _IDA_OUT/gunbros_3.6.0_IOS.c:28602
// Note the hardwired texture scale of 1:4096.
#version 330 core

in vec4 Position;
in vec4 TexCoord;

uniform mat4 mvp;
uniform vec4 constColor; //+mb: may be better to use constant attribute,
                         // but this may be more difficult to fit into some models of use.
uniform int texEnv0;

out vec4 texcoord0;
out vec4 outcolor;

void main()
{
    gl_Position = mvp * Position;

    texcoord0 = TexCoord * 0.0002441406255;

    outcolor = constColor;
}
