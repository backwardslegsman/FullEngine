$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_decalDepth, 12);
SAMPLER2D(s_decalAlbedo, 13);

uniform mat4 u_decalInvView;
uniform mat4 u_decalInvProj;
uniform mat4 u_decalWorldToLocal;
uniform vec4 u_decalColorOpacity;
uniform vec4 u_decalDepthParams;

void main()
{
    float normalizedDepth = texture2D(s_decalDepth, v_texcoord0).r;
    if (normalizedDepth <= 0.0001 || normalizedDepth >= 0.9999)
    {
        discard;
    }

    vec2 ndc = vec2(v_texcoord0.x * 2.0 - 1.0, 1.0 - v_texcoord0.y * 2.0);
    vec4 viewFar = mul(u_decalInvProj, vec4(ndc, 1.0, 1.0));
    if (abs(viewFar.w) <= 0.000001)
    {
        discard;
    }
    viewFar.xyz /= viewFar.w;

    float viewDepth = normalizedDepth * max(u_decalDepthParams.x, 0.0001);
    float rayDepth = max(-viewFar.z, 0.0001);
    vec4 viewPosition = vec4(viewFar.xyz * (viewDepth / rayDepth), 1.0);
    vec4 worldPosition = mul(u_decalInvView, viewPosition);
    vec4 decalPosition = mul(u_decalWorldToLocal, worldPosition);

    if (abs(decalPosition.x) > 1.0 || abs(decalPosition.y) > 1.0 || abs(decalPosition.z) > 1.0)
    {
        discard;
    }

    float projectionDepth = abs(decalPosition.y);
    float depthLimit = clamp(u_decalDepthParams.y, 0.0001, 1.0);
    if (projectionDepth > depthLimit)
    {
        discard;
    }

    vec2 decalUv = vec2(decalPosition.x * 0.5 + 0.5, 0.5 - decalPosition.z * 0.5);
    vec4 albedo = texture2D(s_decalAlbedo, decalUv) * vec4(u_decalColorOpacity.rgb, 1.0);
    float edgeFade = max(u_decalDepthParams.z, 0.0);
    float fadeAlpha = edgeFade > 0.0001 ? saturate((depthLimit - projectionDepth) / edgeFade) : 1.0;
    float alpha = saturate(albedo.a * u_decalColorOpacity.a * fadeAlpha);
    gl_FragColor = vec4(albedo.rgb, alpha);
}
