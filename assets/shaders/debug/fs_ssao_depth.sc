$input v_viewdepth

#include <bgfx_shader.sh>

uniform vec4 u_ssaoDepthParams;

void main()
{
    float viewDepth = v_viewdepth;
    float nearZ = u_ssaoDepthParams.y;
    float farZ = u_ssaoDepthParams.z;
    if (nearZ > 0.0 && farZ > nearZ)
    {
        float hardwareDepth = clamp(gl_FragCoord.z, 0.0, 1.0);
        viewDepth = (nearZ * farZ) / max(farZ - hardwareDepth * (farZ - nearZ), 0.0001);
    }

    float normalizedDepth = saturate(viewDepth * max(u_ssaoDepthParams.x, 0.0));
    gl_FragColor = vec4(normalizedDepth, normalizedDepth, normalizedDepth, 1.0);
}
