$input a_position, a_normal, a_color0, i_data0, i_data1, i_data2, i_data3
$output v_shadowdepth

#include <bgfx_shader.sh>

void main()
{
    mat4 model = mtxFromCols(i_data0, i_data1, i_data2, i_data3);
    vec4 worldPosition = mul(model, vec4(a_position, 1.0));
    vec4 shadowPosition = mul(u_viewProj, worldPosition);
    gl_Position = shadowPosition;
    v_shadowdepth = shadowPosition.z / shadowPosition.w;
}
