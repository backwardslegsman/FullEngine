$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_ssao, 13);

uniform vec4 u_ssaoDepthParams;

void main()
{
    float ao = saturate(texture2D(s_ssao, v_texcoord0).r);
    if (u_ssaoDepthParams.w > 0.5)
    {
        gl_FragColor = vec4(ao, ao, ao, 1.0);
        return;
    }

    gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0 - ao);
}
