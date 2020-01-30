
/* Original code by John Nowl
 * Found at: https://gist.github.com/nowl/828013
 * Document maintained by: Alexander Uggla
*/

#include <stdio.h>

int noise2(int x, int y);
float lin_inter(float x, float y, float s);

float smooth_inter(float x, float y, float s);

float noise2d(float x, float y);

float perlin2d(float x, float y, float freq, int depth);

