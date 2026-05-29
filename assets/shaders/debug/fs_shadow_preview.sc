$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(s_shadowPreview, 0);
uniform vec4 u_shadowPreviewParams;

void main()
{
    const float blackDepth = u_shadowPreviewParams.x;
    const float inverseRange = u_shadowPreviewParams.y;
    const float invert = u_shadowPreviewParams.z;
    float depth = texture2D(s_shadowPreview, v_texcoord0).x;
    float preview = saturate((depth - blackDepth) * inverseRange);
    preview = mix(preview, 1.0 - preview, step(0.5, invert));
    gl_FragColor = vec4(preview, preview, preview, 1.0) * v_color0;
}
