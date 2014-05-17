#ifndef MT_SHADING_TOOLS_SHADINGNODE_H
#define MT_SHADING_TOOLS_SHADINGNODE_H

#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MObjectArray.h>
#include <maya/MString.h>
#include <string>
#include <vector>

struct ShaderAttribute
{
	ShaderAttribute()
	{
		connected = false;
	}
	std::string name;
	std::string type;
	bool connected;
	std::string connectedAttrName;
	std::string connectedNodeName;
	MObject connectedMObject;
};

#define SPLUG_LIST std::vector<ShadingPlug>

class ShadingNode
{
public:
	enum STATE{
		NONE = 0,
		INVALID = 1,
		VALID =2
	};

	MString typeName; //Lambert, MultiplyDivide
	MString fullName; //myLambert1, mdivi_number_123
	MObject mobject;
	STATE nodeState;
	std::vector<ShaderAttribute> inputAttributes;
	std::vector<ShaderAttribute> outputAttributes;

	ShadingNode(MObject& object);
	ShadingNode(const ShadingNode &other);
	ShadingNode();
	~ShadingNode();

	void setMObject(MObject object);
	void init(void);
	void getConnectedInputObjects(MObjectArray& objectArray);
	void getConnectedOutputObjects(MObjectArray& objectArray);
private:
};

#endif
