$input v_shadowdepth

#include <bgfx_shader.sh>

void main()
{
    float depth = clamp(v_shadowdepth, 0.0, 1.0);
    gl_FragColor = vec4(depth, depth, depth, 1.0);
}
