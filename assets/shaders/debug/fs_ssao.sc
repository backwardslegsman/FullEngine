$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_ssaoDepth, 13);

uniform vec4 u_ssaoParams;
uniform vec4 u_ssaoDepthParams;

float sampleOcclusion(vec2 uv, float centerDepth, vec2 offset, float bias)
{
    float sampleDepth = texture2D(s_ssaoDepth, uv + offset).r;
    float depthDelta = centerDepth - sampleDepth;
    return depthDelta > bias ? saturate(depthDelta * 8.0) : 0.0;
}

void main()
{
    float centerDepth = texture2D(s_ssaoDepth, v_texcoord0).r;
    if (centerDepth <= 0.0001 || centerDepth >= 0.9999)
    {
        gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        return;
    }

    vec2 texelRadius = u_ssaoParams.xy * max(u_ssaoParams.z, 0.0);
    float intensity = max(u_ssaoParams.w, 0.0);
    float bias = max(u_ssaoDepthParams.x, 0.0);
    float power = max(u_ssaoDepthParams.y, 0.0001);
    float useEightSamples = step(0.5, u_ssaoDepthParams.z);

    float occlusion = 0.0;
    occlusion += sampleOcclusion(v_texcoord0, centerDepth, vec2(texelRadius.x, 0.0), bias);
    occlusion += sampleOcclusion(v_texcoord0, centerDepth, vec2(-texelRadius.x, 0.0), bias);
    occlusion += sampleOcclusion(v_texcoord0, centerDepth, vec2(0.0, texelRadius.y), bias);
    occlusion += sampleOcclusion(v_texcoord0, centerDepth, vec2(0.0, -texelRadius.y), bias);

    occlusion += sampleOcclusion(v_texcoord0, centerDepth, texelRadius, bias) * useEightSamples;
    occlusion += sampleOcclusion(v_texcoord0, centerDepth, -texelRadius, bias) * useEightSamples;
    occlusion += sampleOcclusion(v_texcoord0, centerDepth, vec2(texelRadius.x, -texelRadius.y), bias) * useEightSamples;
    occlusion += sampleOcclusion(v_texcoord0, centerDepth, vec2(-texelRadius.x, texelRadius.y), bias) * useEightSamples;

    float sampleCount = mix(4.0, 8.0, useEightSamples);
    float ao = pow(saturate(1.0 - (occlusion / sampleCount) * intensity), power);
    gl_FragColor = vec4(ao, ao, ao, 1.0);
}
