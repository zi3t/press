#version 330
// signal.ts shine ring, unchanged but for GLSL 3 names.
in vec2 vUv;
out vec4 fragColor;
uniform float uTime;
void main() {
  float r = length((vUv - .5) * 2.0);
  float p = fract((r - .44) * 1.8 - uTime * .1);
  float a = smoothstep(.25, .4, p) * (1.0 - smoothstep(.4, .7, p));
  fragColor = vec4(1.0, .7, .44, a * .15);
}
