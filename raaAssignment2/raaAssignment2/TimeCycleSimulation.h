#pragma once

#include <osg/Group>
//#include <osg/StateSet>
//#include <osg/Camera>
//#include <osg/Vec4>
//#include <osgViewer/Viewer>
//#include <osg/NodeCallback>
//#include <osg/Timer>

class TimeCycleSimulation : public osg::NodeCallback {

public:

	TimeCycleSimulation(float timeOfDay, float dayDuration);


protected:
	float timeOfDay;
	float dayDuration;

	

};
