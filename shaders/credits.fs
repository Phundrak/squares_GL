#version 330
uniform sampler2D tex;
uniform float alpha;
in  vec2 vsoTexCoord;
out vec4 fragColor;

void main(void) {
  vec4 c = texture(tex, vsoTexCoord);
  vec3 color = vec4(c.rgb, length(c.rgb) > 0.0 ? 1.0 : 0.0).xyz;
  fragColor =
      vec4(color, length(c.rgb) > 0.0 ? alpha : 0.0);
}
