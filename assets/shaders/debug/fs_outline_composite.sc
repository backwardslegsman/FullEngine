$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_selectionMask, 14);

uniform vec4 u_outlineColor;
uniform vec4 u_outlineParams;

void main()
{
    vec2 texel = u_outlineParams.xy;
    float radius = max(u_outlineParams.z, 0.0);
    vec2 offset = texel * radius;

    float center = texture2D(s_selectionMask, v_texcoord0).r;
    float neighbor = 0.0;
    neighbor = max(neighbor, texture2D(s_selectionMask, v_texcoord0 + vec2(offset.x, 0.0)).r);
    neighbor = max(neighbor, texture2D(s_selectionMask, v_texcoord0 + vec2(-offset.x, 0.0)).r);
    neighbor = max(neighbor, texture2D(s_selectionMask, v_texcoord0 + vec2(0.0, offset.y)).r);
    neighbor = max(neighbor, texture2D(s_selectionMask, v_texcoord0 + vec2(0.0, -offset.y)).r);
    neighbor = max(neighbor, texture2D(s_selectionMask, v_texcoord0 + offset).r);
    neighbor = max(neighbor, texture2D(s_selectionMask, v_texcoord0 - offset).r);
    neighbor = max(neighbor, texture2D(s_selectionMask, v_texcoord0 + vec2(offset.x, -offset.y)).r);
    neighbor = max(neighbor, texture2D(s_selectionMask, v_texcoord0 + vec2(-offset.x, offset.y)).r);

    float edge = saturate(neighbor - center);
    gl_FragColor = vec4(u_outlineColor.rgb, u_outlineColor.a * edge);
}
