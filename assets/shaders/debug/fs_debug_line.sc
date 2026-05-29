$input v_color0

#include <bgfx_shader.sh>

void main()
{
    gl_FragColor = vec4(sqrt(max(v_color0.rgb, vec3(0.0, 0.0, 0.0))), v_color0.a);
}
