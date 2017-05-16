#pragma once
class AABB
{
public:
	AABB(vec3 center, float xExtent, float yExtent, float zExtent);
	virtual ~AABB();
};

