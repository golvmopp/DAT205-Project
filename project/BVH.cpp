#include "BVH.h"
#include <AABB.h>
#include <algorithm>
#include <Model.h>
/*
	Used this to implement: http://www.randygaul.net/2013/08/06/dynamic-aabb-tree/
*/

struct Node
{
	bool IsLeaf(void) const
	{
		// The right leaf does not use the same memory as the userdata,
		// and will always be Null (no children)
		return right == Null;
	}

	// Fat AABB for leafs, bounding AABB for branches
	AABB aabb;

	union
	{
		unsigned int parent;
		unsigned int next; // free list
	};

	union
	{
		// Child indices
		struct
		{
			unsigned int left;
			unsigned int right;
		};

		// Since only leaf nodes hold userdata, we can use the
		// same memory used for left/right indices to store
		// the userdata void pointer
		void *userData;
	};

	// leaf = 0, free nodes = -1
	unsigned int height;

	static const unsigned int Null = -1;
};

/*
	params: AABB, Parent, Next, Right, Left, Height
*/
Node createNode(AABB aabb, unsigned int p, unsigned int n, unsigned int r, unsigned int l, unsigned int h)
{
	Node node;
	node.aabb = AABB(vec3(0.f), vec3(0.f));
	node.parent = p; node.next = n;
	node.right = r; node.left = l;
	node.height = h;
	return node;
}

BVH::BVH()
{
}

BVH::~BVH()
{
}

Node BVH::insert(labhelper::Model * model)
{

	/*
	Insertion involves creating a fat AABB to bound the userdata associated with the new leaf node. 
	A fat AABB is just an AABB that is slightly larger than a tight fitting AABB. 
	Once a new leaf node is created with its fat AABB a tree traversal is required in order to 
	find a sibling to pair up with, and a new parent branch is constructed.

	Traversing the tree should be done by following some sort of cost heuristic. 
	The best seems to be a cost involving the surface area of involved nodes. 
	See Box2D for a resource in cost heuristics.

	After a new parent is created and a sibling found, the bounding volume hierarchy of all 
	parent nodes are invalid. A traversal up the tree, correcting all the bounding AABBs and 
	node heights is required. This hierarchy correction can be abstracted into a simple function:
	*/
	Node node;
	for (auto & mesh : model->m_meshes) {
		/* std::cout << value; ... */

		for (int i = mesh.m_start_index; i < mesh.m_number_of_vertices + mesh.m_start_index; i++) {
			// Loop through these vertices to find extreme points, create the "top" AABB from that.
		}

		// Split the top aabb into four, or 8 if we want non-flat tracks, check in each if there're any vertices,
		// if there is that's a smaller AABB in the hierarchy, do the same with that as with the top one (split).
		// if not, ignore. Stop the recursion

	}

	return node;
}

void BVH::remove()
{
	/*
	Removing nodes from binary search trees can involve a stack to trace back a traversal path, 
	however due to the few invariants of the dynamic AABB tree removal quite simple. 
	Since all branches must contain two valid children, and only leaves contain userdata, 
	the only nodes that require deletion are leaves.

	Remove the leaf’s parent and replace it with the leaf’s sibling. 
	After this operation the parent hierarchy needs to be recalculated (just as in insert).
	*/
}


// Takes two AABBs and returns a new AABB that encapsulates both input AABBs
AABB BVH::combine(AABB a, AABB b)
{
	float xMax = std::max(a.getMax(0), b.getMax(0));
	float yMax = std::max(a.getMax(1), b.getMax(1));
	float zMax = std::max(a.getMax(2), b.getMax(2));

	float xMid = (a.center[0] + b.center[0]) / 2.f;
	float yMid = (a.center[1] + b.center[1]) / 2.f;
	float zMid = (a.center[2] + b.center[2]) / 2.f;
	
	vec3 center = vec3(xMid, yMid, zMid);
	vec3 extents = vec3(xMax - xMid, yMax - yMid, zMax - zMid);
	
	return AABB(center, extents);
}

 // Syncs the tree after adding a node on index
 // This function is copied from the link at the top
void BVH::sync(unsigned int index)
{
	while (index)
	{
		unsigned int left = m_nodes[index].left;
		unsigned int right = m_nodes[index].right;

		m_nodes[index].height = 1 + std::max(m_nodes[left].height, m_nodes[right].height);
		m_nodes[index].aabb = combine(m_nodes[left].aabb, m_nodes[right].aabb);

		index = m_nodes[index].parent;
	}
}