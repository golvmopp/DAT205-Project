

#include <GL/glew.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <iostream>

#include <labhelper.h>
#include <imgui.h>
#include <imgui_impl_sdl_gl3.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
using namespace glm;
#include <Model.h>
#include <AABB.h>
#include "hdr.h"
#include "fbo.h"



using std::min;
using std::max;


///////////////////////////////////////////////////////////////////////////////
// Various globals
///////////////////////////////////////////////////////////////////////////////
SDL_Window* g_window = nullptr;
float currentTime  = 0.0f;
float previousTime = 0.0f;
float deltaTime    = 0.0f;
bool showUI = false;
int windowWidth, windowHeight;
float speed = 0.0f;

// Framebuffers for post processing
std::vector<FboInfo> fboList;
// Samples for ambient occlusion
glm::vec3 hemisphereSamples[16];


///////////////////////////////////////////////////////////////////////////////
// Shader programs
///////////////////////////////////////////////////////////////////////////////
GLuint shaderProgram; // Shader for rendering the final image
GLuint simpleShaderProgram; // Shader used to draw the shadow map
GLuint backgroundProgram;
GLuint postFXshader; //Shader for post processing effects
GLuint cutoffShader, hBlurShader, vBlurShader, motionBlurShader;

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
float environment_multiplier = 1.5f;
GLuint environmentMap, irradianceMap, reflectionMap;
const std::string envmap_base_name = "001";

///////////////////////////////////////////////////////////////////////////////
// Light source
///////////////////////////////////////////////////////////////////////////////
vec3 lightPosition;
vec3 point_light_color = vec3(1.f, 1.f, 1.f);

float point_light_intensity_multiplier = 10000.0f;


///////////////////////////////////////////////////////////////////////////////
// Camera parameters.
///////////////////////////////////////////////////////////////////////////////
vec3 cameraPosition(-70.0f, 50.0f, 70.0f);
vec3 cameraDirection = normalize(vec3(0.0f) - cameraPosition);
float cameraSpeed = 1.0f;

vec3 worldUp(0.0f, 1.0f, 0.0f);

///////////////////////////////////////////////////////////////////////////////
// Models
///////////////////////////////////////////////////////////////////////////////
labhelper::Model *fighterModel = nullptr;
labhelper::Model *landingpadModel = nullptr;
labhelper::Model *sphereModel = nullptr;


mat4 roomModelMatrix;
mat4 landingPadModelMatrix; 
mat4 fighterModelMatrix;

mat4 previousViewProjectionMatrix = mat4(1.0f);

mat4 shipTranslation = translate(vec3(0.f, 10.f, 0.f));
mat4 shipRotation = mat4(1.0f);

// AABB ship start, change the center of the AABB according to the position of the ship
AABB shipBV = AABB(vec3(shipTranslation[3]), vec3(12.f, 10.f, 12.f));
AABB goalBV = AABB(vec3(0.f), vec3(10.f, 10.f, 10.f));

void loadShaders(bool is_reload)
{
	GLuint shader = labhelper::loadShaderProgram("simple.vert", "simple.frag", is_reload);
	if (shader != 0) simpleShaderProgram = shader; 
	shader = labhelper::loadShaderProgram("background.vert", "background.frag", is_reload);
	if (shader != 0) backgroundProgram = shader;
	shader = labhelper::loadShaderProgram("shading.vert", "shading.frag", is_reload);
	if (shader != 0) shaderProgram = shader;
	}

