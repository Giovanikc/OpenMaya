#include <maya/MDagPath.h>
#include <maya/MItDag.h>
#include <maya/MFnDagNode.h>
#include <maya/MDagPathArray.h>
#include <maya/MFnCamera.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnInstancer.h>
#include <maya/MFnParticleSystem.h>
#include <maya/MMatrixArray.h>
#include <maya/MIntArray.h>
#include <maya/MLightLinks.h>
#include <maya/MSelectionList.h>
#include <maya/MGlobal.h>
#include <maya/M3dView.h>
#include <maya/MRenderView.h>
#include <maya/MVectorArray.h>
#include <maya/MFileIO.h>

#include "mayaScene.h"
#include "utilities/logging.h"
#include "utilities/tools.h"
#include "utilities/attrTools.h"
#include "utilities/pystring.h"
#include "threads/renderQueueWorker.h"
#include "world.h"

static Logging logger;

MayaScene::MayaScene()
{
	this->init();
}

void MayaScene::init()
{
	this->cando_ipr = false;
	this->good = true;
	this->userThreadUpdateInterval = 50;
	this->renderType = NORMAL;
	this->needsUserThread = false;
	this->renderState = MayaScene::UNDEF;
	this->renderingStarted = false;
}

MString MayaScene::getExportPath(MString ext, MString rendererName)
{
	std::string currentFile = MFileIO::currentFile().asChar();
	std::vector<std::string> parts;
	pystring::split(currentFile, parts, "/");
	currentFile = pystring::replace(parts.back(), ".ma", "");
	currentFile = pystring::replace(currentFile, ".mb", "");
	MString scenePath = this->renderGlobals->basePath + "/" + rendererName + "/" + currentFile.c_str() + "." + ext;
	return scenePath;
}

MString MayaScene::getFileName()
{
	std::string currentFile = MFileIO::currentFile().asChar();
	std::vector<std::string> parts;
	pystring::split(currentFile, parts, "/");
	currentFile = pystring::replace(parts.back(), ".ma", "");
	currentFile = pystring::replace(currentFile, ".mb", "");
	return MString(currentFile.c_str());
}

bool MayaScene::canDoIPR()
{
	return this->cando_ipr;
}

MayaScene::~MayaScene()
{}

void MayaScene::setRenderType(RenderType rtype)
{
	this->renderType = rtype;
}
// Here is checked if the linklist is complete, what means that we do not have to do any 
// complicated light linking, but default procedures. If excludedLights is > 0 then we found 
// any excluded Lights.
bool MayaScene::listContainsAllLights(MDagPathArray& linkedLights, MDagPathArray& excludedLights)
{
	excludedLights.clear();	
	for( uint lId = 0; lId < this->lightList.size(); lId++)
	{
		MDagPath path = this->lightList[lId]->dagPath;
		bool found = false;
		for( uint liId = 0; liId < linkedLights.length(); liId++)
		{
			MDagPath linkedPath = linkedLights[liId];
			if( linkedPath == path )
			{
				found = true;
				break;
			}
		}		
		if( found )
			continue;
		else
			excludedLights.append(path);
	}
	
	if( excludedLights.length() > 0)
		return false;

	return true;
}

bool MayaScene::lightObjectIsInLinkedLightList(MayaObject *lightObject, MDagPathArray& linkedLightsArray)
{
	for (uint lId = 0; lId < linkedLightsArray.length(); lId++)
	{
		if (lightObject->mobject == linkedLightsArray[lId].node())
			return true;
	}
	return false;
}

