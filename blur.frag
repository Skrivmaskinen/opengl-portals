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
  float step_size = 1.0/512; // texSize = size of texture

  int blur_size = 7; // size of filter

  out_Color = vec4(0, 0, 0, 0);
  for(int i = -3; i < 4; ++i)
  {
    for(int j = -3; j < 4; ++j)
    {
      out_Color += texture(texUnit, f_TexCoord +vec2(step_size*i, step_size*j));
    }
  }

  out_Color = out_Color/(blur_size*blur_size);
  //out_Color = texture(texUnit, f_TexCoord+vec2(0.5*step_size, 0) );
}
