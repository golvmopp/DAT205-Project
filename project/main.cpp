

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
bool showUI = true;
int windowWidth, windowHeight;

//car speed physics
float speed = 0.f;
float velocity = 0.f;
float dragCoeff = 0.25f;
float dragForce = 0.f;
float acceleration = 0.f;


// Framebuffers for post processing
std::vector<FboInfo> fboList;
// Samples for ambient occlusion
glm::vec3 hemisphereSamples[16];
glm::vec3 noiseTex[16];
GLuint ssaoTex;

// Shadow stuff
FboInfo shadowMapFB;
int shadowMapResolution = 512;
float polygonOffset_factor = 1.0f;
float polygonOffset_units = 0.8f;
float innerSpotlightAngle = 20.f;
float outerSpotlightAngle = 22.5f;

///////////////////////////////////////////////////////////////////////////////
// Shader programs
///////////////////////////////////////////////////////////////////////////////
GLuint shaderProgram; // Shader for rendering the final image
GLuint simpleShaderProgram; // Shader used to draw the shadow map
GLuint backgroundProgram;
GLuint postFXshader; //Shader for post processing effects
GLuint cutoffShader, hBlurShader, vBlurShader, motionBlurShader;
//FboInfo shadowMapFB;

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
 AABB cps[] = {AABB(vec3(0.f, 10.f, 0.f), vec3(50.f, 50.f, 200.f)),
			   AABB(vec3(-3615.f, 10.f, -1825.f), vec3(200.f, 50.f, 200.f)) };

 /*
outer corners:
 -872, 10, 114
-872, 10, -380
362, 10, -380
362, 10, 114

inner corners:
165, 10, -120
-662, 10, -120
-662, 10, -165
165, 10, -165
mid (-248.5, 42.5)
*/

 AABB yesBox = AABB(vec3(-248.5f, 10.f, 42.5f), vec3(413.5f, 300.f, 22.5f));


 int noOfCheckpoints = 2;
 int nextCheckpoint = 1;

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
	fighterModel = labhelper::loadModelFromOBJ("../scenes/box_ship.obj");
	landingpadModel = labhelper::loadModelFromOBJ("../scenes/racetrack_basic.obj");
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
	// SSAO initialization
	///////////////////////////////////////////////////////////////////////

	//Generating the sample kernel
	int nrSamples = 16;
	for (int i = 0; i < nrSamples; i++) {
		vec3 tmp = labhelper::cosineSampleHemisphere();
		tmp = tmp * labhelper::randf();

		//accelerating interpolation function (fewer samples away from origin)
		hemisphereSamples[i] = tmp;
		float scale = float(i) / float(nrSamples);
		scale = mix(0.1f, 1.0f, scale * scale);
	}

	//Generating the noise texture
	for (int i = 0; i < nrSamples; ++i) {
		noiseTex[i] = vec3(
			labhelper::randf(),
			labhelper::randf(),
			0.0f
			);
		noiseTex[i] = normalize(noiseTex[i]);
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
	//shadow mapping
	mat4 lightMatrix = lightProjectionMatrix * lightViewMatrix * inverse(viewMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "lightMatrix", lightMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "viewSpaceLightDir", normalize(vec3(viewMatrix * vec4(-lightPosition, 0.0f))));
	labhelper::setUniformSlow(currentShaderProgram, "spotOuterAngle", std::cos(radians(outerSpotlightAngle)));
	labhelper::setUniformSlow(currentShaderProgram, "spotInnerAngle", std::cos(radians(innerSpotlightAngle)));
	labhelper::setUniformSlow(currentShaderProgram, "texmapscale", shadowMapResolution);

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
	Applies the bloom filter and sends to render. Activates GL_BLEND
*/
void bloom(int write)
{

	//bright area cutoffs
	glBindFramebuffer(GL_FRAMEBUFFER, fboList[write].framebufferId);
	glUseProgram(cutoffShader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, fboList[write - 1].colorTextureTargets[0]);
	labhelper::drawFullScreenQuad();

	write++;


	// blur light sources horizontally
	glBindFramebuffer(GL_FRAMEBUFFER, fboList[write].framebufferId);
	glUseProgram(hBlurShader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, fboList[write - 1].colorTextureTargets[0]);
	labhelper::drawFullScreenQuad();

	write++;

	//vertically blurred horizontal blur, send to render.
	//activates blending, resets color and depth buffers
	glBindFramebuffer(GL_FRAMEBUFFER, fboList[4].framebufferId);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	glViewport(0, 0, windowWidth, windowHeight);
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(vBlurShader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, fboList[write - 1].colorTextureTargets[0]);

	labhelper::drawFullScreenQuad();

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
			//Maybe need to update FBO sizes
			for (int i = 0; i < 5; i++) {
				fboList[i].resize(w, h);
			}
		}
		////especially for shadow
		if (shadowMapFB.width != shadowMapResolution || shadowMapFB.height != shadowMapResolution) {
			shadowMapFB.resize(shadowMapResolution, shadowMapResolution);
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// setup matrices
	///////////////////////////////////////////////////////////////////////////

	vec4 ref = vec4(10.f, 6.f, 0.f, 1.f);
	vec4 tref = shipRotation * ref;
	cameraPosition = vec3(tref + shipTranslation[3]);

	mat4 projMatrix = perspective(radians(45.0f), float(windowWidth) / float(windowHeight), 5.0f, 5000.0f);

	mat4 viewMatrix = lookAt(cameraPosition, vec3(shipTranslation[3])*vec3(1.0f, 1.2f, 1.f), worldUp);
	

	vec4 lightStartPosition = vec4(40.0f, 40.0f, 0.0f, 1.0f);
	//lightPosition = vec3(rotate(currentTime, worldUp) * lightStartPosition); //rotating sun
	lightPosition = vec3(lightStartPosition.x, lightStartPosition.y, lightStartPosition.z); //static sun
	mat4 lightViewMatrix = lookAt(lightPosition, vec3(0.0f), worldUp);
	mat4 lightProjMatrix = perspective(radians(45.0f), 1.0f, 25.0f, 100.0f);

	///////////////////////////////////////////////////////////////////////////
	// Movement updates
	///////////////////////////////////////////////////////////////////////////
	shipTranslation[3] -= speed * shipRotation[0]; //speed update
	//shipTranslation[2] -= speed * shipRotation[1]; //gravity

	shipBV.move(vec3(shipTranslation[3]));
	
	
	///////////////////////////////////////////////////////////////////////////
	// Bind the environment map(s) to unused texture units
	///////////////////////////////////////////////////////////////////////////
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, environmentMap);
	glActiveTexture(GL_TEXTURE7);
	glBindTexture(GL_TEXTURE_2D, irradianceMap);
	glActiveTexture(GL_TEXTURE8);
	glBindTexture(GL_TEXTURE_2D, reflectionMap);
	
	//Shadow stuff
	glActiveTexture(GL_TEXTURE10);
	glBindTexture(GL_TEXTURE_2D, shadowMapFB.depthBuffer);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	vec4 zeros(0.0f);
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &zeros.x);
	
	glActiveTexture(GL_TEXTURE0);

	// Shadow tomfoolery
	glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFB.framebufferId);
	glViewport(0, 0, shadowMapFB.width, shadowMapFB.height);
	glClearColor(0.2, 0.2, 0.8, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(polygonOffset_factor, polygonOffset_units);

	drawScene(shaderProgram, lightViewMatrix, lightProjMatrix, lightViewMatrix, lightProjMatrix);

	glDisable(GL_POLYGON_OFFSET_FILL);


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

	//Applies a bloom filter. Note that this activates GL_BLEND!
	bloom(1);

	glBindFramebuffer(GL_FRAMEBUFFER, fboList[4].framebufferId);
	glUseProgram(postFXshader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, fboList[0].colorTextureTargets[0]);
	labhelper::drawFullScreenQuad();


	glDisable(GL_BLEND);

	//motion-blur pass
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, fboList[0].depthBuffer);

	glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);

	glUseProgram(motionBlurShader);
	//Setting some uniforms for motion blur. MUST BE DONE AFTER glUseProgram!
	labhelper::setUniformSlow(motionBlurShader, "previousViewProjectionMatrix", previousViewProjectionMatrix);
	labhelper::setUniformSlow(motionBlurShader, "viewProjectionInverse", inverse(projMatrix * viewMatrix));
	glActiveTexture(GL_TEXTURE0);

	glBindTexture(GL_TEXTURE_2D, fboList[4].colorTextureTargets[0]);

	labhelper::drawFullScreenQuad();



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
	float gospeed	= 1.f;
	float godspeed = 4.0f;
	
	

	if (speed != 0) {
		if (state[SDL_SCANCODE_RIGHT]) {
			
			shipRotation[0] += (rotspeed - rotspeed*0.07f*speed) * shipRotation[2];
			
		}
		if (state[SDL_SCANCODE_LEFT]) {
			shipRotation[0] -= (rotspeed - rotspeed*0.07f*speed) * shipRotation[2];

		}
	}

	shipRotation[0] = normalize(shipRotation[0]);
	shipRotation[2] = vec4(cross(vec3(shipRotation[0]), vec3(shipRotation[1])), 0.0f);

	if (state[SDL_SCANCODE_UP]) {
		if (speed <= 10.f) {
			dragForce = -dragCoeff * velocity;
			acceleration = .01f + dragForce;
			velocity += acceleration;
			speed += velocity;
		}
	} 
	else if (state[SDL_SCANCODE_B]) {
		if (speed <= 10.f) {
			speed += velocity;
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

bool checkCheckPoint(void)
{
	if (shipBV.intersect(cps[nextCheckpoint])) {
		if (nextCheckpoint != 0) { // This is a normal checkpoint
			std::cout << "checkpoint " << nextCheckpoint << " reached!" << std::endl;
			nextCheckpoint = (nextCheckpoint + 1) % noOfCheckpoints;
			return false;
		}
		else { // This is the goal checkpoint
			std::cout << "Winner!" << std::endl;
			nextCheckpoint = (nextCheckpoint + 1) % noOfCheckpoints;
			return true;
		}
	}
}


vec3 findDir()
{
	return vec3(1.f);
}

void checkCollisions(void)
{
	if (shipBV.intersect(yesBox)) {
		//shipTranslation *= translate(findDir());
		speed = 0;

	}
}

int main(int argc, char *argv[])
{
	g_window = labhelper::init_window_SDL("OpenGL Project");

	initGL();

	bool stopRendering = false;
	auto startTime = std::chrono::system_clock::now();
	// start lap1 timer
	float lapStart = (std::chrono::system_clock::now() - startTime).count();

	while (!stopRendering) {
		//update currentTime
		std::chrono::duration<float> timeSinceStart = std::chrono::system_clock::now() - startTime;
		previousTime = currentTime;
		currentTime  = timeSinceStart.count();
		deltaTime    = currentTime - previousTime;

		// check events (keyboard among other)
		stopRendering = handleEvents();

		// render to window
		display();
		checkCollisions();

		if (checkCheckPoint())
		{
			float lapTime = currentTime - lapStart;
			lapStart = currentTime;
			std::cout << lapTime << std::endl;
		}
		
		// Render overlay GUI.
		if (showUI) {
			gui();
		}

		// Swap front and back buffer. This frame will now been displayed.
		SDL_GL_SwapWindow(g_window);


	}
	// Free Models
	labhelper::freeModel(fighterModel);
	labhelper::freeModel(landingpadModel);
	labhelper::freeModel(sphereModel);

	// Shut down everything. This includes the window and all other subsystems.
	labhelper::shutDown(g_window);
	return 0;
}
