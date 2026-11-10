#version 330
// View-space normal, position and uv for the lit signal meshes.
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
uniform mat4 matModel;
uniform mat4 matView;
uniform mat4 matProjection;
out vec2 vUv;
out vec3 vNormal;
out vec3 vViewPosition;
void main() {
  vUv = vertexTexCoord;
  mat4 modelView = matView * matModel;
  vNormal = normalize(transpose(inverse(mat3(modelView))) * vertexNormal);
  vec4 mvPosition = modelView * vec4(vertexPosition, 1.0);
  vViewPosition = -mvPosition.xyz;
  gl_Position = matProjection * mvPosition;
}
