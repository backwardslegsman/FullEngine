$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_ssao, 13);

uniform vec4 u_ssaoParams;

void main()
{
    float radius = max(u_ssaoParams.z, 0.0);
    vec2 direction = u_ssaoParams.w > 0.5 ? vec2(0.0, u_ssaoParams.y) : vec2(u_ssaoParams.x, 0.0);
    vec2 offset = direction * radius;

    float center = texture2D(s_ssao, v_texcoord0).r;
    float near0 = texture2D(s_ssao, v_texcoord0 + offset).r;
    float near1 = texture2D(s_ssao, v_texcoord0 - offset).r;
    float far0 = texture2D(s_ssao, v_texcoord0 + offset * 2.0).r;
    float far1 = texture2D(s_ssao, v_texcoord0 - offset * 2.0).r;
    float ao = center * 0.375 + (near0 + near1) * 0.25 + (far0 + far1) * 0.0625;
    gl_FragColor = vec4(ao, ao, ao, 1.0);
}
