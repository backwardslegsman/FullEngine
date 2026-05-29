$input v_normal, v_color0, v_shadowcoord, v_shadowcoord1, v_shadowcoord2, v_shadowcoord3, v_viewdepth

#include <bgfx_shader.sh>

SAMPLER2D(s_shadowMap0, 5);
SAMPLER2D(s_shadowMap1, 6);
SAMPLER2D(s_shadowMap2, 7);
SAMPLER2D(s_shadowMap3, 8);

uniform vec4 u_lightDirIntensity;
uniform vec4 u_lightColor;
uniform vec4 u_materialColor;
uniform vec4 u_shadowParams;
uniform vec4 u_shadowFilterParams;
uniform vec4 u_cascadeSplits;
uniform vec4 u_environmentColors[4];
uniform vec4 u_fogParams;
uniform vec4 u_weatherParams;
uniform vec4 u_fadeParams;

vec3 linearToGamma(vec3 color)
{
    return pow(max(color, vec3(0.0, 0.0, 0.0)), vec3_splat(1.0 / 2.2));
}

float compareShadowDepth(float storedDepth, float currentDepth)
{
    return currentDepth > storedDepth ? 0.0 : 1.0;
}

float sampleShadowMap0(vec2 uv, float currentDepth, float filterMode, float texelSize)
{
    float result = 0.0;
    if (filterMode < 0.5)
    {
        result = compareShadowDepth(texture2D(s_shadowMap0, uv).r, currentDepth);
    }
    else if (filterMode < 1.5)
    {
        vec2 texel = vec2_splat(texelSize);
        float visibility = 0.0;
        visibility += compareShadowDepth(texture2D(s_shadowMap0, uv + texel * vec2(-0.5, -0.5)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap0, uv + texel * vec2(0.5, -0.5)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap0, uv + texel * vec2(-0.5, 0.5)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap0, uv + texel * vec2(0.5, 0.5)).r, currentDepth);
        result = visibility * 0.25;
    }
    else
    {
        vec2 texel = vec2_splat(texelSize);
        float visibility = 0.0;
        visibility += compareShadowDepth(texture2D(s_shadowMap0, uv + texel * vec2(-1.0, -1.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap0, uv + texel * vec2(0.0, -1.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap0, uv + texel * vec2(1.0, -1.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap0, uv + texel * vec2(-1.0, 0.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap0, uv).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap0, uv + texel * vec2(1.0, 0.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap0, uv + texel * vec2(-1.0, 1.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap0, uv + texel * vec2(0.0, 1.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap0, uv + texel * vec2(1.0, 1.0)).r, currentDepth);
        result = visibility * (1.0 / 9.0);
    }
    return result;
}

float sampleShadowMap1(vec2 uv, float currentDepth, float filterMode, float texelSize)
{
    float result = 0.0;
    if (filterMode < 0.5)
    {
        result = compareShadowDepth(texture2D(s_shadowMap1, uv).r, currentDepth);
    }
    else if (filterMode < 1.5)
    {
        vec2 texel = vec2_splat(texelSize);
        float visibility = 0.0;
        visibility += compareShadowDepth(texture2D(s_shadowMap1, uv + texel * vec2(-0.5, -0.5)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap1, uv + texel * vec2(0.5, -0.5)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap1, uv + texel * vec2(-0.5, 0.5)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap1, uv + texel * vec2(0.5, 0.5)).r, currentDepth);
        result = visibility * 0.25;
    }
    else
    {
        vec2 texel = vec2_splat(texelSize);
        float visibility = 0.0;
        visibility += compareShadowDepth(texture2D(s_shadowMap1, uv + texel * vec2(-1.0, -1.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap1, uv + texel * vec2(0.0, -1.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap1, uv + texel * vec2(1.0, -1.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap1, uv + texel * vec2(-1.0, 0.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap1, uv).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap1, uv + texel * vec2(1.0, 0.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap1, uv + texel * vec2(-1.0, 1.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap1, uv + texel * vec2(0.0, 1.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap1, uv + texel * vec2(1.0, 1.0)).r, currentDepth);
        result = visibility * (1.0 / 9.0);
    }
    return result;
}

float sampleShadowMap2(vec2 uv, float currentDepth, float filterMode, float texelSize)
{
    float result = 0.0;
    if (filterMode < 0.5)
    {
        result = compareShadowDepth(texture2D(s_shadowMap2, uv).r, currentDepth);
    }
    else if (filterMode < 1.5)
    {
        vec2 texel = vec2_splat(texelSize);
        float visibility = 0.0;
        visibility += compareShadowDepth(texture2D(s_shadowMap2, uv + texel * vec2(-0.5, -0.5)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap2, uv + texel * vec2(0.5, -0.5)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap2, uv + texel * vec2(-0.5, 0.5)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap2, uv + texel * vec2(0.5, 0.5)).r, currentDepth);
        result = visibility * 0.25;
    }
    else
    {
        vec2 texel = vec2_splat(texelSize);
        float visibility = 0.0;
        visibility += compareShadowDepth(texture2D(s_shadowMap2, uv + texel * vec2(-1.0, -1.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap2, uv + texel * vec2(0.0, -1.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap2, uv + texel * vec2(1.0, -1.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap2, uv + texel * vec2(-1.0, 0.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap2, uv).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap2, uv + texel * vec2(1.0, 0.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap2, uv + texel * vec2(-1.0, 1.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap2, uv + texel * vec2(0.0, 1.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap2, uv + texel * vec2(1.0, 1.0)).r, currentDepth);
        result = visibility * (1.0 / 9.0);
    }
    return result;
}

float sampleShadowMap3(vec2 uv, float currentDepth, float filterMode, float texelSize)
{
    float result = 0.0;
    if (filterMode < 0.5)
    {
        result = compareShadowDepth(texture2D(s_shadowMap3, uv).r, currentDepth);
    }
    else if (filterMode < 1.5)
    {
        vec2 texel = vec2_splat(texelSize);
        float visibility = 0.0;
        visibility += compareShadowDepth(texture2D(s_shadowMap3, uv + texel * vec2(-0.5, -0.5)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap3, uv + texel * vec2(0.5, -0.5)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap3, uv + texel * vec2(-0.5, 0.5)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap3, uv + texel * vec2(0.5, 0.5)).r, currentDepth);
        result = visibility * 0.25;
    }
    else
    {
        vec2 texel = vec2_splat(texelSize);
        float visibility = 0.0;
        visibility += compareShadowDepth(texture2D(s_shadowMap3, uv + texel * vec2(-1.0, -1.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap3, uv + texel * vec2(0.0, -1.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap3, uv + texel * vec2(1.0, -1.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap3, uv + texel * vec2(-1.0, 0.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap3, uv).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap3, uv + texel * vec2(1.0, 0.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap3, uv + texel * vec2(-1.0, 1.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap3, uv + texel * vec2(0.0, 1.0)).r, currentDepth);
        visibility += compareShadowDepth(texture2D(s_shadowMap3, uv + texel * vec2(1.0, 1.0)).r, currentDepth);
        result = visibility * (1.0 / 9.0);
    }
    return result;
}

float sampleCascadeShadow(int cascadeIndex, vec2 uv, float currentDepth, float filterMode, float texelSize)
{
    float result = sampleShadowMap3(uv, currentDepth, filterMode, texelSize);
    if (cascadeIndex == 0)
    {
        result = sampleShadowMap0(uv, currentDepth, filterMode, texelSize);
    }
    else if (cascadeIndex == 1)
    {
        result = sampleShadowMap1(uv, currentDepth, filterMode, texelSize);
    }
    else if (cascadeIndex == 2)
    {
        result = sampleShadowMap2(uv, currentDepth, filterMode, texelSize);
    }
    return result;
}

float sampleMeshShadow(vec3 normal, vec3 lightDirection, vec4 shadowCoord0, vec4 shadowCoord1, vec4 shadowCoord2, vec4 shadowCoord3, float viewDepth)
{
    float shadowFactor = 1.0;
    int cascadeCount = int(clamp(u_shadowParams.x, 0.0, 4.0));
    if (cascadeCount > 0)
    {
        int cascadeIndex = 0;
        vec4 shadowPosition = shadowCoord0;
        float previousFar = 0.0;
        float selectedFar = u_cascadeSplits.x;
        if (cascadeCount > 1 && viewDepth > u_cascadeSplits.x)
        {
            cascadeIndex = 1;
            shadowPosition = shadowCoord1;
            previousFar = u_cascadeSplits.x;
            selectedFar = u_cascadeSplits.y;
        }
        if (cascadeCount > 2 && viewDepth > u_cascadeSplits.y)
        {
            cascadeIndex = 2;
            shadowPosition = shadowCoord2;
            previousFar = u_cascadeSplits.y;
            selectedFar = u_cascadeSplits.z;
        }
        if (cascadeCount > 3 && viewDepth > u_cascadeSplits.z)
        {
            cascadeIndex = 3;
            shadowPosition = shadowCoord3;
            previousFar = u_cascadeSplits.z;
            selectedFar = u_cascadeSplits.w;
        }

        float receiverBias = u_shadowParams.y + u_shadowFilterParams.y * (1.0 - max(dot(normal, lightDirection), 0.0));
        float filterMode = u_shadowFilterParams.z;
        float shadowTexelSize = u_shadowFilterParams.w;
        vec3 shadowCoord = shadowPosition.xyz / shadowPosition.w;
        bool insideShadowMap =
            viewDepth <= selectedFar &&
            shadowCoord.x >= -1.0 &&
            shadowCoord.x <= 1.0 &&
            shadowCoord.y >= -1.0 &&
            shadowCoord.y <= 1.0 &&
            shadowCoord.z >= 0.0 &&
            shadowCoord.z <= 1.0;
        if (insideShadowMap)
        {
            vec2 shadowUv = shadowCoord.xy * 0.5 + vec2_splat(0.5);
            shadowUv.y = 1.0 - shadowUv.y;
            float visibility = sampleCascadeShadow(cascadeIndex, shadowUv, shadowCoord.z - receiverBias, filterMode, shadowTexelSize);
            shadowFactor = mix(1.0 - u_shadowParams.z, 1.0, visibility);
        }

        float blendFraction = clamp(u_shadowFilterParams.x, 0.0, 0.5);
        if (blendFraction > 0.0 && cascadeIndex + 1 < cascadeCount)
        {
            float cascadeSpan = max(selectedFar - previousFar, 0.0001);
            float blendSize = cascadeSpan * blendFraction;
            float blendStart = selectedFar - blendSize;
            if (viewDepth >= blendStart && viewDepth <= selectedFar)
            {
                vec4 nextShadowPosition = shadowCoord1;
                float nextFar = u_cascadeSplits.y;
                if (cascadeIndex == 1)
                {
                    nextShadowPosition = shadowCoord2;
                    nextFar = u_cascadeSplits.z;
                }
                else if (cascadeIndex == 2)
                {
                    nextShadowPosition = shadowCoord3;
                    nextFar = u_cascadeSplits.w;
                }

                vec3 nextShadowCoord = nextShadowPosition.xyz / nextShadowPosition.w;
                bool nextInsideShadowMap =
                    viewDepth <= nextFar &&
                    nextShadowCoord.x >= -1.0 &&
                    nextShadowCoord.x <= 1.0 &&
                    nextShadowCoord.y >= -1.0 &&
                    nextShadowCoord.y <= 1.0 &&
                    nextShadowCoord.z >= 0.0 &&
                    nextShadowCoord.z <= 1.0;
                if (nextInsideShadowMap)
                {
                    vec2 nextShadowUv = nextShadowCoord.xy * 0.5 + vec2_splat(0.5);
                    nextShadowUv.y = 1.0 - nextShadowUv.y;
                    float nextVisibility = sampleCascadeShadow(
                        cascadeIndex + 1,
                        nextShadowUv,
                        nextShadowCoord.z - receiverBias,
                        filterMode,
                        shadowTexelSize);
                    float nextShadowFactor = mix(1.0 - u_shadowParams.z, 1.0, nextVisibility);
                    float blendT = clamp((viewDepth - blendStart) / blendSize, 0.0, 1.0);
                    blendT = blendT * blendT * (3.0 - 2.0 * blendT);
                    shadowFactor = mix(shadowFactor, nextShadowFactor, blendT);
                }
            }
        }
    }

    return shadowFactor;
}

vec3 applyDistanceFog(vec3 color, float viewDepth)
{
    float fogFactor = u_fogParams.x * saturate((viewDepth - u_fogParams.y) * u_fogParams.z);
    return mix(color, u_environmentColors[3].rgb, fogFactor);
}

vec3 applyWeatherWetness(vec3 color)
{
    float amount = clamp(u_weatherParams.y, 0.0, 1.0);
    float darkening = clamp(u_weatherParams.z, 0.0, 1.0);
    return color * (1.0 - amount * darkening);
}

float dither4x4Threshold(vec2 position)
{
    vec2 pixel = floor(position - floor(position * 0.25) * 4.0);
    float x = pixel.x;
    float y = pixel.y;
    float value = 0.0;
    if (y < 0.5)
    {
        if (x < 0.5) { value = 0.0; }
        else if (x < 1.5) { value = 8.0; }
        else if (x < 2.5) { value = 2.0; }
        else { value = 10.0; }
    }
    else if (y < 1.5)
    {
        if (x < 0.5) { value = 12.0; }
        else if (x < 1.5) { value = 4.0; }
        else if (x < 2.5) { value = 14.0; }
        else { value = 6.0; }
    }
    else if (y < 2.5)
    {
        if (x < 0.5) { value = 3.0; }
        else if (x < 1.5) { value = 11.0; }
        else if (x < 2.5) { value = 1.0; }
        else { value = 9.0; }
    }
    else
    {
        if (x < 0.5) { value = 15.0; }
        else if (x < 1.5) { value = 7.0; }
        else if (x < 2.5) { value = 13.0; }
        else { value = 5.0; }
    }
    return (value + 0.5) * (1.0 / 16.0);
}

void main()
{
    vec3 normal = normalize(v_normal);
    vec3 lightDirection = normalize(u_lightDirIntensity.xyz);
    float litFlag = step(0.0, u_materialColor.w);
    float alpha = abs(u_materialColor.w);
    float diffuse = max(dot(normal, lightDirection), 0.0) * u_lightDirIntensity.w;
    float shadowFactor = sampleMeshShadow(
        normal,
        lightDirection,
        v_shadowcoord,
        v_shadowcoord1,
        v_shadowcoord2,
        v_shadowcoord3,
        v_viewdepth);
    vec3 litColor = v_color0.rgb * u_materialColor.rgb * u_lightColor.rgb * max(diffuse * shadowFactor, 0.15);
    vec3 unlitColor = v_color0.rgb * u_materialColor.rgb;
    vec3 finalColor = mix(unlitColor, litColor, litFlag);
    finalColor = applyWeatherWetness(finalColor);
    finalColor = applyDistanceFog(finalColor, v_viewdepth);
    float fadeVisibility = clamp(u_fadeParams.x, 0.0, 1.0);
    float fadeMode = u_fadeParams.y;
    float materialAlphaMode = u_fadeParams.z;
    float alphaCutoff = clamp(u_fadeParams.w, 0.0, 1.0);
    float finalAlpha = v_color0.a * alpha;
    if (materialAlphaMode > 0.5 && materialAlphaMode < 1.5 && finalAlpha < alphaCutoff)
    {
        discard;
    }
    if (fadeMode > 1.5 && fadeVisibility < 0.999)
    {
        if (fadeVisibility <= dither4x4Threshold(gl_FragCoord.xy))
        {
            discard;
        }
    }
    if (fadeMode > 0.5 && fadeMode < 1.5)
    {
        alpha *= fadeVisibility;
    }
    gl_FragColor = vec4(linearToGamma(finalColor), v_color0.a * alpha);
}
