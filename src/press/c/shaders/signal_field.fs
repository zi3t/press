#version 330
// signal.ts backdrop fragment shader, unchanged but for GLSL 3 names.
in vec2 vUv;
out vec4 fragColor;
uniform float uTime, uAspect, uEntry, uSplit;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
vec3 relief(vec2 p) {
  // Authored abstract infrastructure: terraces, grain and scored edges.
  vec2 iso = vec2(p.x + p.y * .62, p.y * 1.6) * 19.0;
  vec2 cell = floor(iso), f = fract(iso);
  float h = hash(cell);
  float edge = step(.075, f.x) * step(.07, f.y);
  float terrace = floor(f.y * (4.0 + floor(h * 8.0))) / 12.0;
  float grain = hash(floor(p * 1700.0)) * .07;
  float light = (.14 + h * .34 + terrace * .16 + grain) * (.35 + .65 * edge);
  return vec3(.42, .39, .31) * light;
}
void main() {
  vec2 p = (vUv - .5) * vec2(uAspect, 1.0);
  float r = length(p);
  float phase = fract(r * 4.15 - uTime * .075);
  float band = smoothstep(.045, .065, phase) * (1.0 - smoothstep(.44, .46, phase));
  vec2 radial = p / max(r, .001);
  vec3 ground = relief(vUv);
  vec3 wave = relief(vUv + radial * .035 * sin(phase * 6.283));
  float reveal = smoothstep(.82, 1.0, uEntry);
  vec3 color = mix(ground * .4, mix(ground * .28, wave * 1.7, band), reveal);
  color = mix(color, ground * .46, uSplit);
  color *= 1.0 - .3 * smoothstep(.2, 1.0, r);
  fragColor = vec4(color, 1.0);
}
