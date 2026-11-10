#version 330
// signal.ts orb, unchanged but for GLSL 3 names.
in vec3 vNormal;
in vec2 vUv;
out vec4 fragColor;
void main() {
  float contours = sin(vUv.x * 31.4 + sin(vUv.y * 21.0) * 1.8) * sin(vUv.y * 17.0 + cos(vUv.x * 19.0));
  vec3 blue = vec3(.13, .45, .46), gold = vec3(.69, .64, .38);
  vec3 c = mix(blue, gold, smoothstep(-.1, .16, contours));
  vec2 grid = abs(fract(vUv * vec2(24.0, 12.0)) - .5);
  c *= 1.0 - .35 * step(.475, max(grid.x, grid.y));
  float light = .42 + .58 * max(0.0, dot(normalize(vNormal), normalize(vec3(-.3, .6, 1.0))));
  fragColor = vec4(c * light, 1.0);
}
