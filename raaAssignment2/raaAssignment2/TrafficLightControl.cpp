#pragma once

#include "TrafficLightControl.h"
#include <iostream>

#include "findNodeVisitor.h"

TrafficLightControl::TrafficLightControl(osg::Node* pPart, osg::Vec3 vTrans, float fRot, float fScale) : raaNodeCallbackFacarde(pPart, vTrans, fRot, fScale)
{
	timeCount = 0;
	trafficIndex = 0;
	isSwitchRequired = false;
	timeCountThreshold = 300;
	raaTrafficSystem::addTarget(this);
}

TrafficLightControl::~TrafficLightControl()
{
	raaTrafficSystem::removeTarget(this);
}

void TrafficLightControl::operator() (osg::Node* node, osg::NodeVisitor* nv)
{

	if (timeCount == timeCountThreshold)
	{

		if (isSwitchRequired) {
			std::list<TrafficLightFacarde*>::iterator it = m_lTrafficLights.begin();
			std::advance(it, trafficIndex);
			changeTrafficLight(*it);
			isSwitchRequired = false;
			trafficIndex = (trafficIndex + 1) % m_lTrafficLights.size();
		}

		std::list<TrafficLightFacarde*>::iterator it = m_lTrafficLights.begin();
		std::advance(it, trafficIndex);
		changeTrafficLight(*it);


		timeCount = 0;
	}

	timeCount++;
}

void TrafficLightControl::switchTo(TrafficLightFacarde* trafficLight) {

	while(true) {
		isSwitchRequired = false;
		std::list<TrafficLightFacarde*>::iterator it = m_lTrafficLights.begin();
		std::advance(it, trafficIndex);

		TrafficLightFacarde* trafficLight2 = (TrafficLightFacarde*)*it;
		if (trafficLight == trafficLight2) {

			trafficLight2->setGreenTrafficLight();
			timeCount = 0;
			timeCountThreshold = 600;
			break;
		}
		else {
			trafficLight2->setRedTrafficLight();
		}

		trafficIndex = (trafficIndex + 1) % m_lTrafficLights.size();

	}

}

void TrafficLightControl::changeTrafficLight(TrafficLightFacarde* pTrafficLight)
{
	pTrafficLight->m_iTrafficLightStatus++;


	if (pTrafficLight->m_iTrafficLightStatus > Colour::AMBER) {
		pTrafficLight->m_iTrafficLightStatus = Colour::RED;
	}

	switch (pTrafficLight->m_iTrafficLightStatus) {
	case		Colour::RED:	pTrafficLight->setRedTrafficLight();	break;
	case		Colour::RED_AND_AMBER:	pTrafficLight->setRedAndAmberTrafficLight();	break;
	case		Colour::GREEN:	pTrafficLight->setGreenTrafficLight(); timeCountThreshold = 600;	break;
	case		Colour::AMBER:	pTrafficLight->setAmberTrafficLight(); isSwitchRequired = true; timeCountThreshold = 300;	break;
	default:															break;
	}

}

void TrafficLightControl::addTrafficLight(TrafficLightFacarde* pTrafficLight)
{
	m_lTrafficLights.push_back(pTrafficLight);
	pTrafficLight->setController(this);
}


