precision mediump float;
varying vec2 vTexcoord;
uniform sampler2D s_tex;
uniform float line_height;

void main()
{
    gl_FragColor = vec4(texture2D(s_tex, vec2(0.5, 0.5)).rgb, 1.0);
}
