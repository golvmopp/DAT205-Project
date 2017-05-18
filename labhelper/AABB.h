#pragma once
#include <glm/glm.hpp>
using namespace glm;

class AABB
{
private:
	AABB();
	float getMax(char xyz);
	float getMin(char xyz);
public:
	AABB(vec3 center, vec3 extent);
	~AABB();
	vec3 move(vec3 newCenter);
	bool intersect(AABB other);
};

