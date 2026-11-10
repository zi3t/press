#version 330
// The signal backdrop: a full-screen plane at the far edge of depth.
in vec3 vertexPosition;
in vec2 vertexTexCoord;
out vec2 vUv;
void main() { vUv = vertexTexCoord; gl_Position = vec4(vertexPosition.xy, 0.999, 1.0); }