// there are two types of linking, light linking and shadow linking.
// Light linking will be done in the object attributes, whereas shadow linking is an attribute of a light
void MayaScene::getLightLinking()
{
	//Logging::debug(MString("----------- MayaScene::getLightLinking ---------------"));
	MLightLinks lightLink;
    bool parseStatus;
	parseStatus = lightLink.parseLinks(MObject::kNullObj);

	for (int oId = 0; oId < this->objectList.size(); oId++)
	{
		MDagPathArray lightArray;
		MayaObject *obj = this->objectList[oId];
		
		if (!obj->mobject.hasFn(MFn::kMesh) && !obj->mobject.hasFn(MFn::kNurbsSurface) && !obj->mobject.hasFn(MFn::kNurbsCurve))
			continue;

		lightLink.getLinkedLights(obj->dagPath, MObject::kNullObj, lightArray);
		//for (uint lId = 0; lId < lightArray.length(); lId++)
		//{
		//	Logging::debug(MString("Object ") + obj->shortName + " has link to light: " + lightArray[lId].fullPathName());			
		//}

		// if one of the light in my scene light list is NOT in the linked light list,
		// the light has either turned off "Illuminate by default" or it is explicilty not linked to this object.
		for (size_t lObjId = 0; lObjId < this->lightList.size(); lObjId++)
		{
			if (lightObjectIsInLinkedLightList(this->lightList[lObjId], lightArray))
			{
				//Logging::debug(MString("Light object ") + this->lightList[lObjId]->shortName + " IS in lightList -> linked.");
			}
			else{
				//Logging::debug(MString("Light object ") + this->lightList[lObjId]->shortName + " is NOT in lightList -> " + obj->shortName + " is not linked.");
				this->lightList[lObjId]->excludedObjects.push_back(obj);
			}
		}

		//lightArray.clear();
		//lightLink.getIgnoredLights(obj->dagPath, MObject::kNullObj, lightArray);
		//for (uint lId = 0; lId < lightArray.length(); lId++)
		//{
		//	Logging::debug(MString("Object ") + obj->shortName + " ignores light: " + lightArray[lId].fullPathName());
		//}
	}
	//MDagPathArray lightArray, excludeLights;
	//MObject nullObj;
	//std::vector<MayaObject *> castNoShadowObjects;

	//for( int oId = 0; oId < this->objectList.size(); oId++)
	//{
	//	MayaObject *mo = this->objectList[oId];
	//	MDagPath path = mo->dagPath;
	//	// the ignored list is more useful because if we have no ignored lights the list is empty
	//	// at the moment this does not work for lights with "illuminates by default" turned off, so
	//	// I use the linkedLights which works and search for excluded ones
	//	mo->linkedLights.clear();
	//	lightLink.getLinkedLights(path, nullObj, lightArray); 	
	//	this->listContainsAllLights(lightArray, excludeLights);
	//	for( uint ll = 0; ll < excludeLights.length(); ll++)
	//	{
	//		MDagPath lpath = excludeLights[ll];
	//		mo->linkedLights.push_back(lpath);
	//	}

	//	bool castsShadows = true;
	//	MFnDependencyNode dn(mo->mobject);
	//	if( getBool(MString("castsShadows"), dn, castsShadows))
	//	{
	//		if( !castsShadows )
	//			castNoShadowObjects.push_back(mo);
	//	}
	//}

	//// shadow linking. Get objects which ignores shadows from a specific light
	//MSelectionList selectionList;
	//for( uint lId = 0; lId < this->lightList.size(); lId++)
	//{
	//	if( castNoShadowObjects.size() > 0)
	//	{
	//		for( uint oId = 0; oId < castNoShadowObjects.size(); oId++)
	//		{
	//			this->lightList[lId]->castNoShadowObjects.push_back(castNoShadowObjects[oId]->dagPath);
	//		}
	//	}
	//}

	//// search for shadow ignoring objects
	//for( uint lId = 0; lId < this->lightList.size(); lId++)
	//{
	//	MayaObject *mo = this->lightList[lId];
	//	MDagPath path = mo->dagPath;
	//	selectionList.clear();
	//	lightLink.getShadowIgnoredObjects(path, selectionList);
	//	int numObj = selectionList.length();
	//	for( int oId = 0; oId < numObj; oId++)
	//	{
	//		MDagPath obDag;
	//		selectionList.getDagPath(oId, obDag);
	//		//logger.debug(MString("Object ") +  obDag.partialPathName() + " ignores shadows from " + path.partialPathName());
	//		mo->shadowObjects.push_back(obDag);
	//	}
	//}
}

