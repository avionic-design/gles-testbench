precision mediump float;
varying vec2 vTexcoord;
uniform float line_height;

void main()
{
    vec3 color, red = vec3(1.0, 0.0, 0.0), blue = vec3(0.0, 0.0, 1.0);
    vec2 position, pattern, threshold = vec2(0.5);
    float frequency = 16.0;

    position = vTexcoord * frequency;
    position = fract(position);

    pattern = step(position, threshold);

    if (pattern.y > 0.0)
        color = mix(red, blue, pattern.x);
    else
        color = mix(blue, red, pattern.x);

    gl_FragColor = vec4(color, 1.0);
}
