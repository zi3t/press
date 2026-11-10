#version 330
// The book's vertex stage. Each volume has its own thickness; the generated
// mesh (press_book.c) is BOOK_MODEL_THICKNESS thick, so scale it along x.
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;

uniform mat4 matModel;
uniform mat4 matView;
uniform mat4 matProjection;
uniform float thickness;

out vec2 vUv;
out vec3 vViewPosition;
out vec3 vNormal;

const float BOOK_MODEL_THICKNESS = 3.374;

void main() {
  vUv = vertexTexCoord;
  float widen = thickness / BOOK_MODEL_THICKNESS;
  vec3 position = vec3(vertexPosition.x * widen, vertexPosition.yz);
  // A normal under a scale s along x scales by 1 / s along x.
  vec3 normal = vec3(vertexNormal.x / widen, vertexNormal.yz);
  mat4 modelView = matView * matModel;
  vNormal = normalize(transpose(inverse(mat3(modelView))) * normal);
  vec4 viewPosition = modelView * vec4(position, 1.0);
  vViewPosition = -viewPosition.xyz;
  gl_Position = matProjection * viewPosition;
}
