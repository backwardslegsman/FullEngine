$input a_position, a_normal, a_tangent, a_color0, a_texcoord0
$output v_normal, v_color0, v_texcoord0, v_shadowcoord, v_shadowcoord1, v_shadowcoord2, v_shadowcoord3, v_viewdepth

#include <bgfx_shader.sh>

uniform mat4 u_shadowViewProj[4];

void main()
{
    vec4 worldPosition = mul(u_model[0], vec4(a_position, 1.0));
    vec4 viewPosition = mul(u_view, worldPosition);
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_normal = normalize(mul(u_model[0], vec4(a_normal, 0.0)).xyz);
    v_color0 = a_color0;
    v_texcoord0 = a_texcoord0;
    v_shadowcoord = mul(u_shadowViewProj[0], worldPosition);
    v_shadowcoord1 = mul(u_shadowViewProj[1], worldPosition);
    v_shadowcoord2 = mul(u_shadowViewProj[2], worldPosition);
    v_shadowcoord3 = mul(u_shadowViewProj[3], worldPosition);
    v_viewdepth = max(-viewPosition.z, 0.0);
}
