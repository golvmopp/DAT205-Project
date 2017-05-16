#include "AABB.h"
#include <glm/glm.hpp>
using namespace glm;


vec3 center, extent;

AABB::AABB(vec3 c, vec3 x)
{
	center = c;
	extent = x;
}

AABB::~AABB()
{
}

vec3 move(vec3 newCenter)
{
	center = newCenter;
	return center;
}

/*
 @param xyz [0, 1, 2] for [x, y, z] axis respectively
*/
float getMax(char xyz)
{
	return center[xyz] + extent[xyz];
}
/*
 @param xyz [0, 1, 2] for [x, y, z] axis respectively
*/
float getMin(char xyz)
{
	return center[xyz] - extent[xyz];
}

bool intersect(AABB other)
{
	return ( (getMin(0) <= other.getMax(0) && getMax(0) >= other.getMin(0)) &&
		(getMin(1) <= other.getMax(1) && getMax(1) >= other.getMin(1)) &&
		(getMin(2) <= other.getMax(2) && getMax(2) >= other.getMin(2)) )
}