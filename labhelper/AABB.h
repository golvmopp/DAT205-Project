#pragma once
class AABB
{
public:
	AABB(vec3 center, vec3 extent);
	~AABB();
	vec3 move(vec3 newCenter);
	float getMax(char xyz);
	float getMin(char xyz);
	bool intersect(AABB other);
};