std::vector<MayaObject *> parentList;

void MayaScene::checkParent(MayaObject *obj)
{
	std::vector<MayaObject *>::iterator iter;
	MFnDagNode node(obj->mobject);
	if( node.parentCount() == 0)
	{	
		obj->parent = NULL;
		return;
	}
	MObject parent = node.parent(0);
	for( iter = parentList.begin(); iter != parentList.end(); iter++)
	{
		if( parent == (*iter)->mobject)
		{
			obj->parent = *iter;
			break;
		}
	}
}

MDagPath MayaScene::getWorld()
{
	MItDag   dagIterator(MItDag::kDepthFirst, MFn::kInvalid);
	MDagPath dagPath;
	
	for (dagIterator.reset(); (!dagIterator.isDone()); dagIterator.next())
	{
		dagIterator.getPath(dagPath);
		if (dagPath.apiType() == MFn::kWorld)
			break;
	}
	return dagPath;
}

bool MayaScene::isGeo(MObject obj)
{
	if( obj.hasFn(MFn::kMesh))
		return true;
	if( obj.hasFn(MFn::kNurbsSurface))
		return true;
	if( obj.hasFn(MFn::kParticle))
		return true;
	if( obj.hasFn(MFn::kSubSurface))
		return true;
	if( obj.hasFn(MFn::kNurbsCurve))
		return true;
	if( obj.hasFn(MFn::kHairSystem))
		return true;

	MFnDependencyNode depFn(obj);
	uint nodeId = depFn.typeId().id();
	for( uint lId = 0; lId < this->objectIdentifier.size(); lId++)
	{
		if( nodeId == this->objectIdentifier[lId])
		{
			logger.debug(MString("Found external objtype: ") + depFn.name());
			return true;
		}
	}

	return false;
}

bool MayaScene::isLight(MObject obj)
{
	if( obj.hasFn(MFn::kLight))
		return true;

	MFnDependencyNode depFn(obj);
	uint nodeId = depFn.typeId().id();
	for( uint lId = 0; lId < this->lightIdentifier.size(); lId++)
	{
		if( nodeId == this->lightIdentifier[lId])
		{
			logger.debug(MString("Found external lighttype: ") + depFn.name());
			return true;
		}
	}
	return false;
}

bool MayaScene::isCamera(MObject obj)
{
	if( obj.hasFn(MFn::kCamera))
	{
		return true;
	}
	return false;
}

bool MayaScene::isCameraRenderable(MObject obj)
{
	MFnDependencyNode camFn(obj);
	bool renderable = true;
	getBool(MString("renderable"), camFn, renderable);
	if( renderable )
		return true;
	return false;
}


void  MayaScene::classifyMayaObject(MayaObject *obj)
{
	if( this->isCamera(obj->mobject))
	{
		this->camList.push_back(obj);
		return;
	}

	if( this->isLight(obj->mobject))
	{
		this->lightList.push_back(obj);
		return;
	}

	if( obj->mobject.hasFn(MFn::kInstancer))
	{
		instancerDagPathList.push_back(obj->dagPath);
		return;
	}

	this->objectList.push_back(obj);
}

//
//	Parse scene starting with "world" through all childs.
//	This allows an easy inheritance of attributes. e.g. you can give some element
//	in the hierarchy the colorAttribute "diffuseColor" and all objects below will inherit
//	this attribute until it is removed or overwritten by another value.
//

std::vector<MayaObject *> origObjects;

