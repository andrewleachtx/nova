/*
    Original code from Dr. Shinjiro Sueda's computer graphics and animation courses.

    Retrieved and modified by Andrew Leach, 2025
*/
#include "Mesh.h"
#include <algorithm>
#include <iostream>
#include <unordered_map>

#include "GLSL.h"
#include "Program.h"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

using namespace std;
const float NEGINF = numeric_limits<float>::lowest();
const float POSINF = numeric_limits<float>::infinity();

Mesh::Mesh() : posBufID(0), norBufID(0), texBufID(0), vaoID(0), eleBufID(0) {}

Mesh::~Mesh() {
    if (posBufID != 0) {
        glDeleteBuffers(1, &posBufID);
    }
    if (norBufID != 0) {
        glDeleteBuffers(1, &norBufID);
    }
    if (texBufID != 0) {
        glDeleteBuffers(1, &texBufID);
    }
    if (vaoID != 0) {
        glDeleteVertexArrays(1, &vaoID);
    }
    if (eleBufID != 0) {
        glDeleteBuffers(1, &eleBufID);
    }
}

void Mesh::loadMesh(const string &meshName) {
	// Load geometry
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	string warnStr, errStr;
	bool rc = tinyobj::LoadObj(&attrib, &shapes, &materials, &warnStr, &errStr, meshName.c_str());
	if(!rc) {
		cerr << errStr << endl;
	} else {
		min_XYZ = glm::vec3(POSINF);
		max_XYZ = glm::vec3(NEGINF);
		for (size_t s = 0; s < shapes.size(); s++) {
			// Loop over faces (polygons)
			size_t index_offset = 0;
			for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
				size_t fv = shapes[s].mesh.num_face_vertices[f];
				// Loop over vertices in the face.
				for(size_t v = 0; v < fv; v++) {
					// access to vertex
					tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                        posBuf.push_back(attrib.vertices[3*idx.vertex_index+0]);
                        posBuf.push_back(attrib.vertices[3*idx.vertex_index+1]);
                        posBuf.push_back(attrib.vertices[3*idx.vertex_index+2]);

                        if (!attrib.normals.empty()) {
                            norBuf.push_back(attrib.normals[3*idx.normal_index+0]);
                            norBuf.push_back(attrib.normals[3*idx.normal_index+1]);
                            norBuf.push_back(attrib.normals[3*idx.normal_index+2]);
                        }
                        if (!attrib.texcoords.empty()) {
                            texBuf.push_back(attrib.texcoords[2*idx.texcoord_index+0]);
                            texBuf.push_back(attrib.texcoords[2*idx.texcoord_index+1]);
                        }

                        // Update bounds
                        min_XYZ.x = min(min_XYZ.x, attrib.vertices[3*idx.vertex_index+0]);
                        min_XYZ.y = min(min_XYZ.y, attrib.vertices[3*idx.vertex_index+1]);
                        min_XYZ.z = min(min_XYZ.z, attrib.vertices[3*idx.vertex_index+2]);

                        max_XYZ.x = max(max_XYZ.x, attrib.vertices[3*idx.vertex_index+0]);
                        max_XYZ.y = max(max_XYZ.y, attrib.vertices[3*idx.vertex_index+1]);
                        max_XYZ.z = max(max_XYZ.z, attrib.vertices[3*idx.vertex_index+2]);
				}

				index_offset += fv;
				// shapes[s].mesh.material_ids[f];
			}
		}
	}
}

void Mesh::fitToUnitBox() {
	// Scale the vertex positions so that they fit within [-1, +1] in all three dimensions.
	glm::vec3 vmin(posBuf[0], posBuf[1], posBuf[2]);
	glm::vec3 vmax(posBuf[0], posBuf[1], posBuf[2]);
	for(int i = 0; i < (int)posBuf.size(); i += 3) {
		glm::vec3 v(posBuf[i], posBuf[i+1], posBuf[i+2]);
		vmin.x = min(vmin.x, v.x);
		vmin.y = min(vmin.y, v.y);
		vmin.z = min(vmin.z, v.z);
		vmax.x = max(vmax.x, v.x);
		vmax.y = max(vmax.y, v.y);
		vmax.z = max(vmax.z, v.z);
	}
	glm::vec3 center = 0.5f*(vmin + vmax);
	glm::vec3 diff = vmax - vmin;
	float diffmax = diff.x;
	diffmax = max(diffmax, diff.y);
	diffmax = max(diffmax, diff.z);
	float scale = 1.0f / diffmax;
	for(int i = 0; i < (int)posBuf.size(); i += 3) {
		posBuf[i  ] = (posBuf[i  ] - center.x) * scale;
		posBuf[i+1] = (posBuf[i+1] - center.y) * scale;
		posBuf[i+2] = (posBuf[i+2] - center.z) * scale;
	}
}

