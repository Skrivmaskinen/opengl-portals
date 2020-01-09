#version 150

// Simplified Phong: No materials, only one, hard coded light source
// (in view coordinates) and no ambient

// Note: Simplified! In particular, the light source is given in view
// coordinates, which means that it will follow the camera.
// You usually give light sources in world coordinates.

out vec4 outColor;
in vec3 exNormal; // Phong
in vec3 exSurface; // Phong (specular)
in vec4 myPosition;


void main(void)
{
    const vec3 light = vec3(0, 0.7, 0);

    vec3 lightDirection = normalize(light - exSurface);
    float intensity = dot(lightDirection, exNormal);
    outColor = vec4(intensity, intensity, intensity, 1.0);
    //outColor = vec4(out_Position, 1);
}
