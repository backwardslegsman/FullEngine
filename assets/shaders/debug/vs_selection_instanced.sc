$input a_position, i_data0, i_data1, i_data2, i_data3

#include <bgfx_shader.sh>

void main()
{
    mat4 model = mat4(i_data0, i_data1, i_data2, i_data3);
    vec4 worldPosition = mul(model, vec4(a_position, 1.0));
    gl_Position = mul(u_viewProj, worldPosition);
}