bool MayaScene::parseSceneHierarchy(MDagPath currentPath, int level, ObjectAttributes *parentAttributes, MayaObject *parentObject)
{
	logger.debug(MString("parse: ") + currentPath.fullPathName(), level);

	MayaObject *mo = mayaObjectCreator(currentPath);
	ObjectAttributes *currentAttributes = mo->getObjectAttributes(parentAttributes);
	mo->parent = parentObject;
	classifyMayaObject(mo);

	// 
	//	find the original mayaObject for instanced objects. Can be useful later.
	//

	if(currentPath.instanceNumber() == 0)
		origObjects.push_back(mo);
	else{
		MFnDagNode node(currentPath.node());
		for( size_t iId = 0; iId < origObjects.size(); iId++)
		{
			MFnDagNode onode(origObjects[iId]->mobject);
			if( onode.object() == node.object() )
			{
				logger.debug(MString("Orig Node found:") + onode.fullPathName(), level);
				mo->origObject = origObjects[iId];
				break;
			}
		}
	}

	uint numChilds = currentPath.childCount();
	for( uint chId = 0; chId < numChilds; chId++)
	{
		MDagPath childPath = currentPath;
		MStatus stat = childPath.push(currentPath.child(chId));
		if( !stat )
		{
			logger.debug(MString("Child path problem: parent: ") + currentPath.fullPathName() + " child id " + chId + " type " + currentPath.child(chId).apiTypeStr());
			continue;
		}
		MString childName = childPath.fullPathName();
		parseSceneHierarchy(childPath, level + 1, currentAttributes, mo);
	}

	return true;
}

bool MayaScene::parseScene()
{
	origObjects.clear();

	clearObjList(this->objectList);
	clearObjList(this->camList);
	clearObjList(this->lightList);

	MDagPath world = getWorld();
	if(parseSceneHierarchy(world, 0, NULL, NULL))
	{
		this->parseInstancerNew(); 
		this->getLightLinking();
		if(this->uiCamera.isValid() && (MGlobal::mayaState() != MGlobal::kBatch))
		{
			MayaObject *cam = NULL;
			for( uint camId = 0; camId < this->camList.size(); camId++)
			{
				if( this->camList[camId]->dagPath == this->uiCamera )
					cam = this->camList[camId];
			}
			if( cam == NULL )
			{
				logger.error(MString("UI Camera not found: ") + this->uiCamera.fullPathName());
				return false;
			}
			clearObjList(this->camList, cam);
		}
		this->good = true;

		return true;
	}
	return false;
}

// the camera from the UI is set via render command
void MayaScene::setCurrentCamera(MDagPath camDagPath)
{
	this->uiCamera = camDagPath;
}

bool MayaScene::updateScene(MFn::Type updateElement)
{
	logger.debug(MString("MayaScene::updateSceneNew."));

	for( int objId = 0; objId < this->objectList.size(); objId++)
	{
		MayaObject *obj = this->objectList[objId];
		
		if( !obj->mobject.hasFn(updateElement))
			continue;

		obj->updateObject();
		Logging::debug(MString("updateObj ") + objId + ": " + obj->dagPath.fullPathName());

		if (!obj->motionBlurred)
		{
			if (this->renderGlobals->currentMbElement.elementType == MbElement::None)
			{
				Logging::debug(MString("found non mb element type. Updating non mb objects.") + objId + ": " + obj->dagPath.fullPathName());
				if (updateElement == MFn::kShape)
					this->shapeUpdateCallback(obj);
				if (updateElement == MFn::kTransform)
				{
					obj->transformMatrices.clear();
					this->transformUpdateCallback(obj);
					obj->transformMatrices.push_back(obj->dagPath.inclusiveMatrix());
				}
			}
		}

		//if( !this->renderGlobals->isMbStartStep )
		// non mb objects will be exported only above
		if( !obj->motionBlurred )
				continue;

		if(this->renderGlobals->isDeformStep())
			if( obj->mobject.hasFn(MFn::kShape))
				this->shapeUpdateCallback(obj);

		if( this->renderGlobals->isTransformStep() )
		{
			if( this->renderGlobals->isMbStartStep )
				obj->transformMatrices.clear();

			obj->transformMatrices.push_back(obj->dagPath.inclusiveMatrix());
			if( obj->mobject.hasFn(MFn::kTransform))
				this->transformUpdateCallback(obj);
		}
	}


	return true;
}

