#version 150

uniform sampler2D texUnit;
uniform float texSize;


in vec2 f_TexCoord;
out vec4 out_Color;

//out vec4 outColor;
//in vec3 exNormal; // Phong
//in vec3 exSurface; // Phong (specular)

void main(void)
{

  // render texture - 1
  // Clamp between [0, 1]
  out_Color = min(max(texture(texUnit, f_TexCoord) - 1, 0), 1);
}
