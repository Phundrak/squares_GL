#version 330
uniform sampler2D myTexture;
uniform vec3 lum;
uniform vec4 couleur;
in  vec3 vsoNormal;
out vec4 fragColor;

void main(void) {
  // normale
  vec3 normal = normalize(vsoNormal);
  vec3 light_source = normalize(vec3(-1, -1, -1));

  // ambiante
  vec3 ambient = couleur.rgb * 0.05;

  // diffuse
  float diffuseCoef = max(0.0, dot(normal, -light_source));
  vec3 diffuse = diffuseCoef * couleur.rgb * 0.04;

  // attenuation
  float attenuation = 20.0;
  vec3 linearColor = ambient + attenuation * (diffuse);

  vec3 gamma = vec3(1.0/2.2);
  fragColor = vec4(pow(linearColor, gamma), couleur.a);
}
