#version 310 es
precision highp float;
in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;
uniform SAMPLER_TYPE imageTex;
void main() {
  vec4 c = texture(imageTex, fragTexCoord);
  // Correct for chroma offset — external OES sampler outputs shifted values
  float r = c.r - 0.0625;
  float g = c.g - 0.0625;
  float b = c.b - 0.0625;
  outColor = vec4(clamp(r, 0.0, 1.0), clamp(g, 0.0, 1.0), clamp(b, 0.0, 1.0), 1.0);
}