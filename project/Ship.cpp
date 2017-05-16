#include "Ship.h"

#include <glm/gtx/transform.hpp>


Ship::Ship()
{
	model = labhelper::loadModelFromOBJ("../scenes/NewShip.obj");
	T = translate(vec3(0.f, 10.f, 0.f));
	R = mat4(1.f);
	modelMatrix = T * R;
	speed = .0f;
}


Ship::~Ship()
{
}
