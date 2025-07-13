
#include <windows.h>
#include <osg/Group>
#include <osg/Geometry>
#include <osg/Geode>
#include <osg/ShapeDrawable>
#include <osg/Shape>
#include <osg/Material>
#include <osg/Switch>

#include "raaTrafficSystem.h"
#include "raaCarFacarde.h"
#include <iostream>
#include "TrafficLightFacarde.h"

#include <cmath>


double radiansToDegrees(double radians) {
    double PI = 4 * std::atan(1);  
    return radians * (180.0 / PI);
}

double getDistance(osg::Vec3f a, osg::Vec3f b) {
    osg::Vec3f difference = a - b;
    return sqrt( pow(difference[0], 2) + pow(difference[1], 2) + pow(difference[2], 2) );

}

raaCarFacarde::raaCarFacarde(osg::Node* pWorldRoot, osg::Node* pPart, osg::AnimationPath* ap, double dSpeed): raaAnimatedFacarde(pPart, ap, dSpeed)
{
	raaTrafficSystem::addTarget(this); // adds the car to the traffic system (static class) which holds a reord of all the dynamic parts in the system
    this->maxSpeed = dSpeed;
    this->slowSpeed = 50.0;

}

raaCarFacarde::~raaCarFacarde()
{
	raaTrafficSystem::removeTarget(this); // removes the car from the traffic system (static class) which holds a reord of all the dynamic parts in the system
}

void raaCarFacarde::operator()(osg::Node* node, osg::NodeVisitor* nv)

{
    bool isNearTrafficLight = false;
    bool isNearOtherCar = false;
    bool canIgnoreTraffic = false;
    bool isBehindACar = false;

    TrafficLightFacarde* nearestTrafficLight = nullptr;
    double nearestCarSpeed = 0.0;

    for (auto it = raaTrafficSystem::colliders().begin(); it != raaTrafficSystem::colliders().end(); it++)
    {

        auto collider = *it;

        if (collider == this) //cannot collide with itself

        {
            continue;
        }

        auto colliderPos = collider->getWorldDetectionPoint();

        auto thisPos = this->getWorldDetectionPoint();


        double distance = getDistance(colliderPos, thisPos);
        
        
        if (distance <= 175) {

            raaCarFacarde* thisCar = (raaCarFacarde*) this;
            raaCarFacarde* colliderCar = (raaCarFacarde*) collider;
            int rotationDifference = thisCar->getRotation() - colliderCar->getRotation();


            if (std::abs(rotationDifference) <= 75) {
                isNearOtherCar = true;
                nearestCarSpeed = colliderCar->m_dSpeed;

            }
            
        }

    }



    //Distance between Car and Traffic light

    double carRotation = this->getRotation();

    for (auto it = raaTrafficSystem::trafficLights().begin(); it != raaTrafficSystem::trafficLights().end(); it++) //traffic light logic

    {

        TrafficLightFacarde* trafficLight = *it;
        osg::Vec3f trafficLightLocation = trafficLight->root()->getBound().center();
        osg::Vec3f thisPos = this->getWorldDetectionPoint();

        double distance = getDistance(thisPos, trafficLightLocation);


        osg::MatrixTransform* tMatTransform = trafficLight->rotation();
        osg::Matrix tMat = tMatTransform->getMatrix();
        osg::Quat tRotation = tMat.getRotate();
        double tAngleRadians;
        osg::Vec3 tAxis;

        tRotation.getRotate(tAngleRadians, tAxis);

        double trafficRotation = round(radiansToDegrees(tAngleRadians));

        if (trafficRotation == 0) { trafficRotation = 360; }

        int rotationDifference = trafficRotation - carRotation;


        //checks if the car is facing toward the traffic light
        if(std::abs(rotationDifference) == 90 || std::abs(rotationDifference) == 270) {


            if (distance <= 275.0) {
                isNearTrafficLight = true;
                nearestTrafficLight = trafficLight;

                
                int lookRotationDifference = this->GetRotationToLookAt(trafficLightLocation) - carRotation;

                //Checks if the car went through the traffic light before it turned from green to yellow
                if (lookRotationDifference <= -135 && lookRotationDifference >= -235) {
                    canIgnoreTraffic = true;
                }

                if (lookRotationDifference <= 45 && lookRotationDifference >= -50) {
                    canIgnoreTraffic = true;
                }

                if (lookRotationDifference <= -310 && lookRotationDifference >= -359) {
                    canIgnoreTraffic = true;
                }

                if (lookRotationDifference <= 0 && lookRotationDifference >= -55) {
                    canIgnoreTraffic = true;
                }


            }
        }


    }

    if (!this->isFrozen) {

        if (isNearTrafficLight) {

            if (nearestTrafficLight->m_iTrafficLightStatus == 3) {

                if (isNearOtherCar) {
                    this->m_dSpeed = this->slowSpeed;
                }
                else {
                    this->m_dSpeed = this->maxSpeed;
                }

            }
            else {

                if (canIgnoreTraffic) {
                    this->m_dSpeed = this->maxSpeed;
                }
                else {
                    this->m_dSpeed = 0;
                }
                

            }

        }
        else if (isNearOtherCar) {
            this->m_dSpeed = nearestCarSpeed;
        }
        else {
            this->m_dSpeed = this->maxSpeed;
        }

    }
    else {
        this->m_dSpeed = 0;
    }


    raaAnimationPathCallback::operator()(node, nv);

}

osg::Vec3f raaCarFacarde::getWorldDetectionPoint()
{

	return this->root()->getBound().center(); // should return the world position of the detection point for this subtree
}

osg::Vec3f raaCarFacarde::getWorldCollisionPoint()
{


	return osg::Vec3(); // should return the world position of the collision point for this subtree
}

int raaCarFacarde::getRotation() {

    osg::MatrixTransform* matTransform = this->getTransform();
    osg::Matrix mat = matTransform->getMatrix();
    osg::Quat rotation = mat.getRotate();

    double angleRadians;
    osg::Vec3 axis;

    rotation.getRotate(angleRadians, axis);

    int carRotation = round(radiansToDegrees(angleRadians));

    

    if (axis[2] == -1) {
        carRotation = 360 - carRotation;
    }

    return carRotation;

}

void raaCarFacarde::toggleFreezeState() {
    this->isFrozen = !this->isFrozen;

    if (this->isFrozen) {
        std::cout << "currently frozen" << "\n";
    }
}

double raaCarFacarde::GetRotationToLookAt(osg::Vec3f target) {
    auto thisPos = this->getWorldDetectionPoint();
    osg::Vec3f lookDirection = target - thisPos;

    lookDirection.normalize();
    double rotation = radiansToDegrees(atan2(lookDirection.x(), lookDirection.y()));
   
    return rotation;
    
}











