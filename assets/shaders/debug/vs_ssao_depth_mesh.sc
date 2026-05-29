$input a_position
$output v_viewdepth

#include <bgfx_shader.sh>

void main()
{
    vec4 worldPosition = mul(u_model[0], vec4(a_position, 1.0));
    vec4 viewPosition = mul(u_view, worldPosition);
    gl_Position = mul(u_viewProj, worldPosition);
    v_viewdepth = max(-viewPosition.z, 0.0);
}
