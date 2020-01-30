// Lab 1-1, multi-pass rendering with FBOs and HDR.
// Revision for 2013, simpler light, start with rendering on quad in final stage.
// Switched to low-res Stanford Bunny for more details.
// No HDR is implemented to begin with. That is your task.


// You can compile like this:
// gcc lab1-1.c ../common/*.c -lGL -o lab1-1 -I../common

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include "perlin_noise.h"
#ifdef __APPLE__
// Mac
	#include <OpenGL/gl3.h>
	#include "MicroGlut.h"
	// uses framework Cocoa
#else
	#ifdef WIN32
// MS
		#include <windows.h>
		#include <stdio.h>
		#include <GL/glew.h>
		#include <GL/glut.h>
	#else
// Linux
		#include <stdio.h>
		#include <GL/gl.h>
		#include "MicroGlut.h"
//		#include <GL/glut.h>
	#endif
#endif

#include "VectorUtils3.h"
#include "GL_utilities.h"
#include "loadobj.h"
#include "zpr.h"


mat4 projectionMatrix;
mat4 viewMatrix;

GLfloat time;
GLfloat dTime;
vec3 ZERO_VECTOR;
vec3 UP_VECTOR;
vec3 RIGHT_VECTOR;
vec3 FRONT_VECTOR;

GLfloat square[] = {
        -1,-1,0,
        -1,1, 0,
        1,1, 0,
        1,-1, 0};

GLfloat squareTexCoord[] = {
        0, 0,
        0, 1,
        1, 1,
        1, 0};
GLuint squareIndices[] = {0, 1, 2, 0, 2, 3};

// initial width and heights
#define W 1000
#define H 750

#define NUM_LIGHTS 1
#define PI 3.1415

void OnTimer(int value);

////////////////////////////////////////////////////////////////////////////////////
/*                              Globals                                           */
////////////////////////////////////////////////////////////////////////////////////

vec3 ambientLightColor;
vec3 flameIllumination;
Model* squareModel;
Point3D cam, point;
Model *model1, *cube, *teapot;
GLuint ground_shader = 0, fire_shader = 0, log_shader = 0, shadow_shader = 0;


// --------------------Scene object globals------------------------------------------
struct SceneObject *bunnyObject, *cubeFloorObject, *screenObject,
        *visionObject, *openingObject, *openingFrameObject, *playerObject,
        *smallBunnyObject, *smallerBunnyObject, *scene1Root;


////////////////////////////////////////////////////////////////////////////////////
/*                                Struct definitions                              */
////////////////////////////////////////////////////////////////////////////////////

typedef struct Camera
{
    /* Camera Properties */
    vec3 position;
    vec3 positionOffset;
    vec3 target;
    vec3 targetOffset;
    mat4 world2view;

    /* Frustum */
    float fovX; // Degrees
    float fovY; // Degrees
    float zNear;
    float zFar;
    mat4 projectionMatrix;


}Camera, *CameraPtr;

// A visible object in the scene
typedef struct SceneObject
{
    Model* model;
    GLuint shader;
    vec3 position;
    vec3 rotation;
    vec3 scaleV;
    struct SceneObject *next;

}SceneObject, *SceneObjectPtr;

SceneObject* fireRoot;
////////////////////////////////////////////////////////////////////////////////////
/*                                Header                                          */
////////////////////////////////////////////////////////////////////////////////////
// Constructors
struct Camera*      newCamera(vec3 inPosition, vec3 inTarget);
struct SceneObject* newSceneObject(Model* inModel, GLuint inShader, vec3 inPosition, float inScale, SceneObject *root);

void enlistSceneObject(SceneObject *root, SceneObject *newSceneObject);
vec3 getRotationOfNormal(vec3 theVector, vec3 x_axis, vec3 y_axis);

////////////////////////////////////////////////////////////////////////////////////
/*                                Cameras                                         */
////////////////////////////////////////////////////////////////////////////////////

// Recalculate the projection matrix of a given camera. Used upon initialization and should be used after changing
// frustum attributes.
void calcProjection(Camera* this)
{
    this->projectionMatrix = perspective(this->fovY, this->fovX / this->fovY, this->zNear, this->zFar);
}

// Recalculate the cameras position and view-direction. Used upon initialization and should be used after changing
// position or target of camera.
void calcWorld2View(Camera* this)
{
    // Create the world2view
    this->world2view = lookAtv(VectorAdd(this->positionOffset, this->position), VectorAdd(this->position, this->targetOffset), UP_VECTOR);

}

