#version 330
// MeshBasicMaterial vertex stage: position and uv only.
in vec3 vertexPosition;
in vec2 vertexTexCoord;
uniform mat4 mvp;
out vec2 vUv;
void main() {
  vUv = vertexTexCoord;
  gl_Position = mvp * vec4(vertexPosition, 1.0);
}
