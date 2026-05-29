$input v_normal, v_color0, v_texcoord0, v_shadowcoord, v_shadowcoord1, v_shadowcoord2, v_shadowcoord3, v_viewdepth

#include <bgfx_shader.sh>

SAMPLER2D(s_layer0, 0);
SAMPLER2D(s_layer1, 1);
SAMPLER2D(s_layer2, 2);
SAMPLER2D(s_layer3, 3);
SAMPLER2D(s_splat, 4);
SAMPLER2D(s_shadowMap0, 5);
SAMPLER2D(s_shadowMap1, 6);
SAMPLER2D(s_shadowMap2, 7);
SAMPLER2D(s_shadowMap3, 8);
SAMPLER2D(s_normal0, 9);
SAMPLER2D(s_normal1, 10);
SAMPLER2D(s_normal2, 11);
SAMPLER2D(s_normal3, 12);

uniform vec4 u_lightDirIntensity;
uniform vec4 u_lightColor;
uniform vec4 u_terrainLayerColors[4];
uniform vec4 u_terrainParams;
uniform vec4 u_shadowParams;
uniform vec4 u_shadowFilterParams;
uniform vec4 u_cascadeSplits;
uniform vec4 u_environmentColors[4];
uniform vec4 u_fogParams;
uniform vec4 u_weatherParams;

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

vec3 applyDistanceFog(vec3 color, float viewDepth)
{
    float fogFactor = u_fogParams.x * saturate((viewDepth - u_fogParams.y) * u_fogParams.z);
    return mix(color, u_environmentColors[3].rgb, fogFactor);
}

vec3 applyWeatherWetness(vec3 color)
{
    float amount = clamp(u_weatherParams.x, 0.0, 1.0);
    float darkening = clamp(u_weatherParams.z, 0.0, 1.0);
    return color * (1.0 - amount * darkening);
}

vec3 unpackTerrainNormal(vec3 encodedNormal, float flipY)
{
    vec3 normal = encodedNormal * 2.0 - vec3_splat(1.0);
    if (flipY > 0.5)
    {
        normal.y = -normal.y;
    }
    return normalize(normal);
}

vec3 applyTerrainNormalMap(vec3 geometricNormal, vec2 uv, vec4 weights)
{
    float strength = clamp(u_terrainParams.y, 0.0, 1.0);
    vec3 tangent = vec3(1.0, 0.0, 0.0);
    tangent = tangent - geometricNormal * dot(tangent, geometricNormal);
    float tangentLength = dot(tangent, tangent);
    if (tangentLength <= 0.0001)
    {
        tangent = vec3(0.0, 0.0, 1.0);
        tangent = tangent - geometricNormal * dot(tangent, geometricNormal);
        tangentLength = max(dot(tangent, tangent), 0.0001);
    }
    tangent *= 1.0 / sqrt(tangentLength);
    vec3 bitangent = normalize(cross(tangent, geometricNormal));

    float flipY = u_terrainParams.z;
    vec3 normal0 = unpackTerrainNormal(texture2D(s_normal0, uv).rgb, flipY);
    vec3 normal1 = unpackTerrainNormal(texture2D(s_normal1, uv).rgb, flipY);
    vec3 normal2 = unpackTerrainNormal(texture2D(s_normal2, uv).rgb, flipY);
    vec3 normal3 = unpackTerrainNormal(texture2D(s_normal3, uv).rgb, flipY);
    vec3 tangentNormal = normalize(
        normal0 * weights.r +
        normal1 * weights.g +
        normal2 * weights.b +
        normal3 * weights.a);
    vec3 mappedNormal = normalize(
        tangent * tangentNormal.x +
        bitangent * tangentNormal.y +
        geometricNormal * tangentNormal.z);
    return normalize(mix(geometricNormal, mappedNormal, strength));
}

