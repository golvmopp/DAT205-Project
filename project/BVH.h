#include <vector>
#include <AABB.h>
#include <Model.h>
struct Node;

class BVH
{
public:
	BVH();
	~BVH();

	Node createNode();
	Node insert(labhelper::Model * model);
	void remove();

private:
	std::vector<Node> m_nodes;

	void sync(unsigned int index);
	AABB combine(AABB a, AABB b);
};