// Create a new camera struct and return a pointer to it.
struct Camera* newCamera(vec3 inPosition, vec3 inTarget)
{
    // inPosition:  the position from which the camera looks from.
    // inTarget:    the point that the camera looks at.
    //////////////////////////////////////////////////////
    // Allocate memory for a new camera.
    Camera* this;
    this = (Camera *)malloc(sizeof(Camera));

    // Camera attributes
    this->position = inPosition;
    this->positionOffset = ZERO_VECTOR;

    this->target = inTarget;
    this->targetOffset = ZERO_VECTOR;

    // Matrixify
    calcWorld2View(this);

    // Frustum attributes
    this->fovX = 90*W/H;
    this->fovY = 90;
    this->zNear = 0.001;
    this->zFar = 1000;
    // Matrixify
    calcProjection(this);

    return this;

}

// Add a vector 'addedPosition' to camera 'this' position.
void changeCameraPosition(Camera *this, vec3 *addedPosition)
{
    this->position = VectorAdd(this->position, *addedPosition);
    calcWorld2View(this);
}

// Set the Cameras position to 'newPosition'.
void setCameraPosition(Camera *this, vec3 *newPosition)
{
    this->position = *newPosition;
    calcWorld2View(this);
}

// Move the camera forward 'amount' meters.
// The movement vector is the xz-direction from the cameras position to its target.
void moveCamForward(Camera *this, float amount)
{
    vec3 moveVector = VectorSub(this->target, this->position);
    moveVector.y = 0;
    moveVector = Normalize(moveVector);
    moveVector = ScalarMult(moveVector, amount);
    this->position = VectorAdd(this->position, moveVector);
    this->target = VectorAdd(this->target, moveVector);
    calcWorld2View(this);
}

// Set camera 'this' target to 'newTarget'.
void setCameraTarget(Camera *this, vec3 *newTarget)
{
    this->target = *newTarget;
    calcWorld2View(this);
}

// Rotate the camera along the y-axis 'rotationRadian' radians.
void rotateCamera(Camera *this, float rotationRadian)
{
    vec3 newTarget = VectorSub(this->target, this->position);
    newTarget = MultVec3(ArbRotate(UP_VECTOR, rotationRadian), newTarget);
    newTarget = VectorAdd(newTarget, this->position);
    setCameraTarget(this, &newTarget);
}

void strafeCamera(Camera *this, float amount)
{
    vec3 lookingDirection = Normalize(VectorSub(this->target, this->position));
    vec3 strafeDirection = CrossProduct(lookingDirection, UP_VECTOR);
    strafeDirection = ScalarMult(strafeDirection, amount);
    this->position = VectorAdd(this->position, strafeDirection);
    this->target = VectorAdd(this->target, strafeDirection);
    calcWorld2View(this);
}

/* Some code about perspectives for reference.
 *
    perspective(90, 1.0, 0.1, 1000);
    mat4 perspective(float fovyInDegrees, float aspectRatio, float znear, float zfar);
*/
// --------------------Camera globals-----------------------------------------------

struct Camera  *playerCamera, *surveillanceCamera;


////////////////////////////////////////////////////////////////////////////////////
/*                         SceneObject                                            */
////////////////////////////////////////////////////////////////////////////////////
struct SceneObject* newSceneObject(Model* inModel, GLuint inShader, vec3 inPosition, float inScale, SceneObject *root)
{
    // Allocate memory
    SceneObject* this;
    this = (SceneObject *)malloc(sizeof(SceneObject));

    // Set input
    this->model     = inModel;
    this->shader    = inShader;
    this->position  = inPosition;
    this->scaleV    = SetVector(inScale, inScale, inScale);
    this->rotation  = SetVector(0, 0, 0);
    this->next      = NULL;


    if (root != NULL)
    {
        enlistSceneObject(root, this);
    }

    // Return the created sceneObject
    return this;
}

void enlistSceneObject(SceneObject *root, SceneObject *newSceneObject)
{
    newSceneObject->next = root->next;
    root->next = newSceneObject;
}

