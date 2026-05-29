$input a_position
$output v_skyfactor

#include <bgfx_shader.sh>

void main()
{
    gl_Position = vec4(a_position.xy, 0.0, 1.0);
    v_skyfactor = saturate(a_position.y * 0.5 + 0.5);
}
