// Ported from ogles_vs_mvp_constcolor (jay.brown, 12-12-2010).
// Source: _IDA_OUT/gunbros_3.6.0_IOS.c:28599
// GL 3.3 changes: attribute -> in, varying -> out, precision qualifiers dropped.
#version 330 core

in vec4 Position;

uniform mat4 mvp;
uniform vec4 constColor; //+mb: may be better to use constant attribute,
                         // but this may also be more difficult to fit into some models of use.
out vec4 outcolor;

void main()
{
    gl_Position = mvp * Position;

    outcolor = constColor;
}