void renderSceneObject(SceneObject* in, Camera* activeCam)
{
    // in: the SceneObject to render.
    // activeCam: the camera perspective to render to.
    //////////////////////////////////////

    // <<Activate shader program>>
    glUseProgram(in->shader);
    //------------------------------------
    // <<Matrix creation>>
    // Correctly placed in the WORLD
    mat4 model2world;

    model2world = T(in->position.x, in->position.y, in->position.z);
    model2world = Mult(model2world, Rx(in->rotation.x));
    model2world = Mult(model2world, Ry(in->rotation.y));
    model2world = Mult(model2world, Rz(in->rotation.z));

    model2world = Mult(model2world, S(in->scaleV.x, in->scaleV.y, in->scaleV.z));
    /*
    mat4 model2world;
    model2world = Rx(in->rotation.x);
    model2world = Mult(model2world, Ry(in->rotation.y));
    model2world = Mult(model2world, Rz(in->rotation.z));

    model2world = Mult(model2world, T(in->position.x, in->position.y, in->position.z));
    model2world = Mult(model2world, S(in->scaleV.x, in->scaleV.y, in->scaleV.z));
    */
    // Correctly placed in the SCENE
    mat4 model2view;
    model2view = Mult(activeCam->world2view, model2world);
    //------------------------------------

    // <<Send information>>
    glUniform1f(glGetUniformLocation(in->shader, "time"), time);
    glUniformMatrix4fv(glGetUniformLocation(in->shader, "projectionMatrix"), 1, GL_TRUE, activeCam->projectionMatrix.m);
    glUniformMatrix4fv(glGetUniformLocation(in->shader, "model2view"), 1, GL_TRUE, model2view.m);
    glUniformMatrix4fv(glGetUniformLocation(in->shader, "model2world"), 1, GL_TRUE, model2world.m);
    glUniform3fv(glGetUniformLocation(in->shader, "objectCenter"), 1, &(in->position.x));
    glUniform3fv(glGetUniformLocation(in->shader, "camPos"), 1, &(activeCam->position.x) );
    glUniform1i(glGetUniformLocation(in->shader, "texUnit"), 0);


    // Lighting

    glUniform3fv(glGetUniformLocation(in->shader, "ambientColor"), 1, &(ambientLightColor.x));

    SceneObject * currentFire = fireRoot;
    int count = 0;
    while(currentFire->next != NULL)
    {
        // Advance in list
        currentFire = currentFire->next;
        count = count + 1;

        char pointName[40];
        char colorName[40];
        sprintf(pointName, "lightPoint%d", count);
        glUniform3fv(glGetUniformLocation(in->shader, pointName), 1, &(currentFire->position.x));

        vec3 flame = ScalarMult(flameIllumination, 0.5 + 0.5*perlin2d(currentFire->position.x + time, currentFire->position.y + time,10, 1));

        sprintf(colorName, "lightColor%d", count);
        glUniform3fv(glGetUniformLocation(in->shader, colorName), 1, &(flame.x));
    }
    while(count < 3)
    {
        count = count + 1;

        char pointName[80];
        char colorName[80];
        sprintf(pointName, "lightPoint%d", count);
        glUniform3fv(glGetUniformLocation(in->shader, pointName), 1, &(ZERO_VECTOR.x));

        sprintf(colorName, "lightColor%d", count);
        glUniform3fv(glGetUniformLocation(in->shader, colorName), 1, &(ZERO_VECTOR.x));

    }
    //------------------------------------

    // <<Set options>>
    // Enable Z-buffering
    glEnable(GL_DEPTH_TEST);
    // Enable backface culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    //------------------------------------
    // <<Draw>>
    DrawModel(in->model, in->shader, "in_Position", "in_Normal", NULL);
    //------------------------------------
}

// Render all scene objects connected to a scene.
void renderScene(SceneObject *scene, Camera *activeCamera)
{
    SceneObject *current = scene;

    // Check if there is another SceneObject
    while(current->next != NULL)
    {
        // Advance in list
        current = current->next;

        // Render SceneObject
        renderSceneObject(current, activeCamera);
    }

}
////////////////////////////////////////////////////////////////////////////////////
/*                                  Input                                         */
////////////////////////////////////////////////////////////////////////////////////

/* Key States */

// Walk
int forward = 0;
int backward = 0;

// Rotate
int rotLeft = 0;
int rotRight = 0;

// Strafe
int strafeLeft = 0;
int strafeRight = 0;

// Remove?
int currentCamID = 0;

// Key is pressed
void KeyDown(unsigned char key,
             __attribute__((unused)) int x,
             __attribute__((unused)) int y)
{
    switch (key)
    {

        // Movement forward and backward
        case 'w':
            forward = 1;
            break;
        case 's':
            backward = 1;
            break;

        // Rotation
        case 'a':
            rotLeft = 1;
            break;
        case 'd':
            rotRight = 1;
            break;

        // Strafing
        case 'q':
            strafeLeft = 1;
            break;
        case 'e':
            strafeRight = 1;
            break;

        default:
            break;
    }
}
// Key is released
void KeyUp(unsigned char key,
             __attribute__((unused)) int x,
             __attribute__((unused)) int y)
{
    switch (key)
    {


        case 'w':
            forward = 0;
            break;
        case 's':
            backward = 0;
            break;

        case 'a':
            rotLeft = 0;
            break;
        case 'd':
            rotRight = 0;
            break;

        case 'q':
            strafeLeft = 0;
            break;
        case 'e':
            strafeRight = 0;
            break;
        default:
            break;
    }
}

