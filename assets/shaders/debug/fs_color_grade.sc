$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_sceneColor, 12);

uniform vec4 u_colorGradeParams;
uniform vec4 u_colorGradeControls;
uniform vec4 u_colorGradeLift;
uniform vec4 u_colorGradeGain;

vec3 toLinear(vec3 color)
{
    return pow(max(color, vec3_splat(0.0)), vec3_splat(2.2));
}

vec3 toOutput(vec3 color)
{
    return pow(max(color, vec3_splat(0.0)), vec3_splat(1.0 / 2.2));
}

vec3 tonemapAcesApprox(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), vec3_splat(0.0), vec3_splat(1.0));
}

vec3 applyTonemap(vec3 color)
{
    vec3 reinhard = color / (vec3_splat(1.0) + color);
    vec3 aces = tonemapAcesApprox(color);
    float reinhardWeight = step(0.5, u_colorGradeParams.y) * (1.0 - step(1.5, u_colorGradeParams.y));
    float acesWeight = step(1.5, u_colorGradeParams.y);
    return color * (1.0 - reinhardWeight - acesWeight) +
        reinhard * reinhardWeight +
        aces * acesWeight;
}

vec3 applyGrading(vec3 color)
{
    color = max(color * u_colorGradeGain.rgb + u_colorGradeLift.rgb, vec3_splat(0.0));

    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3_splat(luma), color, u_colorGradeControls.y);
    color = (color - vec3_splat(0.5)) * u_colorGradeControls.x + vec3_splat(0.5);

    const float gamma = max(u_colorGradeControls.z, 0.0001);
    return pow(max(color, vec3_splat(0.0)), vec3_splat(1.0 / gamma));
}

void main()
{
    vec4 sourceColor = texture2D(s_sceneColor, v_texcoord0);
    vec3 color = toLinear(sourceColor.rgb);

    float tonemapStageWeight = 1.0 - step(1.5, u_colorGradeParams.w);
    float gradingStageWeight = min((1.0 - step(0.5, u_colorGradeParams.w)) + step(1.5, u_colorGradeParams.w), 1.0);
    vec3 exposedColor = color * max(u_colorGradeParams.x, 0.0);
    vec3 tonemappedColor = mix(exposedColor, applyTonemap(exposedColor), step(0.5, u_colorGradeParams.z));
    color = mix(color, tonemappedColor, tonemapStageWeight);
    color = mix(color, applyGrading(color), gradingStageWeight);

    gl_FragColor = vec4(toOutput(clamp(color, vec3_splat(0.0), vec3_splat(1.0))), sourceColor.a);
}
