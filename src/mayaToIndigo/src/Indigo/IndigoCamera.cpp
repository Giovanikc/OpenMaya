#include "Indigo.h"
#include "utilities/logging.h"
#include "../mtin_common/mtin_renderGlobals.h"
#include "../mtin_common/mtin_mayaScene.h"
#include "../mtin_common/mtin_mayaObject.h"

#include <maya/MFnCamera.h>

static Logging logger;

void IndigoRenderer::defineCamera()
{
	mtin_MayaObject *icam = (mtin_MayaObject *)this->mtin_scene->camList[0];
	MFnCamera camFn(icam->dagPath);
	MVector camUp = camFn.upDirection(MSpace::kWorld);
	MVector camView = camFn.viewDirection(MSpace::kWorld);
	MPoint camPos = camFn.eyePoint(MSpace::kWorld);

	//==================== Create camera node =========================
	Indigo::SceneNodeCameraRef cam(new Indigo::SceneNodeCamera());
	cam->lens_radius = 0.0001;
	cam->autofocus = false;
	cam->exposure_duration = 1.0 / 30.0;
	cam->focus_distance = 1.0;
	cam->lens_sensor_dist = 0.03;
	cam->lens_shift_right_distance = 0;
	cam->lens_shift_up_distance = 0;
	cam->sensor_width = 0.035;

	cam->forwards = Indigo::Vec3d(camView.x, camView.y, camView.z);
	cam->up = Indigo::Vec3d(camUp.x, camUp.y, camUp.z);
	cam->setPos(Indigo::Vec3d(camPos.x, camPos.y, camPos.z));

	sceneRootRef->addChildNode(cam);

}