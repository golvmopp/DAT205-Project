#pragma once
#include <glm/glm.hpp>
using namespace glm;

class AABB
{
private: 

public:
	vec3 center;
	vec3 extent;
	AABB(vec3 center, vec3 extent);
	~AABB();
	AABB();
	vec3 move(vec3 newCenter);
	float getMax(char xyz);
	float getMin(char xyz);
	bool intersect(AABB other);
};

