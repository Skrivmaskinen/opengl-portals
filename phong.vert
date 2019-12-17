#version 150

in  vec3 in_Position;
in  vec3 in_Normal;
out vec3 exNormal; // Phong
out vec3 exSurface; // Phong (specular)
out vec3 f_lightPoint;

uniform vec3 lightPoint;
uniform mat4 modelviewMatrix;
uniform mat4 projectionMatrix;

void main(void)
{
	exNormal = inverse(transpose(mat3(modelviewMatrix))) * in_Normal; // Phong, "fake" normal transformation

	exSurface = vec3(modelviewMatrix * vec4(in_Position, 1.0)); // Don't include projection here - we only want to go to view coordinates
	f_lightPoint = vec3(projectionMatrix * vec4(lightPoint, 1));
	gl_Position = projectionMatrix * modelviewMatrix * vec4(in_Position, 1.0); // This should include projection
}