// Checks the key states and transforms the given camera by wasd-movement.
void characterControl(Camera *this, float speed)
{

    strafeCamera(this, speed*(strafeRight-strafeLeft)*dTime);

    if(forward == 1)
    {
        moveCamForward(this, speed*dTime);
    }
    if(backward == 1)
    {
        moveCamForward(this, -1*speed/2*dTime);
    }
    if(rotLeft == 1)
    {
        rotateCamera(this, speed/2*dTime);
    }
    if(rotRight == 1)
    {
        rotateCamera(this, -1*speed/2*dTime);
    }
}

////////////////////////////////////////////////////////////////////////////////////
/*                              Initializations                                   */
////////////////////////////////////////////////////////////////////////////////////

void initTime()
{
    time = (GLfloat)glutGet(GLUT_ELAPSED_TIME);
    dTime = 0.0;
}
void initGL()
{
    // shader info
    dumpInfo();

    // Clear
    glClearColor(0.1, 0.1, 0.3, 0);
    glClearDepth(1.0);

    // Color
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    // Culling and depth
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    // Get alpha
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable( GL_BLEND );
    glBlendFunc(GL_ONE, GL_ZERO);

    printError("GL inits");
}
void initConstants()
{
    // Complex constants
    UP_VECTOR = SetVector(0, 1, 0);
    RIGHT_VECTOR = SetVector(1, 0, 0);
    FRONT_VECTOR = SetVector(0, 0, 1);
    ZERO_VECTOR = SetVector(0, 0, 0);

    ambientLightColor = SetVector(0, 0, 0.05);
    flameIllumination = SetVector(1, 0.60, 0);
}
void initShaders()
{
    // Load and compile shaders initShader
    log_shader    = loadShaders("log.vert", "log.frag");  // renders with light (used for initial renderin of teapot)
    ground_shader = loadShaders("ground.vert", "ground.frag");
    fire_shader   = loadShaders("mainFire.vert", "mainFire.frag");
    shadow_shader = loadShaders("shadow.vert", "shadow.frag");
    printError("init shader");

}
void initModels()
{
    // Load models
    cube = LoadModelPlus("objects/cubeplus.obj");
    /*
     squareModel = LoadDataToModel(
            square, NULL, squareTexCoord, NULL,
            squareIndices, 4, 6);
    */
}

SceneObject* mainFire;


void initSceneObjects()
{
    // The root of scene1
    scene1Root = newSceneObject(NULL, 0, ZERO_VECTOR, 1, NULL);
    fireRoot = newSceneObject(NULL, 0, ZERO_VECTOR, 1, NULL);
    // ---

    cubeFloorObject = newSceneObject(cube, shadow_shader, SetVector(0, 0, 0), 9, scene1Root);
    cubeFloorObject->scaleV.y = 0.1;
    cubeFloorObject = newSceneObject(cube, shadow_shader, SetVector(-25, 0, -10), 9, scene1Root);
    cubeFloorObject->scaleV.y = 0.1;


    cubeFloorObject = newSceneObject(cube, ground_shader, SetVector(0, -1, 0), 500, scene1Root);
    cubeFloorObject->scaleV.y = 1;

    cubeFloorObject = newSceneObject(cube, ground_shader, SetVector(9, 1.5, 0), 3, scene1Root);


    // The number of logs around the fire.
    int count_logs = 10;
    for (int i = 0; i < count_logs; ++i)
    {
        // The angle that the log is positioned at.
        float log_radians = 2*PI*i/count_logs;
        //printf("%f \n",log_radians);
        // Create a log sceneobject and add it to the scene1root. Position 1 unit away from fire at log_radians angle.
        SceneObject* fire_log = newSceneObject(cube, log_shader, SetVector(1*cos(log_radians), 1, 1*sin(log_radians)), 1.5, scene1Root);
        // Make the log a thin line
        fire_log->scaleV.y = 0.5;
        fire_log->scaleV.x = 0.5;
        // Point the log at the fire.
        fire_log->rotation.y = -log_radians-PI/2;
        // Make log semi-stand up.
        fire_log->rotation.x = PI/4*sin(log_radians);
        fire_log->rotation.z = PI/4*cos(log_radians);
    }


    // The flame in the bonfire.
    mainFire = newSceneObject(cube, fire_shader, SetVector(0, 2.5, 0), 10, fireRoot);
    mainFire->scaleV.x = 5;
    mainFire->scaleV.z = 1;

    mainFire = newSceneObject(cube, fire_shader, SetVector(-25, 2.5, -10), 10, fireRoot);
    mainFire->scaleV.x = 5;
    mainFire->scaleV.z = 1;

    playerObject = newSceneObject(cube, log_shader, SetVector(0, 5, 15), 1, scene1Root);
    playerObject->scaleV = SetVector(1, 4, 1);


}
void initCameras()
{
    // Initialize camera
    cam = SetVector(0, 5, 15);
    point = SetVector(0, 0, 0);
    playerCamera = newCamera(cam, point);

}