bool MayaScene::updateScene()
{
	updateScene(MFn::kShape);
	updateScene(MFn::kTransform);

	for(size_t camId = 0; camId < this->camList.size(); camId++)
	{
		MayaObject *obj = this->camList[camId];
		obj->updateObject();

		if( !this->renderGlobals->isMbStartStep )
			if( !obj->motionBlurred )
				continue;
		
		if( this->renderGlobals->isTransformStep() )
		{
			if( this->renderGlobals->isMbStartStep )
				obj->transformMatrices.clear();
			obj->transformMatrices.push_back(obj->dagPath.inclusiveMatrix());
			this->transformUpdateCallback(obj);
		}
	}


	std::vector<MayaObject *>::iterator mIter;
	mIter = this->lightList.begin();
	for(;mIter!=this->lightList.end(); mIter++)
	{
		MayaObject *obj = *mIter;
		obj->updateObject();

		if( !this->renderGlobals->isMbStartStep )
			if( !obj->motionBlurred )
				continue;
		
		if( this->renderGlobals->isTransformStep() )
		{
			if( this->renderGlobals->isMbStartStep )
				obj->transformMatrices.clear();
			obj->transformMatrices.push_back(obj->dagPath.inclusiveMatrix());
			//this->transformUpdateCallback(obj);
			this->shapeUpdateCallback(obj);
		}
	}

	if( this->renderGlobals->isTransformStep() )
	{
		this->updateInstancer();
	}

	return true;
}


void MayaScene::clearObjList(std::vector<MayaObject *>& objList)
{
	size_t numElements = objList.size();
	for( size_t i = 0; i < numElements; i++)
		this->mayaObjectDeleter(objList[i]);
	objList.clear();
}

void MayaScene::clearObjList(std::vector<MayaObject *>& objList, MayaObject *notThisOne)
{
	size_t numElements = objList.size();

	for( size_t i = 0; i < numElements; i++)
	{
		if( objList[i] != notThisOne)
			this->mayaObjectDeleter(objList[i]);
	}
	objList.clear();
	objList.push_back(notThisOne);
	notThisOne->index = 0;
}

void MayaScene::clearInstancerNodeList()
{
	size_t numElements = this->instancerNodeElements.size();
	for( size_t i = 0; i < numElements; i++)
		this->mayaObjectDeleter(this->instancerNodeElements[i]);

	this->instancerNodeElements.clear();
}

bool MayaScene::updateInstancer()
{
	logger.debug("update instancer.");
	
	// updates only required for a transform step
	if( !this->renderGlobals->isTransformStep() )
		return true;

	size_t numElements = this->instancerNodeElements.size();
	for( size_t i = 0; i < numElements; i++)
	{
		MayaObject *obj =  this->instancerNodeElements[i];
		MMatrix origMatrix;
		origMatrix.setToIdentity();
		if( obj->origObject != NULL )
		{
			origMatrix = obj->origObject->dagPath.inclusiveMatrix();
			origMatrix.inverse();
		}
		MFnInstancer instFn(obj->instancerDagPath);
		MDagPathArray dagPathArray;
		MMatrix matrix;
		instFn.instancesForParticle(obj->instancerParticleId, dagPathArray, matrix); 
		for( uint k = 0; k < dagPathArray.length(); k++)
			logger.debug(MString("Particle mobj id: ") + i + "particle id: " + obj->instancerParticleId + " path id " + k + " - " + dagPathArray[k].fullPathName());
		if( this->renderGlobals->isMbStartStep )
			obj->transformMatrices.clear();

		obj->transformMatrices.push_back(origMatrix * matrix);
		this->transformUpdateCallback(obj);
		//this->shapeUpdateCallback(obj);
	}
	return true;
}

