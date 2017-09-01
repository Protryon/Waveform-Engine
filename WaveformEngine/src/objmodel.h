#ifndef OBJMODEL_H_
#define OBJMODEL_H_

#include <stdint.h>
#include <unistd.h>
#include "hashmap.h"

struct mtl {
		float red;
		float green;
		float blue;
		//we dont use the rest for now
};

struct mtllib {
		struct hashmap* mtls;
};

struct objvertex {
		double x;
		double y;
		double z;
};

struct objface {
		struct mtl* mtl;
		uint32_t* verticies;
		size_t vertex_count;
};

struct objgroup {
		char* name;
		struct objface* faces;
		size_t face_count;
};

struct objmodel {
		struct objvertex* verticies;
		size_t vertex_count;
		struct objgroup* groups;
		size_t group_count;
		struct mtllib* mtllib;
};

struct objmodel* obj_readModel(char* model);

void obj_freeModel(struct objmodel* model);

void obj_drawFace(struct objmodel* model, struct objface* face);

void obj_drawGroup(struct objmodel* model, struct objgroup* group);

void obj_drawModel(struct objmodel* model);

#endif /* OBJMODEL_H_ */
