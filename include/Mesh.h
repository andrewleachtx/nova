/*
    Original code from Dr. Shinjiro Sueda's computer graphics and animation courses.

    Retrieved and modified by Andrew Leach, 2025
*/
#pragma once
#ifndef MESH_H
#define MESH_H

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>

class Program;

/**
 * A shape defined by a list of triangles
 * - posBuf should be of length 3*ntris
 * - norBuf should be of length 3*ntris (if normals are available)
 * - texBuf should be of length 2*ntris (if texture coords are available)
 * posBufID, norBufID, and texBufID are OpenGL buffer identifiers.
 */
class Mesh
{
public:
	Mesh();
	virtual ~Mesh();
	void loadMesh(const std::string &meshName, bool isSpring=false);
	void fitToUnitBox();
	void init();

    void updatePosBuf(const std::vector<glm::vec3>& new_posBuf);
	void draw(Program& prog, bool indexed=false) const;
    const unsigned getPosBufID() const;
    const unsigned getPosBufSize() const;

    //! THESE ARE NOT CONST GETTERS!!! YOU COULD BE UPDATING
    std::vector<unsigned int>& getIndexBuf() { return indexBuf; }
    std::vector<float>& getPosBuf() { return posBuf; }
	std::vector<float>& getNorBuf() { return norBuf; }
    
    glm::vec3 getMinXYZ() const { return min_XYZ; }
	glm::vec3 getMaxXYZ() const { return max_XYZ; }
	
private:
    // Stores indices of vertices
    std::vector<unsigned int> indexBuf;

	std::vector<float> posBuf;
	std::vector<float> norBuf;
	std::vector<float> texBuf;

	unsigned posBufID;
	unsigned norBufID;
	unsigned texBufID;
    
    unsigned vaoID;
    unsigned eleBufID;

    glm::vec3 min_XYZ;
    glm::vec3 max_XYZ;
};

#endif
