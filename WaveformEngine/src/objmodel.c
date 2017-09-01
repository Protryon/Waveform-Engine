#include "objmodel.h"
#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>
#include "smem.h"
#include "xstring.h"
#include <fcntl.h>
#include <errno.h>
#include "hashmap.h"

struct objmodel* obj_readModel(char* model) {
	struct objmodel* mod = scalloc(sizeof(struct objmodel));
	char* cl = model;
	size_t ml = strlen(model);
	int state = 0;
	struct objgroup* group = NULL;
	struct mtl* cmtl = NULL;
	for (size_t i = 0; i < ml; i++) {
		if (model[i] == '\n' || i == ml - 1) {
			if (model[i] == '\n') model[i] = 0;
			cl = trim(cl);
			if (strlen(cl) == 0 || cl[0] == '#' || startsWith(cl, "s ")) goto cnt;
			if (startsWith(cl, "mtllib")) {
				char* fil = cl + 7;
				if (startsWith(fil, "/") || contains(fil, "..") || !endsWith(fil, ".mtl")) goto sk;
				char* tex = smalloc(1025);
				size_t texs = 1024;
				size_t texi = 0;
				char path[1024];
				snprintf(path, 1024, "assets/%s", fil);
				int fd = open(path, O_RDONLY);
				if (fd < 0) {
					printf("Error opening materials: %s\n", strerror(errno));
					exit(1);
				}
				ssize_t r = 0;
				while ((r = read(fd, tex + texi, texs - texi)) > 0) {
					texi += r;
					if (texs - texi < 1024) {
						texs += 1024;
						tex = srealloc(tex, texs + 1);
					}
				}
				tex[texi] = 0;
				close(fd);
				mod->mtllib = smalloc(sizeof(struct mtllib));
				mod->mtllib->mtls = new_hashmap(256, 0, 1, 0);
				while ((tex = strstr(tex, "newmtl ")) != NULL) {
					char* name = tex + 7;
					struct mtl* mtl = smalloc(sizeof(struct mtl));
					tex = strchr(name, '\n');
					if (tex == NULL) continue;
					tex[0] = 0;
					tex++;
					tex = strstr(tex, "Ka ");
					if (tex == NULL) continue;
					tex += 3;
					mtl->red = strtod(tex, &tex) * 256.;
					mtl->green = strtod(tex, &tex) * 256.;
					mtl->blue = strtod(tex, &tex) * 256.;
					put_hashmap(mod->mtllib->mtls, (uint64_t) name, strlen(name) + 1, mtl);
				}
				sk: ;
			}
			if (state == 0) {
				if (streq_nocase(cl, "g")) goto cnt;
				else if (startsWith(cl, "g ")) {
					state = 1;
					group = scalloc(sizeof(struct objgroup));
					group->name = xstrdup(cl + 2, 0);
				} else if (startsWith(cl, "v ")) {
					mod->verticies = srealloc(mod->verticies, (mod->vertex_count + 1) * sizeof(struct objvertex));
					mod->verticies[mod->vertex_count].x = strtod(cl + 2, &cl);
					mod->verticies[mod->vertex_count].y = strtod(cl, &cl);
					mod->verticies[mod->vertex_count].z = strtod(cl, &cl);
					mod->vertex_count++;
				} else goto cnt;
			} else if (state == 1) {
				if (startsWith(cl, "g ")) {
					mod->groups = srealloc(mod->groups, (mod->group_count + 1) * sizeof(struct objgroup));
					memcpy(&mod->groups[mod->group_count], group, sizeof(struct objgroup));
					mod->group_count++;
					group = scalloc(sizeof(struct objgroup));
					group->name = xstrdup(cl + 2, 0);
				} else if (startsWith(cl, "f ")) {
					group->faces = srealloc(group->faces, (group->face_count + 1) * sizeof(struct objface));
					group->faces[group->face_count].vertex_count = 0;
					group->faces[group->face_count].verticies = NULL;
					group->faces[group->face_count].mtl = cmtl;
					cl += 2;
					char* lcl = cl;
					do {
						lcl = cl;
						uint32_t n = (uint32_t) strtoll(cl, &cl, 10);
						if (cl == lcl) break;
						group->faces[group->face_count].verticies = srealloc(group->faces[group->face_count].verticies, (group->faces[group->face_count].vertex_count + 1) * sizeof(uint32_t));
						group->faces[group->face_count].verticies[group->faces[group->face_count].vertex_count] = n - 1;
						group->faces[group->face_count].vertex_count++;
					} while (lcl != cl);
					group->face_count++;
				} else if (startsWith(cl, "usemtl ")) {
					cmtl = get_hashmap(mod->mtllib->mtls, (uint64_t)(cl + 7), strlen(cl + 7) + 1);
				} else goto cnt;
			}
			cnt: ;
			cl = model + i + 1;
		}
	}
	if (group != NULL) {
		mod->groups = srealloc(mod->groups, (mod->group_count + 1) * sizeof(struct objgroup));
		memcpy(&mod->groups[mod->group_count], group, sizeof(struct objgroup));
		mod->group_count++;
	}
	return mod;
}

void obj_freeModel(struct objmodel* model) {

}

void obj_drawFace(struct objmodel* model, struct objface* face) {
	if (face->mtl != NULL) glColor3f(face->mtl->red, face->mtl->green, face->mtl->blue);
	glBegin (GL_POLYGON);
	for (size_t i = 0; i < face->vertex_count; i++) {
		if (face->verticies[i] < model->vertex_count) {
			struct objvertex* v = &model->verticies[face->verticies[i]];
			glVertex3f(v->x, v->y, v->z);
		}
	}
	glEnd();
}

void obj_drawGroup(struct objmodel* model, struct objgroup* group) {
	for (size_t i = 0; i < group->face_count; i++) {
		obj_drawFace(model, &group->faces[i]);
	}
}

void obj_drawModel(struct objmodel* model) {
	for (size_t i = 0; i < model->group_count; i++) {
		obj_drawGroup(model, &model->groups[i]);
	}
}