void init(void)
{
    initTime();
    initGL();
    initConstants();
    initShaders();
    initModels();
    initSceneObjects();
    initCameras();

	glutTimerFunc(5, &OnTimer, 0);
}

////////////////////////////////////////////////////////////////////////////////////
/*                              Help functions                                    */
////////////////////////////////////////////////////////////////////////////////////

void OnTimer(int value)
{
	glutPostRedisplay();
	glutTimerFunc(5, &OnTimer, value);
}

////////////////////////////////////////////////////////////////////////////////////
/*                          Callback functions                                    */
////////////////////////////////////////////////////////////////////////////////////

// This function is called whenever it is time to render
//  a new frame; due to the idle()-function below, this
//  function will get called several times per second
void display(void)
{
    //////////////////////////////////////
    /*              TIME                */
    //////////////////////////////////////
    // Update time (seconds)
    dTime = (GLfloat) glutGet(GLUT_ELAPSED_TIME) / 1000 - time;
    time = time + dTime;
    //printf("Time:%f\n", time );

    // Colour of the sky.
    //ambientLightColor = ScalarMult( SetVector(0.2, 0.2, 0.5), perlin2d(time, time, 1, 1));
    //printf("%f\n", perlin2d(time, time, 1, 1));
    //////////////////////////////////////
    /*          Camera control          */
    //////////////////////////////////////

    // Character movement.
    characterControl(playerCamera, 6);

    // Update players targetOffset.
    playerCamera->targetOffset = VectorSub(playerCamera->target, playerCamera->position);


    // Move the players body. TODO: Maybe should be other way around but works for now.
    playerObject->position = playerCamera->position;

    // Walk on the ground please.
    playerObject->position.y = playerObject->scaleV.y / 2;

    //////////////////////////////////////
    /*          Object control          */
    //////////////////////////////////////



    // Rotate fires
    SceneObject* current = fireRoot;
    while(current->next != NULL)
    {
        current = current->next;

        // rotate fire
        vec3 direction = VectorSub(current->position, playerCamera->position);
        direction.y = 0;
        direction = Normalize(direction);

        // if we would divide by zero, skip that rotation update.
        if(direction.z != 0)
        {
            current->rotation.y = atan(direction.x / direction.z);
        }
    }

    //////////////////////////////////////
    //          Buffer cleaning         //
    //////////////////////////////////////
    // Sky box
    glClearColor(ambientLightColor.x, ambientLightColor.y, ambientLightColor.z, 0);

    // Clear color- and depth-buffer.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // No additive blending for objects// Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //glBlendFunc(GL_ONE, GL_ZERO);
    // Render the scene
    renderScene(scene1Root, playerCamera);

    // Additive blending for fire
    glBlendFunc(GL_ONE, GL_ONE);
    renderScene(fireRoot, playerCamera);
    //////////////////////////////////////
    //        End of render-loop        //
    //////////////////////////////////////

    glutSwapBuffers();
}

void reshape(GLsizei w, GLsizei h)
{
	glViewport(0, 0, w, h);
	GLfloat ratio = (GLfloat) w / (GLfloat) h;
	projectionMatrix = perspective(90, ratio, 1.0, 1000);
}
// This function is called whenever the computer is idle
// As soon as the machine is idle, ask GLUT to trigger rendering of a new
// frame
void idle()
{
  glutPostRedisplay();
}

//-----------------------------main-----------------------------------------------
int main(int argc, char *argv[])
{
	glutInit(&argc, argv);

	glutInitDisplayMode(GLUT_RGBA | GLUT_DEPTH | GLUT_DOUBLE | GLUT_STENCIL);
	glutInitWindowSize(W, H);

	glutInitContextVersion(3, 2);
	glutCreateWindow ("Procedural Campfire");
	glutDisplayFunc(display);
	glutReshapeFunc(reshape);

    glutKeyboardFunc(KeyDown);
    glutKeyboardUpFunc(KeyUp);

	glutIdleFunc(idle);

	init();
	glutMainLoop();
	exit(0);
}