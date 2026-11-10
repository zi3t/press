#version 330
// MeshStandardMaterial (no maps) under one ambient and two directional
// lights: three.js's physical model (GGX, Smith-correlated visibility,
// Schlick Fresnel, roughness widened by the normal's screen-space change),
// with colour management off and no tone mapping.
in vec3 vNormal;
in vec3 vViewPosition;
out vec4 fragColor;
uniform vec3 diffuse;
uniform float roughness;
uniform float metalness;
uniform vec3 ambientLightColor;
uniform vec3 directionalDirection[2];
uniform vec3 directionalColor[2];
#define RECIPROCAL_PI 0.3183098861837907
#define EPSILON 1e-6
#define saturate(a) clamp(a, 0.0, 1.0)
float pow2(float x) { return x * x; }
vec3 F_Schlick(vec3 f0, float f90, float dotVH) {
  float fresnel = exp2((-5.55473 * dotVH - 6.98316) * dotVH);
  return f0 * (1.0 - fresnel) + (f90 * fresnel);
}
float V_GGX_SmithCorrelated(float alpha, float dotNL, float dotNV) {
  float a2 = pow2(alpha);
  float gv = dotNL * sqrt(a2 + (1.0 - a2) * pow2(dotNV));
  float gl = dotNV * sqrt(a2 + (1.0 - a2) * pow2(dotNL));
  return 0.5 / max(gv + gl, EPSILON);
}
float D_GGX(float alpha, float dotNH) {
  float a2 = pow2(alpha);
  float denom = pow2(dotNH) * (a2 - 1.0) + 1.0;
  return RECIPROCAL_PI * a2 / pow2(denom);
}
vec3 BRDF_GGX(vec3 lightDir, vec3 viewDir, vec3 normal, vec3 f0, float f90, float rough) {
  float alpha = pow2(rough);
  vec3 halfDir = normalize(lightDir + viewDir);
  float dotNL = saturate(dot(normal, lightDir));
  float dotNV = saturate(dot(normal, viewDir));
  float dotNH = saturate(dot(normal, halfDir));
  float dotVH = saturate(dot(viewDir, halfDir));
  vec3 F = F_Schlick(f0, f90, dotVH);
  float V = V_GGX_SmithCorrelated(alpha, dotNL, dotNV);
  float D = D_GGX(alpha, dotNH);
  return F * (V * D);
}
void main() {
  vec3 normal = normalize(vNormal);  // FrontSide: back faces are culled, never flipped
  vec3 diffuseColor = diffuse * (1.0 - metalness);
  vec3 dxy = max(abs(dFdx(normal)), abs(dFdy(normal)));
  float geometryRoughness = max(max(dxy.x, dxy.y), dxy.z);
  float rough = min(max(roughness, 0.0525) + geometryRoughness, 1.0);
  vec3 specularColor = mix(vec3(0.04), diffuse, metalness);
  vec3 viewDir = normalize(vViewPosition);
  vec3 light = ambientLightColor * RECIPROCAL_PI * diffuseColor;
  for (int i = 0; i < 2; i++) {
    float dotNL = saturate(dot(normal, directionalDirection[i]));
    vec3 irradiance = dotNL * directionalColor[i];
    light += irradiance * RECIPROCAL_PI * diffuseColor;
    light += irradiance * BRDF_GGX(directionalDirection[i], viewDir, normal, specularColor, 1.0, rough);
  }
  fragColor = vec4(light, 1.0);
}
