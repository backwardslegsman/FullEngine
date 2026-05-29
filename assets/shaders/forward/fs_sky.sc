$input v_skyfactor

#include <bgfx_shader.sh>

uniform vec4 u_environmentColors[4];

vec3 linearToGamma(vec3 color)
{
    return pow(max(color, vec3(0.0, 0.0, 0.0)), vec3_splat(1.0 / 2.2));
}

void main()
{
    float horizonBlend = smoothstep(0.0, 0.55, v_skyfactor);
    float zenithBlend = smoothstep(0.45, 1.0, v_skyfactor);
    vec3 lower = mix(u_environmentColors[2].rgb, u_environmentColors[1].rgb, horizonBlend);
    vec3 sky = mix(lower, u_environmentColors[0].rgb, zenithBlend);
    gl_FragColor = vec4(linearToGamma(sky), 1.0);
}
