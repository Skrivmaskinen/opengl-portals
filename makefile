all :  project

project: project.c  ../common/GL_utilities.c ../common/VectorUtils3.c ../common/LoadTGA.c ../common/loadobj.c ../common/zpr.c ../common/Linux/MicroGlut.c perlin_noise.c
	gcc -Wall -o project -DGL_GLEXT_PROTOTYPES project.c ../common/GL_utilities.c ../common/VectorUtils3.c ../common/LoadTGA.c ../common/loadobj.c ../common/zpr.c ../common/Linux/MicroGlut.c -I../common -I../common/Linux perlin_noise.c -lXt -lX11 -lm -lGL

clean :
	rm project

