$input a_position, a_normal, a_color0, a_texcoord1, a_texcoord2
$output v_viewdepth

#include <bgfx_shader.sh>

uniform mat4 u_skinningPalette[64];

int skinIndex(float value)
{
    return int(clamp(value + 0.5, 0.0, 63.0));
}

void main()
{
    vec4 localPosition = vec4(a_position, 1.0);
    int joint0 = skinIndex(a_texcoord1.x);
    int joint1 = skinIndex(a_texcoord1.y);
    int joint2 = skinIndex(a_texcoord1.z);
    int joint3 = skinIndex(a_texcoord1.w);

    vec4 skinnedPosition =
        mul(u_skinningPalette[joint0], localPosition) * a_texcoord2.x +
        mul(u_skinningPalette[joint1], localPosition) * a_texcoord2.y +
        mul(u_skinningPalette[joint2], localPosition) * a_texcoord2.z +
        mul(u_skinningPalette[joint3], localPosition) * a_texcoord2.w;

    vec4 worldPosition = mul(u_model[0], skinnedPosition);
    vec4 viewPosition = mul(u_view, worldPosition);
    gl_Position = mul(u_viewProj, worldPosition);
    v_viewdepth = max(-viewPosition.z, 0.0);
}
