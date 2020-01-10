#version 150

uniform mat4 modelviewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 model2world;
uniform float time;

in  vec3 in_Position;
in  vec3 in_Normal;
out vec3 exNormal; // Phong
out vec3 exSurface; // Phong (specular)
out vec4 myPosition;
out float out_time;

void main(void)
{

    out_time = time;
    exNormal = inverse(transpose(mat3(modelviewMatrix))) * in_Normal; // Phong, "fake" normal transformation

    exSurface = vec3(model2world * vec4(in_Position, 1.0)); // Don't include projection here - we only want to go to view coordinates
    myPosition = vec4(in_Position, 1.0);
    gl_Position = projectionMatrix * modelviewMatrix * myPosition;// This should include projection
}
