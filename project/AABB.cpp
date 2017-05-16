#include "AABB.h"
#include <glm/glm.hpp>
using namespace glm;


vec3 center;
float xExtent, yExtent, zExtent;

AABB::AABB(vec3 c, float x, float y, float z)
{
	center = c;
	xExtent = x;
	yExtent = y;
	zExtent = z;
}


AABB::~AABB()
{
}