std::vector<MayaObject *> InstDoneList;
// parsing all particle instancer nodes in the scene
// here I put all nodes created by an instancer node into a array of its own.
// The reason why I don't simply add it to the main list is that I have to recreate them
// during sceneUpdate() for every frame because the list can change with the birth or death 
// of a particle
bool MayaScene::parseInstancerNew()
{
	MStatus stat;
	bool result = true;
	logger.debug(MString("parseInstancerNew"));
	MDagPath dagPath;
	size_t numInstancers = instancerDagPathList.size();
	size_t numobjects = this->objectList.size();

	// if we have any previously defined instancer elements, delete it
	this->clearInstancerNodeList();

	for(int iId = 0; iId < numInstancers; iId++)
	{
		MDagPath instPath = instancerDagPathList[iId];
		MObject instancerMObject = instPath.node();
		MString path = instPath.fullPathName();
		MFnInstancer instFn(instPath);
		int numParticles = instFn.particleCount();
		logger.debug(MString("Detected instancer. instPath: ") + path + " it has " + numParticles + " particle instances");
		MDagPathArray allPaths;
		MMatrixArray allMatrices;
		MIntArray pathIndices;
		MIntArray pathStartIndices;
		MObjectArray nodeList;
		getConnectedInNodes(MString("inputPoints"), instPath.node(), nodeList);
		bool hasParticleSystem = false;
		MVectorArray rgbPP;
		if( nodeList.length() > 0)
		{
			if( nodeList[0].hasFn(MFn::kParticle))
			{
				logger.debug(MString("Found a particle system called ") + getObjectName(nodeList[0]));
				MFnParticleSystem pSystem(nodeList[0], &stat);
				if( stat )
				{
					if( pSystem.hasRgb())
					{
						hasParticleSystem = true;
						pSystem.rgb(rgbPP);
					}
				}else{
					logger.debug(MString("Could nod get a particleSystem from node "));
				}
			}
		}
		//MFnParticleSystem
		
		// give me all instances in this instancer
		instFn.allInstances( allPaths, allMatrices, pathStartIndices, pathIndices );
	
		for( int p = 0; p < numParticles; p++ )
		{
			MMatrix particleMatrix = allMatrices[p];

			//  the number of paths instanced under a particle is computed by
			//  taking the difference between the starting path index for this
			//  particle and that of the next particle.  The size of the start
			//  index array is always one larger than the number of particles.
			//
			int numPaths = pathStartIndices[p+1]-pathStartIndices[p];
	        
			//  the values pathIndices[pathStart...pathStart+numPaths] give the
			//  indices in the allPaths array of the paths instanced under this
			//  particular particle.  Remember, different paths can be instanced
			//  under each particle.
			//
			int pathStart = pathStartIndices[p];

			//  loop through the instanced paths for this particle
			ObjectAttributes *currentAttributes = NULL;
			for( int i = pathStart; i < pathStart+numPaths; i++ )
			{
				int curPathIndex = pathIndices[i];
				MDagPath curPath = allPaths[curPathIndex];
				
				MayaObject *particleMObject =  this->mayaObjectCreator(curPath);
				MFnDependencyNode pOrigNode(particleMObject->mobject);
				MObject pOrigObject = pOrigNode.object();

				// search for the correct orig MayaObject element
				// TODO: visibiliy check - necessary?
				MayaObject *origObj = NULL;
				std::vector<MayaObject *>::iterator mIter = origObjects.begin();
				for( ;mIter != origObjects.end(); mIter++)
				{
					MayaObject *obj = *mIter;
					if( obj->mobject == pOrigObject)
					{
						origObj = obj;
					}
				}
				if( origObj == NULL)
				{
					logger.debug(MString("Orig particle instancer obj not found."));
					continue;
				}
				currentAttributes = particleMObject->getObjectAttributes(origObj->attributes);
				particleMObject->origObject = origObj;
				particleMObject->isInstancerObject = true;
				particleMObject->visible = true;
				particleMObject->instancerParticleId = p;
				particleMObject->instanceNumber = p;
				particleMObject->instancerDagPath = instPath;
				particleMObject->instancerMObj = instancerMObject;
				particleMObject->fullName = origObj->fullName + MString("_i_") + p;				
				particleMObject->shortName = origObj->shortName + MString("_i_") + p;
				this->instancerNodeElements.push_back(particleMObject);
				particleMObject->index = (int)(this->instancerNodeElements.size() - 1);
				currentAttributes->hasInstancerConnection = true;
				if( hasParticleSystem )
				{
					if( particleMObject->attributes != NULL )
					{
						particleMObject->attributes->hasColorOverride = true;
						particleMObject->attributes->colorOverride = MColor(rgbPP[p].x,rgbPP[p].y,rgbPP[p].z);
					}
				}
			}
		}
	}
	return true;
}