void Mesh::init() {
    glGenVertexArrays(1, &vaoID);
    glBindVertexArray(vaoID);

	// Send the position array to the GPU
	glGenBuffers(1, &posBufID);
	glBindBuffer(GL_ARRAY_BUFFER, posBufID);
	glBufferData(GL_ARRAY_BUFFER, posBuf.size()*sizeof(float), &posBuf[0], GL_STATIC_DRAW);
	
	// Send the normal array to the GPU
	if(!norBuf.empty()) {
		glGenBuffers(1, &norBufID);
		glBindBuffer(GL_ARRAY_BUFFER, norBufID);
		glBufferData(GL_ARRAY_BUFFER, norBuf.size()*sizeof(float), &norBuf[0], GL_STATIC_DRAW);
	}

	if(!texBuf.empty()) {
		glGenBuffers(1, &texBufID);
		glBindBuffer(GL_ARRAY_BUFFER, texBufID);
		glBufferData(GL_ARRAY_BUFFER, texBuf.size()*sizeof(float), &texBuf[0], GL_STATIC_DRAW);
	}

    glGenBuffers(1, &eleBufID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eleBufID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBuf.size() * sizeof(unsigned int), indexBuf.data(), GL_STATIC_DRAW);

    // Unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    GLSL::checkError(GET_FILE_LINE);
}

// Unused
void Mesh::updatePosBuf(const std::vector<glm::vec3>& new_posBuf) {
    if (posBuf.size() != new_posBuf.size() * 3) {
        cerr << "Pos buf size different, failing to update" << __FILE__ << ": " << __LINE__ << endl; 
        return;
    }

    // Update posBuf with new positions
    for (int i = 0; i < new_posBuf.size(); i++) {
        posBuf[i * 3] = new_posBuf[i].x;
        posBuf[i * 3 + 1] = new_posBuf[i].y;
        posBuf[i * 3 + 2] = new_posBuf[i].z;
    }

    // Update the VBO on the GPU
    glBindBuffer(GL_ARRAY_BUFFER, getPosBufID());
    glBufferSubData(GL_ARRAY_BUFFER, 0, posBuf.size() * sizeof(float), posBuf.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Mesh::draw(Program& prog, bool instanced, size_t offset, size_t num_instances) const {
	// Bind position buffer
	int h_pos = prog.getAttribute("aPos");
	glEnableVertexAttribArray(h_pos);
	glBindBuffer(GL_ARRAY_BUFFER, posBufID);
	glVertexAttribPointer(h_pos, 3, GL_FLOAT, GL_FALSE, 0, (const void *)0);

	// Bind normal buffer
	int h_nor = prog.getAttribute("aNor");
	if(h_nor != -1 && norBufID != 0) {
		glEnableVertexAttribArray(h_nor);
		glBindBuffer(GL_ARRAY_BUFFER, norBufID);
		glVertexAttribPointer(h_nor, 3, GL_FLOAT, GL_FALSE, 0, (const void *)0);
	}

	// Bind texcoords buffer
	int h_tex = prog.getAttribute("aTex");
	if(h_tex != -1 && texBufID != 0) {
		glEnableVertexAttribArray(h_tex);
		glBindBuffer(GL_ARRAY_BUFFER, texBufID);
		glVertexAttribPointer(h_tex, 2, GL_FLOAT, GL_FALSE, 0, (const void *)0);
	}

    int idx_ct = (int)posBuf.size() / 3;
    if (instanced) {
        glDrawArraysInstanced(GL_TRIANGLES, (GLint)offset, (GLsizei)idx_ct, (GLsizei)num_instances);
    }
    else {
        glDrawArrays(GL_TRIANGLES, 0, idx_ct);
    }
    
	// Disable and unbind
	if(h_tex != -1) {
	 	glDisableVertexAttribArray(h_tex);
	}
	if(h_nor != -1) {
		glDisableVertexAttribArray(h_nor);
	}

	glDisableVertexAttribArray(h_pos);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	GLSL::checkError(GET_FILE_LINE);
}

const unsigned Mesh::getPosBufID() const {
	return posBufID;
}

const size_t Mesh::getPosBufSize() const {
    return posBuf.size();
}

const unsigned Mesh::getVAOID() const {
    return vaoID;
}