void initGL()
{
	///////////////////////////////////////////////////////////////////////
	//		Load Shaders
	///////////////////////////////////////////////////////////////////////
	backgroundProgram = labhelper::loadShaderProgram("background.vert", "background.frag");
	shaderProgram = labhelper::loadShaderProgram("shading.vert", "shading.frag");
	simpleShaderProgram = labhelper::loadShaderProgram("simple.vert", "simple.frag");
	postFXshader = labhelper::loadShaderProgram("postFX.vert", "postFX.frag");
	
	cutoffShader = labhelper::loadShaderProgram("postFX.vert", "cutoff.frag");
	hBlurShader = labhelper::loadShaderProgram("postFX.vert", "horizontal_blur.frag");
	vBlurShader = labhelper::loadShaderProgram("postFX.vert", "vertical_blur.frag");
	motionBlurShader = labhelper::loadShaderProgram("postFX.vert", "motion_blur.frag");



	///////////////////////////////////////////////////////////////////////
	// Load models and set up model matrices
	///////////////////////////////////////////////////////////////////////
	fighterModel = labhelper::loadModelFromOBJ("../scenes/NewShip.obj");
	landingpadModel = labhelper::loadModelFromOBJ(	"../scenes/landingpad.obj");
	sphereModel = labhelper::loadModelFromOBJ("../scenes/sphere.obj");

	roomModelMatrix = mat4(1.0f);
	fighterModelMatrix = translate(15.0f * worldUp);
	landingPadModelMatrix = mat4(1.0f);

	///////////////////////////////////////////////////////////////////////
	// Load environment map
	///////////////////////////////////////////////////////////////////////
	const int roughnesses = 8;
	std::vector<std::string> filenames;
	for (int i = 0; i < roughnesses; i++)
		filenames.push_back("../scenes/envmaps/" + envmap_base_name + "_dl_" + std::to_string(i) + ".hdr");

	reflectionMap = labhelper::loadHdrMipmapTexture(filenames);
	environmentMap = labhelper::loadHdrTexture("../scenes/envmaps/" + envmap_base_name + ".hdr");
	irradianceMap = labhelper::loadHdrTexture("../scenes/envmaps/" + envmap_base_name + "_irradiance.hdr");

	///////////////////////////////////////////////////////////////////////
	// Setup Framebuffer
	///////////////////////////////////////////////////////////////////////
	int w, h;
	SDL_GetWindowSize(g_window, &w, &h);
	for (int i = 0; i < 5; i++) {
		fboList.push_back(FboInfo());
		fboList[i].resize(w,h);
	}

	///////////////////////////////////////////////////////////////////////
	// Generate samples for ambient occlusion
	///////////////////////////////////////////////////////////////////////
	int nrSamples = 16;
	for (int i = 0; i < nrSamples; i++) {
		vec3 tmp = labhelper::cosineSampleHemisphere();
		tmp = tmp * labhelper::randf();
		hemisphereSamples[i] = tmp;
	}

	

	glEnable(GL_DEPTH_TEST);	// enable Z-buffering 
	glEnable(GL_CULL_FACE);		// enables backface culling


}

void debugDrawLight(const glm::mat4 &viewMatrix, const glm::mat4 &projectionMatrix, const glm::vec3 &worldSpaceLightPos)
{
	mat4 modelMatrix = glm::translate(worldSpaceLightPos);
	glUseProgram(shaderProgram);
	labhelper::setUniformSlow(shaderProgram, "modelViewProjectionMatrix", projectionMatrix * viewMatrix * modelMatrix);
	labhelper::render(sphereModel);
	labhelper::setUniformSlow(shaderProgram, "modelViewProjectionMatrix", projectionMatrix * viewMatrix);
	labhelper::debugDrawLine(viewMatrix, projectionMatrix, worldSpaceLightPos);
}


void drawBackground(const mat4 &viewMatrix, const mat4 &projectionMatrix)
{
	glUseProgram(backgroundProgram);
	labhelper::setUniformSlow(backgroundProgram, "environment_multiplier", environment_multiplier);
	labhelper::setUniformSlow(backgroundProgram, "inv_PV", inverse(projectionMatrix * viewMatrix));
	labhelper::setUniformSlow(backgroundProgram, "camera_pos", cameraPosition);
	labhelper::drawFullScreenQuad();
}

