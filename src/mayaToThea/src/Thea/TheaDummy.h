#ifndef THEA_RENDER_H
#define THEA_RENDER_H

#include <vector>
#include "rendering/renderer.h"
#include "shadingtools/shadingUtils.h"
#include "shadingtools/material.h"

#include <maya/MObject.h>
#include <maya/MMatrix.h>
#include <maya/MColor.h>
#include <maya/MString.h>


class mtth_MayaScene;
class mtth_RenderGlobals;
class mtth_MayaObject;


class TheaShadingNode
{
public:
	enum ShaderType { UNDEF = 0, BASIC_BSDF, BSSDF_BSDF, COATING_BSDF, THINFILM_BSDF, GLOSSY_BSDF, MATERIAL, FILETEXTURE, CHECHERTEXTURE, CURLTEXTURE, MARBLETEXTURE };
	void *data;
	ShaderType type;

	TheaShadingNode()
	{
		data = NULL;
		type = UNDEF;
	}
	~TheaShadingNode()
	{
		if (data != NULL)
		{
		}
	}
};

class ShadingNodeMap
{
public:
	std::vector<std::string> names;
	std::vector<TheaShadingNode> shadingNodes;
	void add(std::string name, TheaShadingNode& sn)
	{
		TheaShadingNode tmpsn;
		if (!this->find(name, tmpsn))
		{
			names.push_back(name);
			shadingNodes.push_back(sn);
		}
	}
	bool find(std::string name, TheaShadingNode& sn)
	{
		for (size_t i = 0; i < names.size(); i++)
		{
			if (names[i] == name)
			{
				sn = shadingNodes[i];
				return true;
			}
		}
		return false;
	}
	void clear()
	{
		names.clear();
		shadingNodes.clear();
	}
};


class TheaDRenderer : public MayaTo::Renderer
{
public:

	mtth_MayaScene *mtth_scene;
	mtth_RenderGlobals *mtth_renderGlobals;

	bool isGood;

	std::vector<mtth_MayaObject *> interactiveUpdateList;
	std::vector<MObject> interactiveUpdateMOList;

	TheaDRenderer(){};
	~TheaDRenderer(){};
	void defineCamera(){};
	void defineEnvironment(){};
	void defineGeometry(){};
	void defineLights(){};
	void defineSettings();
	//TheaSDK::Normal3D getSunDirection();

	void render(){};

	void initializeRenderer(){};
	void updateShape(MayaObject *obj){};
	void updateTransform(MayaObject *obj){};
	void IPRUpdateEntities(){};
	void reinitializeIPRRendering(){};
	void abortRendering(){};

	void defineMesh(mtth_MayaObject *obj){};
	void frameBufferCallback(){};
	static void renderEndCallback(void *ptr){};
	void doTestRender(){};
	void matrixToTransform(MMatrix& m, TheaSDK::Transform& t){};
	void defineShader(mtth_MayaObject *obj){};
	void defineIBLNode(TheaSDK::XML::EnvironmentOptions::IBLMap&, const char *attName){};
	void setTextureOrColor(const char *attrName, MObject& shaderNode, void *theaTextureNode){};
	void setTextureOrFloat(const char *attrName, MObject& shaderNode, void *theaTextureNode){};
	void createDefaultMaterial(){};
	bool assignDefinedShader(mtth_MayaObject *obj, Material& mat){};
	void clearMaterialLists(){};
	void createTheaShadingNode(ShadingNode& sn, mtth_MayaObject *obj){};
	void assignParameters(ShadingNode& sn, TheaShadingNode& tsn){};
	//void assignObjParameters(ShadingNode& sn, Object& o){};
};

#endif