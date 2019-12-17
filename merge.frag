#version 150

uniform sampler2D texUnit;
uniform sampler2D texUnit2;
uniform float texSize;


in vec2 f_TexCoord;
out vec4 out_Color;

//out vec4 outColor;
//in vec3 exNormal; // Phong
//in vec3 exSurface; // Phong (specular)

void main(void)
{
  vec4 fbo1 = texture(texUnit, f_TexCoord);
  vec4 fbo2 = texture(texUnit2, f_TexCoord);

  out_Color = fbo1 + fbo2;
}
