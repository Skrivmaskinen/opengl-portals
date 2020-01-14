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

Model* squareModel;
Point3D cam, point, teleCam;
Model *model1, *cube, *teapot;
FBOstruct *fbo1, *fbo2, *fbo_unfiltered, *fbo_finished, *fbo_screen1;
GLuint plain_white_shader =0, phongshader = 0, plaintextureshader = 0, blur_shader = 0, truncate_shader = 0, merger_shader = 0, window_shader = 0, screen_shader = 0, fire_shader = 0;
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

////////////////////////////////////////////////////////////////////////////////////
/*                                Header                                          */
////////////////////////////////////////////////////////////////////////////////////
// Constructors
struct Camera*      newCamera(vec3 inPosition, vec3 inTarget);
struct SceneObject* newSceneObject(Model* inModel, GLuint inShader, vec3 inPosition, float inScale, SceneObject *root);

void enlistSceneObject(SceneObject *root, SceneObject *newSceneObject);

mat4 Rxyz(vec3 rotationVector);
vec3 getRotationOfNormal(vec3 theVector, vec3 x_axis, vec3 y_axis);
void getScreenAxis(SceneObject *this, vec3 *out_X, vec3 *out_Y);

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

void setRotation(SceneObject* in, float inX, float inY, float inZ)
{
    // in: the SceneObject to rotate
    // inX, inY, inZ: new rotation
    /////////////////////////////////////////

    // Set the rotation
    in->rotation = SetVector(inX, inY, inZ);
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
    glUniform3fv(glGetUniformLocation(in->shader, "lightPoint"), 1, &(UP_VECTOR.x));
    glUniform3fv(glGetUniformLocation(in->shader, "objectCenter"), 1, &(in->position.x));
    glUniform3fv(glGetUniformLocation(in->shader, "camPos"), 1, &(activeCam->position.x) );
    glUniform1i(glGetUniformLocation(in->shader, "texUnit"), 0);
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

void getScreenAxis(SceneObject *this, vec3 *out_X, vec3 *out_Y)
{
    vec3 X = MultVec3(Ry(this->rotation.y), RIGHT_VECTOR);
    vec3 Y = MultVec3(Rx(this->rotation.x), UP_VECTOR);
    X = MultVec3(Rz(this->rotation.z), X);
    Y = MultVec3(Rz(this->rotation.z), Y);
    X = ScalarMult(X, this->scaleV.x);
    Y = ScalarMult(Y, this->scaleV.y);

    *out_X = X;
    *out_Y = Y;
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

int show_blipp = 0;
// Key is pressed
void KeyDown(unsigned char key,
             __attribute__((unused)) int x,
             __attribute__((unused)) int y)
{
    switch (key)
    {
        // Arbitrary toggle, use when needed
        case 'v':
            show_blipp = 1;
            break;
        case 'c':
            show_blipp = 0;
            break;

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
}
void initShaders()
{
    // Load and compile shaders initShader
    plaintextureshader  = loadShaders("plaintextureshader.vert", "plaintextureshader.frag");  // puts texture on teapot
    phongshader         = loadShaders("phong.vert", "phong.frag");  // renders with light (used for initial renderin of teapot)
    blur_shader         = loadShaders("blur.vert", "blur.frag"); // Low pass filter
    truncate_shader     = loadShaders("truncate.vert", "truncate.frag"); // Get the overexponated part of the image
    merger_shader       = loadShaders("merge.vert", "merge.frag"); //Combine two FBOs to a singe output
    window_shader       = loadShaders("window.vert", "window.frag");
    screen_shader       = loadShaders("screen.vert", "screen.frag");
    plain_white_shader  = loadShaders("plain_white.vert", "plain_white.frag");
    fire_shader         = loadShaders("mainFire.vert", "mainFire.frag");
    printError("init shader");

}
void initFBOs()
{
    // Initialize FBOs
    fbo1 = initFBO(W, H, 0);
    fbo2 = initFBO(W, H, 0);
    fbo_unfiltered = initFBO(W, H, 0);
    fbo_finished = initFBO(W, H, 0);
    fbo_screen1 = initFBO(W, H, 0);
}
void initModels()
{
    // Load models
    //model1 = LoadModelPlus("teapot.obj");
    model1 = LoadModelPlus("stanford-bunny.obj");
    cube = LoadModelPlus("objects/cubeplus.obj");
    teapot = LoadModelPlus("objects/teapot.obj");

    squareModel = LoadDataToModel(
            square, NULL, squareTexCoord, NULL,
            squareIndices, 4, 6);
}

SceneObject* mainFire;
SceneObject* fireRoot;
void initSceneObjects()
{
    // The root of scene1
    scene1Root = newSceneObject(NULL, 0, ZERO_VECTOR, 1, NULL);
    fireRoot = newSceneObject(NULL, 0, ZERO_VECTOR, 1, NULL);
    // ---
    cubeFloorObject = newSceneObject(cube, window_shader, SetVector(0, -1, 0), 100, scene1Root);
    cubeFloorObject->scaleV.y = 1;

    // The number of logs around the fire.
    int count_logs = 10;
    for (int i = 0; i < count_logs; ++i)
    {
        // The angle that the log is positioned at.
        float log_radians = 2*PI*i/count_logs;
        //printf("%f \n",log_radians);
        // Create a log sceneobject and add it to the scene1root. Position 1 unit away from fire at log_radians angle.
        SceneObject* fire_log = newSceneObject(cube, phongshader, SetVector(1*cos(log_radians), 1, 1*sin(log_radians)), 1.5, scene1Root);
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

    mainFire = newSceneObject(cube, fire_shader, SetVector(5, 2.5, 0), 10, fireRoot);
    mainFire->scaleV.x = 5;
    mainFire->scaleV.z = 1;

    playerObject = newSceneObject(cube, phongshader, SetVector(0, 5, 15), 1, scene1Root);
    playerObject->scaleV = SetVector(1, 4, 1);

}
void initCameras()
{
    // Initialize cameras
    cam = SetVector(0, 5, 15);
    teleCam = SetVector(0, 5, -15);
    point = SetVector(0, 5, 0);

    playerCamera = newCamera(cam, point);
    surveillanceCamera = newCamera(teleCam, point);
}

void init(void)
{
    initTime();
    initGL();
    initConstants();
    initShaders();
    initFBOs();
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

vec3 getRotationOfNormal(vec3 theVector, vec3 x_axis, vec3 y_axis)
{
    // Normalize the axix
    theVector = Normalize(theVector);
    x_axis = Normalize(x_axis);
    y_axis = Normalize(y_axis);

    // Get the radian angle of the tilt
    vec3 outRadians;
    outRadians.x = asin(DotProduct(theVector, y_axis));
    outRadians.y = asin(DotProduct(theVector, x_axis));
    outRadians.z = 0;

    return outRadians;
}

// Rotation in xyz directions
mat4 Rxyz(vec3 rotationVector)
{
    mat4 rotX = Rx(rotationVector.x);
    mat4 rotY = Ry(rotationVector.y);
    mat4 rotZ = Rz(rotationVector.z);

    return Mult(rotX, Mult(rotY, rotZ));
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

    //////////////////////////////////////
    /*          Camera control          */
    //////////////////////////////////////

    // Character movement.
    characterControl(playerCamera, 6);

    // Update players targetOffset.
    playerCamera->targetOffset = VectorSub(playerCamera->target, playerCamera->position);
    surveillanceCamera->targetOffset = VectorSub(surveillanceCamera->target, surveillanceCamera->position);

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
    glClearColor(0.0, 0.0, 0.0, 0);

    // Clear color- and depth-buffer.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // No transparency for objects
    glBlendFunc(GL_ONE, GL_ZERO);
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
	glutCreateWindow ("Alexs' awesome portals");
	glutDisplayFunc(display);
	glutReshapeFunc(reshape);

    glutKeyboardFunc(KeyDown);
    glutKeyboardUpFunc(KeyUp);

	glutIdleFunc(idle);

	init();
	glutMainLoop();
	exit(0);
}


////////////////////////////////////////////////////////////////////////////////////
/*                                  The Dump                                      */
////////////////////////////////////////////////////////////////////////////////////

/* // Replace fragment with '1' when drawn to.
glStencilFunc(GL_ALWAYS, 1, 0xFFFFFFFF); // GL_ Initially the stencil test is disabled. If there is no stencil buffer, no stencil modification can occur and it is as if the stencil test always passes. ALWAYS (ingmar)
glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE); // GL_REPLACE (ingmar)

// Render opening object to stencil.
renderSceneObject(openingObject, playerCamera);

//////////////////////////////////////
//       Render portal-view         //
//////////////////////////////////////
// Turn on the color and depth buffer to allow normal rendering.
glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
glDepthMask(GL_TRUE);

// Don't throw away stencil. We will probably render several objects in with the same stencil.
glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
// Render where stencil == 1
glStencilFunc(GL_EQUAL, 1, 0xFFFFFFFF);

// Render scene 1 with surveillanceCamera
renderScene(scene1Root, surveillanceCamera);

//////////////////////////////////////
//      Block portal from player    // Idea from: https://en.wikibooks.org/wiki/OpenGL_Programming/Mini-Portal#Clipping_the_portal_scene_-_stencil_buffer
//////////////////////////////////////
// Disable colour
glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
// Enable depth
glDepthMask(GL_TRUE);
// Disable stencil-test
glDisable(GL_STENCIL_TEST);

// openingObject is rendered to only the depth buffer, blocking new renders behind the portal
renderSceneObject(openingObject, playerCamera);

//////////////////////////////////////
//        Draw player view          //
//////////////////////////////////////
// Enable rendering to color-buffer
glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

// Render the scene
renderScene(scene1Root, playerCamera);*/

/*

// Convert playerCameras position into local coordinates
vec3 localPlayerCoords = global2local(screenCoords, openingObject->position, playerCamera->position);
surveillanceCamera->positionOffset = VectorSub(local2global(recorderCoords, surveillanceCamera->position, localPlayerCoords), surveillanceCamera->position);

vec3 localPlayerTargetCoords = global2local(screenCoords, openingObject->position, playerCamera->target);
surveillanceCamera->targetOffset = VectorSub(local2global(recorderCoords, surveillanceCamera->position, localPlayerTargetCoords), surveillanceCamera->position);

*/
/**/
//vec3 newPos = VectorAdd(SetVector(3*sin(time/2), 0, 0),  cam);
//rotateCamera(surveillanceCamera, dTime);
//updateFOV(surveillanceCamera, playerCamera, screenObject);

/* -----BUNNY-----
// Activate shader program
glUseProgram(phongshader);

vm2 = viewMatrix;
// Scale and place bunny since it is too small
vm2 = Mult(vm2, T(0, -8.5, 0));
vm2 = Mult(vm2, S(80,80,80));

glUniformMatrix4fv(glGetUniformLocation(phongshader, "projectionMatrix"), 1, GL_TRUE, projectionMatrix.m);
glUniformMatrix4fv(glGetUniformLocation(phongshader, "modelviewMatrix"), 1, GL_TRUE, vm2.m);
glUniform3fv(glGetUniformLocation(phongshader, "camPos"), 1, &cam.x);
glUniform1i(glGetUniformLocation(phongshader, "texUnit"), 0);

// Enable Z-buffering
glEnable(GL_DEPTH_TEST);
// Enable backface culling
glEnable(GL_CULL_FACE);
glCullFace(GL_BACK);

DrawModel(model1, phongshader, "in_Position", "in_Normal", NULL);

*/
/*
//setCameraPosition(playerCamera, &newPos);
// Render scene ----------------------------------------------------------
Camera * currentCam;
if(currentCamID == 1)
{
    currentCam = surveillanceCamera;
}
else
{
    currentCam = playerCamera;
}

renderSceneObject(bunnyObject, currentCam);
renderSceneObject(cubeFloorObject, currentCam);
*/

//glActiveTexture(GL_TEXTURE0);
//glBindTexture(GL_TEXTURE_2D, fbo_screen1);
//renderSceneObject(screenObject, playerCamera);


// render to fbo_unfiltered with fbo_screen1 as texture.
//useFBO(fbo_unfiltered, fbo_screen1, 0L);

// Done rendering the FBO! Set up for rendering on screen, using the result as texture!
//////////////////////////////////////
/*          Blooming                */
//////////////////////////////////////
/*
// Truncate fbo_unfiltered and put inside fbo1
runFilter(truncate_shader, fbo_unfiltered, 0L, fbo1);

// MEGABLUR fbo1 ping-ponging between fbo1 and fbo2
for (int i = 0; i < 10; ++i)
{
    runFilter(blur_shader, fbo1, 0L, fbo2);
    runFilter(blur_shader, fbo2, 0L, fbo1);
}

runFilter(merger_shader, fbo1, fbo_unfiltered, fbo_finished);
*/
//////////////////////////////////////
/*          Render to screen        */
//////////////////////////////////////
/*
//renderSceneObject(screenObject, playerCamera);
// Final output that is shown on screen
renderSceneObject(visionObject, playerCamera);
*/
/*
//	glFlush(); // Can cause flickering on some systems. Can also be necessary to make drawing complete.
useFBO(0L, fbo_finished, 0L);
glClearColor(0.0, 0.0, 0.0, 0);
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

// Activate second shader program
glUseProgram(plaintextureshader);

glDisable(GL_CULL_FACE);
glDisable(GL_DEPTH_TEST);
DrawModel(squareModel, plaintextureshader, "in_Position", NULL, "in_TexCoord");
*/

// Render scene
//renderSceneObject(bunnyObject, surveillanceCamera);
//renderSceneObject(cubeFloorObject, surveillanceCamera);

/*
// render from surveillanceCamera to fbo_screen1
useFBO(fbo_screen1, 0L, 0L);

// Clear fbo
glClearColor(0.4, 0.1, 0.3, 0);
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

*/
/*
 void GetCameraAxis(Camera *this, vec3 *out_X, vec3 *out_Y )
{
vec3 z_axis = Normalize(VectorSub(this->target, this->position));
float radians_x;
*out_Y = MultVec3(Rx(1), z_axis);
}
*/


/*
    // Reset stencil
    glClear(GL_STENCIL_BUFFER_BIT);

    // Enable stencil
    glEnable(GL_STENCIL_TEST);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);

    // Replace fragment with '1' when drawn to.
    glStencilFunc(GL_ALWAYS, 1, 0xFFFFFFFF); // GL_ Initially the stencil test is disabled. If there is no stencil buffer, no stencil modification can occur and it is as if the stencil test always passes. ALWAYS (ingmar)
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE); // GL_REPLACE (ingmar)

    renderSceneObject(openingObject, playerCamera);

    // Replace fragment with '0' when drawn to.
    glStencilFunc(GL_ALWAYS, 1, 0xFFFFFFFF); // GL_ALWAYS (ingmar)
    glStencilOp(GL_ZERO, GL_REPLACE, GL_ZERO);

    renderSceneObject(bunnyObject, playerCamera);
    //renderSceneObject(cubeFloorObject, playerCamera);

    // Turn on the color and depth buffer to allow normal rendering.
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);

    // Don't throw away stencil. We will probably render several objects in with the same stencil.
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    //////////////////////////////////////
    //          Player camera           //
    //////////////////////////////////////
    // Only write to framebuffer if stencil buffer equals 0.
    glStencilFunc(GL_EQUAL, 0, 0xFFFFFFFF);

    // Render scene 1 with playerCamera
    renderScene(scene1Root, playerCamera);

    //////////////////////////////////////
    //         Surveillance camera      //
    //////////////////////////////////////

    // Only write to framebuffer if stencil buffer equals 1.
    glStencilFunc(GL_EQUAL, 1, 0xFFFFFFFF);

    // Render scene 1 with surveillanceCamera
    renderScene(scene1Root, surveillanceCamera);

    glDisable(GL_STENCIL_TEST);
    */