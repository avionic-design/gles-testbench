precision mediump float;
varying vec2 vTexcoord;
uniform sampler2D s_tex;
uniform float line_height;
uniform vec3 add;
uniform vec3 factor;

void main()
{
    float r, g, b;
    r = (texture2D(s_tex, vTexcoord).r + add.x/256.0) * factor.x;
    g = (texture2D(s_tex, vTexcoord).g + add.y/256.0) * factor.y;
    b = (texture2D(s_tex, vTexcoord).b + add.z/256.0) * factor.z;
    gl_FragColor = vec4(r, g, b, 1.0);
}
