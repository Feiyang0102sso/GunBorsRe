// Ported from ogles_ps_tex0 (jay.brown, 12-12-2010).
// Source: _IDA_OUT/gunbros_3.6.0_IOS.c:28597
#version 330 core

uniform sampler2D tex0;
// The gun's scripted heat is a red overlay in CBrother::Draw.
// Zero alpha keeps the existing sprite and mesh shading unchanged.
uniform vec4 meshOverlay;
// PNGLoader supplies straight alpha. ONE/ONE effects need premultiplied RGB.
uniform bool additiveOpaque;

in vec4 texcoord0;
in float opacity;

out vec4 fragColor;

void main()
{
    fragColor = texture(tex0, texcoord0.xy);
    fragColor.rgb = mix(fragColor.rgb, meshOverlay.rgb, meshOverlay.a);
    fragColor.a *= opacity;
    if (additiveOpaque) {
        fragColor.rgb *= fragColor.a;
    }
}
