precision mediump float;
varying vec2 vTexcoord;
uniform sampler2D s_tex;
uniform float line_height;

void main() {
	vec2 tmpcoord;
	vec2 tmpcoord2;
	vec4 factor = vec4(0.3, 0.3, 0.3, 1.0);
	vec4 sum;

	tmpcoord.x = vTexcoord.x;
	tmpcoord.y = vTexcoord.y + line_height;
	tmpcoord2.x = vTexcoord.x;
	tmpcoord2.y = vTexcoord.y - line_height;

	sum = texture2D(s_tex, vTexcoord) + texture2D(s_tex, tmpcoord) + texture2D(s_tex, tmpcoord2);

	gl_FragColor = sum * factor;
}