void main()
{
    vec2 uv = v_texcoord0 * u_terrainParams.x;
    vec4 weights = texture2D(s_splat, uv);
    float totalWeight = max(dot(weights, vec4_splat(1.0)), 0.0001);
    weights /= totalWeight;

    vec3 layer0 = texture2D(s_layer0, uv).rgb * u_terrainLayerColors[0].rgb;
    vec3 layer1 = texture2D(s_layer1, uv).rgb * u_terrainLayerColors[1].rgb;
    vec3 layer2 = texture2D(s_layer2, uv).rgb * u_terrainLayerColors[2].rgb;
    vec3 layer3 = texture2D(s_layer3, uv).rgb * u_terrainLayerColors[3].rgb;
    vec3 albedo =
        layer0 * weights.r +
        layer1 * weights.g +
        layer2 * weights.b +
        layer3 * weights.a;

    vec3 normal = applyTerrainNormalMap(normalize(v_normal), uv, weights);
    vec3 lightDirection = normalize(u_lightDirIntensity.xyz);
    float diffuse = max(dot(normal, lightDirection), 0.0) * u_lightDirIntensity.w;
    float receiverBias = u_shadowParams.y + u_shadowFilterParams.y * (1.0 - max(dot(normal, lightDirection), 0.0));
    float filterMode = u_shadowFilterParams.z;
    float shadowTexelSize = u_shadowFilterParams.w;
    float shadowFactor = 1.0;
    int cascadeCount = int(clamp(u_shadowParams.x, 0.0, 4.0));
    if (cascadeCount > 0)
    {
        int cascadeIndex = 0;
        vec4 shadowPosition = v_shadowcoord;
        float previousFar = 0.0;
        float selectedFar = u_cascadeSplits.x;
        if (cascadeCount > 1 && v_viewdepth > u_cascadeSplits.x)
        {
            cascadeIndex = 1;
            shadowPosition = v_shadowcoord1;
            previousFar = u_cascadeSplits.x;
            selectedFar = u_cascadeSplits.y;
        }
        if (cascadeCount > 2 && v_viewdepth > u_cascadeSplits.y)
        {
            cascadeIndex = 2;
            shadowPosition = v_shadowcoord2;
            previousFar = u_cascadeSplits.y;
            selectedFar = u_cascadeSplits.z;
        }
        if (cascadeCount > 3 && v_viewdepth > u_cascadeSplits.z)
        {
            cascadeIndex = 3;
            shadowPosition = v_shadowcoord3;
            previousFar = u_cascadeSplits.z;
            selectedFar = u_cascadeSplits.w;
        }

        vec3 shadowCoord = shadowPosition.xyz / shadowPosition.w;
        bool insideShadowMap =
            v_viewdepth <= selectedFar &&
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
            float currentDepth = shadowCoord.z - receiverBias;
            float visibility = 1.0;
            if (cascadeIndex == 0)
            {
                visibility = sampleShadowMap0(shadowUv, currentDepth, filterMode, shadowTexelSize);
            }
            else if (cascadeIndex == 1)
            {
                visibility = sampleShadowMap1(shadowUv, currentDepth, filterMode, shadowTexelSize);
            }
            else if (cascadeIndex == 2)
            {
                visibility = sampleShadowMap2(shadowUv, currentDepth, filterMode, shadowTexelSize);
            }
            else
            {
                visibility = sampleShadowMap3(shadowUv, currentDepth, filterMode, shadowTexelSize);
            }
            shadowFactor = mix(1.0 - u_shadowParams.z, 1.0, visibility);
        }

        float blendFraction = clamp(u_shadowFilterParams.x, 0.0, 0.5);
        if (blendFraction > 0.0 && cascadeIndex + 1 < cascadeCount)
        {
            float cascadeSpan = max(selectedFar - previousFar, 0.0001);
            float blendSize = cascadeSpan * blendFraction;
            float blendStart = selectedFar - blendSize;
            if (v_viewdepth >= blendStart && v_viewdepth <= selectedFar)
            {
                vec4 nextShadowPosition = v_shadowcoord1;
                float nextFar = u_cascadeSplits.y;
                if (cascadeIndex == 1)
                {
                    nextShadowPosition = v_shadowcoord2;
                    nextFar = u_cascadeSplits.z;
                }
                else if (cascadeIndex == 2)
                {
                    nextShadowPosition = v_shadowcoord3;
                    nextFar = u_cascadeSplits.w;
                }

                vec3 nextShadowCoord = nextShadowPosition.xyz / nextShadowPosition.w;
                bool nextInsideShadowMap =
                    v_viewdepth <= nextFar &&
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
                    float nextCurrentDepth = nextShadowCoord.z - receiverBias;
                    float nextVisibility = 1.0;
                    if (cascadeIndex == 0)
                    {
                        nextVisibility = sampleShadowMap1(nextShadowUv, nextCurrentDepth, filterMode, shadowTexelSize);
                    }
                    else if (cascadeIndex == 1)
                    {
                        nextVisibility = sampleShadowMap2(nextShadowUv, nextCurrentDepth, filterMode, shadowTexelSize);
                    }
                    else
                    {
                        nextVisibility = sampleShadowMap3(nextShadowUv, nextCurrentDepth, filterMode, shadowTexelSize);
                    }
                    float nextShadowFactor = mix(1.0 - u_shadowParams.z, 1.0, nextVisibility);
                    float blendT = clamp((v_viewdepth - blendStart) / blendSize, 0.0, 1.0);
                    blendT = blendT * blendT * (3.0 - 2.0 * blendT);
                    shadowFactor = mix(shadowFactor, nextShadowFactor, blendT);
                }
            }
        }
    }
    vec3 litColor = albedo * v_color0.rgb * u_lightColor.rgb * max(diffuse * shadowFactor, 0.18);
    litColor = applyWeatherWetness(litColor);
    litColor = applyDistanceFog(litColor, v_viewdepth);
    gl_FragColor = vec4(linearToGamma(litColor), v_color0.a);
}