void drawScene(GLuint currentShaderProgram, const mat4 &viewMatrix, const mat4 &projectionMatrix, const mat4 &lightViewMatrix, const mat4 &lightProjectionMatrix)
{
	glUseProgram(currentShaderProgram);
	// Light source
	vec4 viewSpaceLightPosition = viewMatrix * vec4(lightPosition, 1.0f);
	labhelper::setUniformSlow(currentShaderProgram, "point_light_color", point_light_color);
	labhelper::setUniformSlow(currentShaderProgram, "point_light_intensity_multiplier", point_light_intensity_multiplier);
	labhelper::setUniformSlow(currentShaderProgram, "viewSpaceLightPosition", vec3(viewSpaceLightPosition));
	labhelper::setUniformSlow(currentShaderProgram, "viewSpaceLightDir", normalize(vec3(viewMatrix * vec4(-lightPosition, 0.0f))));

	// Hemisphere samples
	labhelper::setUniformSlow(currentShaderProgram, "hemisphere_samples", 16, hemisphereSamples[0]);


	
	// Environment
	labhelper::setUniformSlow(currentShaderProgram, "environment_multiplier", environment_multiplier);

	// camera
	labhelper::setUniformSlow(currentShaderProgram, "viewInverse", inverse(viewMatrix));

	// landing pad 
	labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix", projectionMatrix * viewMatrix * landingPadModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", viewMatrix * landingPadModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "normalMatrix", inverse(transpose(viewMatrix * landingPadModelMatrix)));

	labhelper::render(landingpadModel);

	// Fighter
	labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix", projectionMatrix * viewMatrix * fighterModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", viewMatrix * fighterModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "normalMatrix", inverse(transpose(viewMatrix * fighterModelMatrix)));

	labhelper::render(fighterModel);


}

/*
	Returns which FBO in list contains the result, i.e. bloomed lights
	write is the first FBO written to in this function. It will
	use results from FBO #write-1 to begin with.
*/
int bloom(int write) 
{
	glActiveTexture(GL_TEXTURE0); // Just to make sure

	// Cut off light sources to blur
	glBindFramebuffer(GL_FRAMEBUFFER, fboList[write].framebufferId);
	glUseProgram(cutoffShader);
	glBindTexture(GL_TEXTURE_2D, fboList[write - 1].colorTextureTargets[0]);
	labhelper::drawFullScreenQuad();

	write++;

	// blur light sources horizontally
	glBindFramebuffer(GL_FRAMEBUFFER, fboList[write].framebufferId);
	glUseProgram(hBlurShader);
	glBindTexture(GL_TEXTURE_2D, fboList[write - 1].colorTextureTargets[0]);
	labhelper::drawFullScreenQuad();

	write++;

	// bloom light sources vertically
	glBindFramebuffer(GL_FRAMEBUFFER, fboList[write].framebufferId);
	glUseProgram(vBlurShader);
	glBindTexture(GL_TEXTURE_2D, fboList[write - 1].colorTextureTargets[0]);
	labhelper::drawFullScreenQuad();

	// Return "pointer" to blurred light sources to be used as bloom effect
	return write;
}