bool MayaScene::getShadingGroups()
{
	return true;
}

void MayaScene::getPasses()
{
	// clear
	//for( size_t renderPassId = 0; renderPassId < this->renderGlobals->renderPasses.size(); renderPassId++)
	//	delete this->renderGlobals->renderPasses[renderPassId];
	//this->renderGlobals->renderPasses.clear();

	//// add pass for lights for shadow maps 

	//// add pass for lights with photon maps

	//// finally do the normal beauty passes
	//RenderPass *rp = new RenderPass();
	//rp->evalFrequency = RenderPass::OncePerFrame;
	//rp->passType = RenderPass::Beauty;

	//int numcams = (int)this->camList.size();
	//for( int objId = 0; objId < numcams; objId++)
	//{
	//	MayaObject *obj = this->camList[objId];

	//	// the renderable check is moved here because it will disable
	//	// rendering from the UI with a normally not renderable camera.
	//	if( MGlobal::mayaState() == MGlobal::kBatch )
	//	{
	//		bool renderable = false;
	//		MFnDependencyNode camFn(obj->mobject);
	//		getBool("renderable", camFn, renderable);
	//		if( renderable )
	//		{
	//			rp->objectList.push_back(obj);
	//			logger.debug(MString("Adding camera ") + camFn.name() + " to render cam list.");
	//		}
	//	}else{
	//		rp->objectList.push_back(obj);
	//	}
	//}	
	//
	//this->renderGlobals->renderPasses.push_back(rp);
}


bool MayaScene::doFrameJobs()
{
	logger.debug("MayaScene::doFrameJobs()");
	logger.progress(MString("\n========== Start rendering of frame ") + this->currentFrame + " ==============\n");

	this->parseScene(); 
	this->renderGlobals->getMbSteps();

	if(this->renderGlobals->mbElementList.size() == 0)
	{
		logger.error(MString("no mb steps, somethings wrong."));
		return false;
	}

	int numMbSteps = (int)this->renderGlobals->mbElementList.size();
	
	for( uint camId = 0; camId < this->camList.size(); camId++)
	{
		MayaObject *camera = this->camList[camId];
		if( !this->isCameraRenderable(camera->mobject) && (!(camera->dagPath == this->uiCamera)))
		{	
			logger.debug(MString("Camera ") + camera->shortName + " is not renderable, skipping.");
			continue;
		}
		
		logger.info(MString("Rendering camera ") + camera->shortName);

		for( int mbStepId = 0; mbStepId < numMbSteps; mbStepId++)
		{
			this->renderGlobals->currentMbStep = mbStepId;
			this->renderGlobals->isMbStartStep = this->renderGlobals->mbElementList[mbStepId].time == this->renderGlobals->mbElementList[0].time;
			this->renderGlobals->currentMbElement = this->renderGlobals->mbElementList[mbStepId];
			this->renderGlobals->currentFrameNumber = (float)(this->renderGlobals->currentFrame + this->renderGlobals->mbElementList[mbStepId].time);
			bool needView = true;
			if( mbStepId > 0)
			{
				if( this->renderGlobals->mbElementList[mbStepId].time == this->renderGlobals->mbElementList[mbStepId-1].time)
				{
					needView = false;
				}
			}

			if( needView )
			{
				logger.debug(MString("doFrameJobs() viewFrame: ") + this->renderGlobals->currentFrameNumber);
				if( MGlobal::mayaState() != MGlobal::kBatch )
				{
					MRenderView::setCurrentCamera( camera->dagPath );
				}
				MGlobal::viewFrame(this->renderGlobals->currentFrameNumber);
			}

			// TODO: dynamic runup necessary?
			this->updateScene();
			logger.info(MString("update scene done"));
			this->renderGlobals->currentMbStep++;
		}

		if( MGlobal::mayaState() != MGlobal::kBatch )
			MGlobal::viewFrame(this->renderGlobals->currentFrame);

		this->renderImage();
		
		EventQueue::Event e;
		e.type = EventQueue::Event::FRAMEDONE;
		theRenderEventQueue()->push(e);

		logger.setOutType(Logging::OutputWindow);
	}

	return true;
}



