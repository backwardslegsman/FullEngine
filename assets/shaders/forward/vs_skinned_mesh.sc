$input a_position, a_normal, a_color0, a_texcoord1, a_texcoord2
$output v_normal, v_color0, v_texcoord0, v_shadowcoord, v_shadowcoord1, v_shadowcoord2, v_shadowcoord3, v_viewdepth

#include <bgfx_shader.sh>

uniform mat4 u_shadowViewProj[4];
uniform mat4 u_skinningPalette[64];

int skinIndex(float value)
{
    return int(clamp(value + 0.5, 0.0, 63.0));
}

void main()
{
    vec4 localPosition = vec4(a_position, 1.0);
    vec4 localNormal = vec4(a_normal, 0.0);

    int joint0 = skinIndex(a_texcoord1.x);
    int joint1 = skinIndex(a_texcoord1.y);
    int joint2 = skinIndex(a_texcoord1.z);
    int joint3 = skinIndex(a_texcoord1.w);

    vec4 skinnedPosition =
        mul(u_skinningPalette[joint0], localPosition) * a_texcoord2.x +
        mul(u_skinningPalette[joint1], localPosition) * a_texcoord2.y +
        mul(u_skinningPalette[joint2], localPosition) * a_texcoord2.z +
        mul(u_skinningPalette[joint3], localPosition) * a_texcoord2.w;
    vec3 skinnedNormal =
        mul(u_skinningPalette[joint0], localNormal).xyz * a_texcoord2.x +
        mul(u_skinningPalette[joint1], localNormal).xyz * a_texcoord2.y +
        mul(u_skinningPalette[joint2], localNormal).xyz * a_texcoord2.z +
        mul(u_skinningPalette[joint3], localNormal).xyz * a_texcoord2.w;

    vec4 worldPosition = mul(u_model[0], skinnedPosition);
    vec4 viewPosition = mul(u_view, worldPosition);
    gl_Position = mul(u_viewProj, worldPosition);
    v_normal = normalize(mul(u_model[0], vec4(normalize(skinnedNormal), 0.0)).xyz);
    v_color0 = a_color0;
    v_texcoord0 = vec2_splat(0.0);
    v_shadowcoord = mul(u_shadowViewProj[0], worldPosition);
    v_shadowcoord1 = mul(u_shadowViewProj[1], worldPosition);
    v_shadowcoord2 = mul(u_shadowViewProj[2], worldPosition);
    v_shadowcoord3 = mul(u_shadowViewProj[3], worldPosition);
    v_viewdepth = max(-viewPosition.z, 0.0);
}