void display(void)
{
	///////////////////////////////////////////////////////////////////////////
	// Check if window size has changed and resize buffers as needed
	///////////////////////////////////////////////////////////////////////////
	{
		int w, h;
		SDL_GetWindowSize(g_window, &w, &h);
		if (w != windowWidth || h != windowHeight) {
			windowWidth = w;
			windowHeight = h;
			//Maybe need to update FBO sizes, or is this handled already?
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// setup matrices
	///////////////////////////////////////////////////////////////////////////

	vec4 ref = vec4(60.f, 25.f, 0.f, 1.f);
	vec4 tref = shipRotation * ref;
	cameraPosition = vec3(tref + shipTranslation[3]);

	mat4 projMatrix = perspective(radians(45.0f), float(windowWidth) / float(windowHeight), 5.0f, 5000.0f);

	mat4 viewMatrix = lookAt(cameraPosition, vec3(shipTranslation[3])*vec3(1.0f, 1.5f, 1.f), worldUp);


	vec4 lightStartPosition = vec4(40.0f, 40.0f, 0.0f, 1.0f);
	lightPosition = vec3(rotate(currentTime, worldUp) * lightStartPosition);
	mat4 lightViewMatrix = lookAt(lightPosition, vec3(0.0f), worldUp);
	mat4 lightProjMatrix = perspective(radians(45.0f), 1.0f, 25.0f, 100.0f);

	///////////////////////////////////////////////////////////////////////////
	// Movement updates
	///////////////////////////////////////////////////////////////////////////
	shipTranslation[3] -= speed * shipRotation[0]; //speed update
	//shipTranslation[2] -= speed * shipRotation[1]; //gravity
	shipBV.move(shipTranslation[3]);

	///////////////////////////////////////////////////////////////////////////
	// Bind the environment map(s) to unused texture units
	///////////////////////////////////////////////////////////////////////////
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, environmentMap);
	glActiveTexture(GL_TEXTURE7);
	glBindTexture(GL_TEXTURE_2D, irradianceMap);
	glActiveTexture(GL_TEXTURE8);
	glBindTexture(GL_TEXTURE_2D, reflectionMap);
	glActiveTexture(GL_TEXTURE0);

	///////////////////////////////////////////////////////////////////////////
	// Draw from camera   @ This draws the base scene to the first FBO
	///////////////////////////////////////////////////////////////////////////
	glBindFramebuffer(GL_FRAMEBUFFER, fboList[0].framebufferId);
	if (fboList[0].width != windowWidth || fboList[0].height != windowHeight) {
		fboList[0].resize(windowWidth, windowHeight);
	}
	glViewport(0, 0, windowWidth, windowHeight);
	glClearColor(0.2, 0.2, 0.8, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


	drawBackground(viewMatrix, projMatrix);
	drawScene(shaderProgram, viewMatrix, projMatrix, lightViewMatrix, lightProjMatrix);
	debugDrawLight(viewMatrix, projMatrix, vec3(lightPosition));


	///////////////////////////////////////////////////////////////////////////
	// Post Processing Passes
	///////////////////////////////////////////////////////////////////////////

	//bright area cutoffs to fbo1
	glBindFramebuffer(GL_FRAMEBUFFER, fboList[1].framebufferId);
	glUseProgram(cutoffShader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, fboList[0].colorTextureTargets[0]);

	labhelper::drawFullScreenQuad();

	//horizontally blurred cutoffs to fbo2
	glBindFramebuffer(GL_FRAMEBUFFER, fboList[2].framebufferId);
	glUseProgram(hBlurShader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, fboList[1].colorTextureTargets[0]);

	labhelper::drawFullScreenQuad();

	//vertically blurred horizontal blur to 0
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	glViewport(0, 0, windowWidth, windowHeight);
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(vBlurShader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, fboList[2].colorTextureTargets[0]);

	labhelper::drawFullScreenQuad();

	/*
	//blend together blurred cutoff with regular shader
	glBindFramebuffer(GL_FRAMEBUFFER, fboList[0].framebufferId);
	glUseProgram(postFXshader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, fboList[3].colorTextureTargets[0]);

	labhelper::drawFullScreenQuad();
	*/

	
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, fboList[0].depthBuffer);

	glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);

	glUseProgram(motionBlurShader);
	//Setting some uniforms for motion blur. MUST BE DONE AFTER glUseProgram!
	labhelper::setUniformSlow(motionBlurShader, "previousViewProjectionMatrix", previousViewProjectionMatrix);
	labhelper::setUniformSlow(motionBlurShader, "viewProjectionInverse", inverse(projMatrix * viewMatrix));
	glActiveTexture(GL_TEXTURE0);

	glBindTexture(GL_TEXTURE_2D, fboList[0].colorTextureTargets[0]);

	labhelper::drawFullScreenQuad();

	glDisable(GL_BLEND);





	//variable to be used for motion blur
	previousViewProjectionMatrix = (projMatrix * viewMatrix);
}