void MayaScene::theRenderThread( MayaScene *mayaScene)
{
	logger.debug(MString("theRenderThread: renderScene"));
	mayaScene->renderScene();

	EventQueue::Event e;
	e.type = EventQueue::Event::RENDERDONE;
	theRenderEventQueue()->push(e);
}

void MayaScene::startRenderThread()
{
	logger.debug(MString("startRenderThread: startThread"));
	rendererThread = boost::thread(MayaScene::theRenderThread, this);
	logger.debug(MString("startRenderThread: startThread detach"));
	rendererThread.detach();
}

bool MayaScene::renderScene()
{
	logger.info("MayaScene::renderScene()");
	getWorldPtr()->state = MayaToWorld::UIRENDER;
	this->getPasses();

	if( this->renderType == IPR )
		if(!this->canDoIPR())
		{
			logger.warning("Sorry this renderer cannot render in IPR mode.");
			return false;
		}

	if(!this->doPreRenderJobs())
	{
		logger.error("doPreRenderJobs failed.");
		return false;
	}

	int numFrames = (int)this->renderGlobals->frameList.size();
	for( int frameNr = 0; frameNr < numFrames; frameNr++)
	{
		this->currentFrame = this->renderGlobals->frameList[frameNr];
		this->renderGlobals->currentFrame = this->currentFrame;
		if(!this->doPreFrameJobs())
		{
			logger.error("doPreFrameJobs failed.");
			return false;
		}
		if(!this->doFrameJobs())
		{
			logger.error("doFrameJobs failed.");
			return false;
		}

		if(!this->doPostFrameJobs())
		{
			logger.error("doPostFrameJobs failed.");
			return false;
		}
		logger.progress(MString("\n========== Rendering of frame ") + this->currentFrame + " done ==============\n");
	}

	if(!this->doPostRenderJobs())
	{
		logger.error("doPostRenderJobs failed.");
		return false;
	}

	getWorldPtr()->state = MayaToWorld::NONE;
	return true;
}

MayaObject *MayaScene::getObject(MObject obj)
{
	MayaObject *mo = NULL;
	size_t numobjects = this->objectList.size();
	for( size_t objId = 0; objId < numobjects; objId++)
	{
		if( this->objectList[objId]->mobject == obj)
			return this->objectList[objId];
	}
	return mo;
}

MayaObject *MayaScene::getObject(MDagPath dp)
{
	MayaObject *mo = NULL;
	size_t numobjects = this->objectList.size();
	for( size_t objId = 0; objId < numobjects; objId++)
	{
		if( this->objectList[objId]->dagPath == dp)
			return this->objectList[objId];
	}
	return mo;
}

void MayaScene::waitForFrameCompletion()
{
	while( 1 )
	{
		if( this->renderState == RenderState::FRAMEDONE )
			break;
		if( this->renderState == RenderState::RENDERERROR )
			break;
		boost::this_thread::sleep(boost::posix_time::milliseconds(500));
	}
}