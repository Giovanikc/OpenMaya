#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnMessageAttribute.h>

#include "mt@_renderGlobalsNode.h"

MTypeId	MayaTo@Globals::id(0x000000);

//	------------- automatically created attributes start ----------- // 
//	------------- automatically created attributes end ----------- // 


MayaTo@Globals::MayaTo@Globals()
{
	imageFormatList.append("Exr");
}

MayaTo@Globals::~MayaTo@Globals()
{}


void *MayaTo@Globals::creator()
{
	return new MayaTo@Globals();
}

MStatus	MayaTo@Globals::initialize()
{
	MayaRenderGlobalsNode::initialize();

	MFnNumericAttribute nAttr;
	MFnTypedAttribute tAttr;
	MFnGenericAttribute gAttr;
	MFnEnumAttribute eAttr;
	MFnMessageAttribute mAttr;

	MStatus stat = MStatus::kSuccess;

//	------------- automatically created attributes start ----------- // 
//	------------- automatically created attributes end ----------- // 

	return stat;

}