bool handleEvents(void)
{
	// check events (keyboard among other)
	SDL_Event event;
	bool quitEvent = false;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT || (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE)) {
			quitEvent = true;
		}
		if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_g) {
			showUI = !showUI;
		}
		if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_r) {
			loadShaders(true);
		}
		if (event.type == SDL_MOUSEMOTION && !ImGui::IsMouseHoveringAnyWindow()) { //Doesn't work since camera now locked behind vehicle
			static int prev_xcoord = event.motion.x;
			static int prev_ycoord = event.motion.y;
			int delta_x = event.motion.x - prev_xcoord;
			int delta_y = event.motion.y - prev_ycoord;

			if (event.button.button & SDL_BUTTON(SDL_BUTTON_LEFT)) {
				float rotationSpeed = 0.005f;
				mat4 yaw = rotate(rotationSpeed * -delta_x, worldUp);
				mat4 pitch = rotate(rotationSpeed * -delta_y, normalize(cross(cameraDirection, worldUp)));
				cameraDirection = vec3(pitch * yaw * vec4(cameraDirection, 0.0f));
			}
			prev_xcoord = event.motion.x;
			prev_ycoord = event.motion.y;
		}
	}

	// check keyboard state (which keys are still pressed)
	const uint8_t *state = SDL_GetKeyboardState(nullptr);
	vec3 cameraRight = cross(cameraDirection, worldUp);

	///////////////////// NEW STEERING
	float rotspeed	= .05f;
	float gospeed	= 2.0f;
	float godspeed = 4.0f;
	
	if (speed != 0) {
		if (state[SDL_SCANCODE_RIGHT]) {
			if (speed > 5) {
				shipRotation[0] += rotspeed * shipRotation[2];
			}
			else {
				shipRotation[0] += (rotspeed*0.2f*speed) * shipRotation[2];
			}
		}
		if (state[SDL_SCANCODE_LEFT]) {
			if (speed > 5) {
				shipRotation[0] -= rotspeed * shipRotation[2];
			}
			else {
				shipRotation[0] -= (rotspeed*0.2f*speed) * shipRotation[2];

			}
		}
	}

	shipRotation[0] = normalize(shipRotation[0]);
	shipRotation[2] = vec4(cross(vec3(shipRotation[0]), vec3(shipRotation[1])), 0.0f);

	if (state[SDL_SCANCODE_UP]) {
		if (speed <= 10.f) {
			speed += 0.05f*gospeed;
		}
	} 
	else if (state[SDL_SCANCODE_B]) {
		if (speed <= 13.f) {
			speed += 0.05f*godspeed;
		}

	}
	else if (state[SDL_SCANCODE_DOWN]) {
		if (speed >= -5.f) {
			speed -= 0.05f*gospeed;
		}
	}
	else {
		if (speed > 0) {
			speed -= 0.02f*gospeed;
			if (speed < 0) {
				speed = 0;
			}
			}
		else if (speed < 0) {
			speed += 0.02f*gospeed;
			if (speed > 0) {
				speed = 0;
			}
		}
		
	}

	if (state[SDL_SCANCODE_LCTRL]) {

	}

	if (state[SDL_SCANCODE_LSHIFT]) {
		speed = 0;
	}

	fighterModelMatrix = shipTranslation * shipRotation;
	////////////////////// END  NEW STEERING

	if (state[SDL_SCANCODE_W]) {
		cameraPosition += cameraSpeed* cameraDirection;
	}
	if (state[SDL_SCANCODE_S]) {
		cameraPosition -= cameraSpeed * cameraDirection;
	}
	if (state[SDL_SCANCODE_A]) {
		cameraPosition -= cameraSpeed * cameraRight;
	}
	if (state[SDL_SCANCODE_D]) {
		cameraPosition += cameraSpeed * cameraRight;
	}
	if (state[SDL_SCANCODE_Q]) {
		cameraPosition -= cameraSpeed * worldUp;
	}
	if (state[SDL_SCANCODE_E]) {
		cameraPosition += cameraSpeed * worldUp;
	}

	return quitEvent;
}

void gui()
{
	// Inform imgui of new frame
	ImGui_ImplSdlGL3_NewFrame(g_window);

	// ----------------- Set variables --------------------------
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	// ----------------------------------------------------------
	// Render the GUI.
	ImGui::Render();
}

void collisionTest(void)
{
	if (shipBV.intersect(goalBV)) {
		std::cout << "win" << std::endl;
	}
}


int main(int argc, char *argv[])
{
	g_window = labhelper::init_window_SDL("OpenGL Project");

	initGL();

	bool stopRendering = false;
	auto startTime = std::chrono::system_clock::now();

	while (!stopRendering) {
		//update currentTime
		std::chrono::duration<float> timeSinceStart = std::chrono::system_clock::now() - startTime;
		previousTime = currentTime;
		currentTime  = timeSinceStart.count();
		deltaTime    = currentTime - previousTime;

		collisionTest();

		// render to window
		display();

		// Render overlay GUI.
		if (showUI) {
			gui();
		}

		// Swap front and back buffer. This frame will now been displayed.
		SDL_GL_SwapWindow(g_window);

		// check events (keyboard among other)
		stopRendering = handleEvents();
	}
	// Free Models
	labhelper::freeModel(fighterModel);
	labhelper::freeModel(landingpadModel);
	labhelper::freeModel(sphereModel);

	// Shut down everything. This includes the window and all other subsystems.
	labhelper::shutDown(g_window);
	return 0;
}
