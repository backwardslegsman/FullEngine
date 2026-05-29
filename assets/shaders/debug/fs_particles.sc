$input v_texcoord0, v_viewdepth, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(s_particleTexture, 15);
SAMPLER2D(s_particleDepth, 14);
uniform vec4 u_particleParams;

void main()
{
    vec4 texel = texture2D(s_particleTexture, v_texcoord0);
    vec4 color = texel * v_color0;
    if (u_particleParams.x > 0.5 && u_particleParams.y > 0.0001)
    {
        vec2 screenUv = gl_FragCoord.xy * u_particleParams.zw;
        float sceneDepthMeters = texture2D(s_particleDepth, screenUv).r * 1000.0;
        if (sceneDepthMeters > 0.0001 && sceneDepthMeters < 999.9)
        {
            float depthGapMeters = sceneDepthMeters - v_viewdepth;
            color.a *= saturate(depthGapMeters / u_particleParams.y);
        }
    }
    gl_FragColor = vec4(sqrt(max(color.rgb, vec3(0.0, 0.0, 0.0))), color.a);
}
