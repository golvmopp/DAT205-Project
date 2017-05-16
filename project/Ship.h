
#include <Model.h>
#include <labhelper.h>
#include <glm/glm.hpp>
using namespace glm;

class Ship
{

public:
	labhelper::Model *model = nullptr;
	mat4 modelMatrix;
	mat4 T, R;
	float speed;
	Ship();
	~Ship();
};

