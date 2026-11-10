#version 330
// MeshBasicMaterial with an sRGB map (decoded to linear, as three.js samples
// an SRGBColorSpace texture), a colour and an opacity. Output stays linear:
// the renderer has colour management off and no output transform.
in vec2 vUv;
out vec4 fragColor;
uniform sampler2D map;
uniform vec3 diffuse;
uniform float opacity;
uniform float useMap;
vec3 srgbToLinear(vec3 c) {
  return mix(c * 0.0773993808, pow(c * 0.9478672986 + vec3(0.0521327014), vec3(2.4)), vec3(greaterThan(c, vec3(0.04045))));
}
void main() {
  vec4 color = vec4(diffuse, opacity);
  if (useMap > 0.5) {
    vec4 texel = texture(map, vUv);
    color *= vec4(srgbToLinear(texel.rgb), texel.a);
  }
  fragColor = color;
}
