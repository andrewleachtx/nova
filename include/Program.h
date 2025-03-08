/*
    Original code from Dr. Shinjiro Sueda's computer graphics and animation courses.

    Retrieved and modified by Andrew Leach, 2025
*/
#pragma  once
#ifndef PROGRAM_H
#define PROGRAM_H

#include <map>
#include <string>

#include <GL/glew.h>

/**
 * An OpenGL Program (vertex and fragment shaders)
 */
class Program
{
public:
	Program();
	virtual ~Program();
	
	void setVerbose(bool v) { verbose = v; }
	bool isVerbose() const { return verbose; }
	
	void setShaderNames(const std::string &v, const std::string &f);
	virtual bool init();
	virtual void bind();
	virtual void unbind();

	void addAttribute(const std::string &name);
	void addUniform(const std::string &name);
	GLint getAttribute(const std::string &name) const;
	GLint getUniform(const std::string &name) const;
	
protected:
	std::string vShaderName;
	std::string fShaderName;
	
private:
	GLuint pid;
	std::map<std::string,GLint> attributes;
	std::map<std::string,GLint> uniforms;
	bool verbose;
};

#endif
