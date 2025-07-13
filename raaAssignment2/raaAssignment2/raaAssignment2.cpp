#include <windows.h>
#include <iostream>
#include <math.h>
#include <osgViewer/Viewer>
#include <osgGA/TrackballManipulator>
#include <osgGA/FlightManipulator>
#include <osgGA/DriveManipulator>
#include <osgGA/KeySwitchMatrixManipulator>
#include <osgGA/StateSetManipulator>
#include <osgViewer/ViewerEventHandlers>
#include <osg/AnimationPath>
#include <osg/Matrix>
#include <osg/Material>
#include <osg/ShapeDrawable>
#include "raaInputController.h"
#include "raaAssetLibrary.h"
#include "raaFacarde.h"
#include "raaSwitchActivator.h"
#include "raaRoadTileFacarde.h"
#include "raaAnimationPointFinder.h"
#include "raaAnimatedFacarde.h"
#include "raaCarFacarde.h"
#include "raaTrafficSystem.h"
#include "TrafficLightFacarde.h"
#include "TrafficLightControl.h"
#include "findNodeVisitor.h"

//A custom simulation for time utilizing OSG time system
class TimeCycleSimulation : public osg::NodeCallback {
	public:
		TimeCycleSimulation(osg::Camera* camera) {
			dayDuration = 45.0;
			this->camera = camera;
			isTransitioningFromNight = false;
			isSimulationPaused = false;
		}

		virtual void operator()(osg::Node* node, osg::NodeVisitor* nv) override {
			double newTime = nv->getFrameStamp()->getSimulationTime();
			
			if (!isSimulationPaused) {
				double delta = newTime - lastTimeUpdated;
				update(delta);
			}

			lastTimeUpdated = newTime;

			traverse(node, nv);
		}

		void update(double deltaTime) {

			if (!isTransitioningFromNight) {
				timeOfDay += deltaTime * simulationTimeMultiplier;
			}
			else {
				timeOfDay -= deltaTime * simulationTimeMultiplier;
			}

			if (timeOfDay > dayDuration) {
				isTransitioningFromNight = true;
			}

			if (timeOfDay < 0) {
				isTransitioningFromNight = false;
			}
			

			float progress = timeOfDay / dayDuration;

			osg::Vec4 dayColor(0.5f, 0.7f, 1.0f, 1.0f); //light blue
			osg::Vec4 nightColor(0.0f, 0.0f, 0.05f, 1.0f); //dark blue

			osg::Vec4 currentColor = dayColor * (1.0f - progress) + nightColor * progress; //using lerp formula

			camera->setClearColor(currentColor);
		}

	protected:
		
		double timeOfDay;
		double dayDuration;
		double lastTimeUpdated;
		bool isTransitioningFromNight;
		bool isSimulationPaused;
		float simulationTimeMultiplier = 1.0;
		osg::Camera* camera;

};
  
findNodeVisitor::findNodeVisitor() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN),
searchForName(), isFound(false)
{
}

  
findNodeVisitor::findNodeVisitor(const std::string& searchName) :
	osg::NodeVisitor(TRAVERSE_ALL_CHILDREN),
	searchForName(searchName), isFound(false)
{
}


void findNodeVisitor::apply(osg::Node& searchNode)
{

	if (searchNode.getName().compare(searchForName) == 0)
	{
		//std::cout << "Name: " << searchNode.getName() << "\n";
		isFound = true;
		foundNodeList.push_back(&searchNode);
	}


}

void findNodeVisitor::setNameToFind(const std::string& searchName)
{
	searchForName = searchName;
	isFound = false;
	foundNodeList.clear();
	
}

void findNodeVisitor::apply(osg::Transform& searchNode)
{
	osgSim::DOFTransform* dofNode =
		dynamic_cast<osgSim::DOFTransform*> (&searchNode);
	if (dofNode)
	{
		dofNode->setAnimationOn(false);
	}
	apply((osg::Node&)searchNode);
	traverse(searchNode);
}
osg::Node* findNodeVisitor::getFirst()
{
	return *(foundNodeList.begin());
}

//When a car has been clicked on in the scene
void onCarPicked(raaCarFacarde* car) {
	car->toggleFreezeState();
	
}

//When a traffic light has been clicked on in the scene
void onTrafficLightPicked(TrafficLightFacarde* trafficLight) {
	trafficLight->getController()->switchTo(trafficLight);
}


void onNodePicked(std::string nodeName) {


	//Fires the relevant event depending on what type of node it is (a car or a traffic light)
	for (auto it = raaTrafficSystem::colliders().begin(); it != raaTrafficSystem::colliders().end(); it++)
	{
		raaCarFacarde* car = (raaCarFacarde*) *it;
		findNodeVisitor nv(nodeName);
		car->root()->accept(nv);

		if (nv.isNodeFound()) {
			onCarPicked(car);
			break;
		}
	}

	for (auto it = raaTrafficSystem::trafficLights().begin(); it != raaTrafficSystem::trafficLights().end(); it++)
	{
		TrafficLightFacarde* trafficLight = (TrafficLightFacarde*)*it;

		findNodeVisitor nv(nodeName);
		trafficLight->root()->accept(nv);

		if (nv.isNodeFound()) {
			onTrafficLightPicked(trafficLight);
			break;
		}
	}

	//std::cout << "------------------------------------------------------" << "\n";

}


/*For interation with dynamic entities within the scnee*/

class PickHandler : public osgGA::GUIEventHandler
{
public:

	PickHandler() : _mX(0.), _mY(0.){}
	bool handle(const osgGA::GUIEventAdapter& ea,
		osgGA::GUIActionAdapter& aa)
	{
		osgViewer::Viewer* viewer =
			dynamic_cast<osgViewer::Viewer*>(&aa);
		if (!viewer)
			return(false);

		switch (ea.getEventType())
		{
		case osgGA::GUIEventAdapter::PUSH:
		case osgGA::GUIEventAdapter::MOVE:
		{

			_mX = ea.getX();
			_mY = ea.getY();
			return(false);
		}
		case osgGA::GUIEventAdapter::RELEASE:
		{

			if (_mX == ea.getX() && _mY == ea.getY())
			{
				if (pick(ea.getXnormalized(),
					ea.getYnormalized(), viewer))
					return(true);
			}
			return(false);
		}

		default:
			return(false);
		}
	}

protected:
	// Store mouse xy location for button press & move events.
	float _mX, _mY;

	osg::Node* child;

	// Perform a pick operation.
	bool pick(const double x, const double y,
		osgViewer::Viewer* viewer)
	{
		

		if (!viewer->getSceneData())
			return(false);


		osgUtil::LineSegmentIntersector* picker =
			new osgUtil::LineSegmentIntersector(osgUtil::Intersector::PROJECTION, x, y);

		osgUtil::IntersectionVisitor iv(picker);


		viewer->getCamera()->accept(iv);

		if (picker->containsIntersections())
		{
			osgUtil::LineSegmentIntersector::Intersections a = picker->getIntersections();


			const osg::NodePath& nodePath =
				picker->getFirstIntersection().nodePath;

			unsigned int idx = nodePath.size();

			while (idx--)
			{
				
				osg::Object* obj =
					dynamic_cast<osg::Object*>(
						nodePath[idx]);

				

				if (obj->getName() == "Translation") {
					onNodePicked(nodePath[idx - 1]->getName());
					break;
				}
				

			}



		}
	}

};



typedef std::vector<raaAnimationPointFinder>raaAnimationPointFinders;
osg::Group* g_pRoot = 0; // root of the sg
float g_fTileSize = 472.441f; // width/depth of the standard road tiles
std::string g_sDataPath = "../../Data/";

enum raaRoadTileType
{
	Normal,
	LitTJunction,
	LitXJunction,
};

void addRoadTile(std::string sAssetName, std::string sPartName, int xUnit, int yUnit, float fRot, osg::Group* pParent)
{
	raaFacarde* pFacarde = new raaRoadTileFacarde(raaAssetLibrary::getNamedAsset(sAssetName, sPartName), osg::Vec3(g_fTileSize * xUnit, g_fTileSize * yUnit, 0.0f), fRot);
	pParent->addChild(pFacarde->root());
}

TrafficLightControl* addTrafficControl(std::string sAssetName, std::string sPartName, int xUnit, int yUnit, float fRot, osg::Group* pParent)
{
	TrafficLightControl* tc = new TrafficLightControl(raaAssetLibrary::getNamedAsset(sAssetName, sPartName), osg::Vec3(g_fTileSize * xUnit, g_fTileSize * yUnit, 0.0f), fRot, 1.0f);
	pParent->addChild(tc->root());

	return tc;
	
}

TrafficLightFacarde* addTrafficLight(std::string sPartName, int xUnit, int yUnit, float fRot, osg::Group* pParent) {
	TrafficLightFacarde* tlFacarde = new TrafficLightFacarde(raaAssetLibrary::getClonedAsset("trafficLight", sPartName), osg::Vec3(xUnit, yUnit, 0.0f), fRot, 0.08f);
	pParent->addChild(tlFacarde->root());

	return tlFacarde;
}

osg::Node* buildAnimatedVehicleAsset()
{
	osg::Group* pGroup = new osg::Group();

	osg::Geode* pGB = new osg::Geode();
	osg::ShapeDrawable* pGeomB = new osg::ShapeDrawable(new osg::Box(osg::Vec3(0.0f, 0.0f, 0.0f), 100.0f, 60.0f, 40.0f));
	osg::Material* pMat = new osg::Material();
	pMat->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.3f, 0.3f, 0.1f, 1.0f));
	pMat->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0.8f, 0.8f, 0.3f, 1.0f));
	pMat->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(1.0f, 1.0f, 0.6f, 1.0f));

	pGroup->addChild(pGB);
	pGB->addDrawable(pGeomB);

	pGB->getOrCreateStateSet()->setAttribute(pMat, osg::StateAttribute::ON || osg::StateAttribute::OVERRIDE);
	pGB->getOrCreateStateSet()->setAttributeAndModes(new osg::PolygonMode(osg::PolygonMode::FRONT_AND_BACK, osg::PolygonMode::LINE), osg::StateAttribute::ON || osg::StateAttribute::OVERRIDE);

	return pGroup;
}



osg::AnimationPath* createAnimationPath(raaAnimationPointFinders apfs, osg::Group* pRoadGroup)
{
	float fAnimTime = 0.0f;
	osg::AnimationPath* ap = new osg::AnimationPath();

	for (int i = 0; i < apfs.size(); i++)
	{
		float fDistance = 0.0f;
		osg::Vec3 vs;
		osg::Vec3 ve;

		vs.set(apfs[i].translation().x(), apfs[i].translation().y(), apfs[i].translation().z());

		if (i == apfs.size() - 1)
			ve.set(apfs[0].translation().x(), apfs[0].translation().y(), apfs[0].translation().z());
		else
			ve.set(apfs[i + 1].translation().x(), apfs[i + 1].translation().y(), apfs[i + 1].translation().z());

		float fXSqr = pow((ve.x() - vs.x()), 2);
		float fYSqr = pow((ve.y() - vs.y()), 2);
		float fZSqr = pow((ve.z() - vs.z()), 2);

		fDistance = sqrt(fXSqr + fYSqr);
		ap->insert(fAnimTime, osg::AnimationPath::ControlPoint(apfs[i].translation(), apfs[i].rotation()));
		fAnimTime += (fDistance / 10.0f);
	}

	return ap;
}

void buildRoad(osg::Group* pRoadGroup, osg::Group* trafficLightGroup)
{
	addRoadTile("roadStraight", "straight1", 0, 0, 0.0f, pRoadGroup);

	TrafficLightControl* tc1 = addTrafficControl("roadTJunction", "TJunction1", 1, 0, 270.0f, pRoadGroup);
	TrafficLightFacarde* tl0 = addTrafficLight("trafficLight0", 650, -175, 90, trafficLightGroup);
	TrafficLightFacarde* tl1 = addTrafficLight("trafficLight1", 650, 175, 180, trafficLightGroup);
	TrafficLightFacarde* tl2 = addTrafficLight("trafficLight2", 300, 175, 270, trafficLightGroup);
	tc1->addTrafficLight(tl0);
	tc1->addTrafficLight(tl1);
	tc1->addTrafficLight(tl2);



	addRoadTile("roadStraight", "straight2", 2, 0, 0.0f, pRoadGroup);
	addRoadTile("roadCurve", "curve1", 3, 0, 90.0f, pRoadGroup);
	addRoadTile("roadStraight", "straight3", 3, 1, 90.0f, pRoadGroup);


	TrafficLightControl* tc2 = addTrafficControl("roadTJunction", "TJunction2", 3, 2, 0.0f, pRoadGroup);
	TrafficLightFacarde* tl3 = addTrafficLight("trafficLight3", 1250, 1125, 270, trafficLightGroup);
	TrafficLightFacarde* tl4 = addTrafficLight("trafficLight4", 1250, 775, 0, trafficLightGroup);
	TrafficLightFacarde* tl5 = addTrafficLight("trafficLight5", 1600, 1125, 180, trafficLightGroup);
	tc2->addTrafficLight(tl3);
	tc2->addTrafficLight(tl4);
	tc2->addTrafficLight(tl5);


	addRoadTile("roadStraight", "straight4", 3, 3, 90.0f, pRoadGroup);
	addRoadTile("roadCurve", "curve2", 3, 4, 180.0f, pRoadGroup);
	addRoadTile("roadStraight", "straight5", 2, 4, 180.0f, pRoadGroup);

	TrafficLightControl* tc3 = addTrafficControl("roadTJunction", "TJunction3", 1, 4, 90.0f, pRoadGroup);
	TrafficLightFacarde* tl6 = addTrafficLight("trafficLight6", 300, 1725, 0, trafficLightGroup);
	TrafficLightFacarde* tl7 = addTrafficLight("trafficLight7", 300, 2075, 270, trafficLightGroup);
	TrafficLightFacarde* tl8 = addTrafficLight("trafficLight8", 650, 1725, 90, trafficLightGroup);
	tc3->addTrafficLight(tl6);
	tc3->addTrafficLight(tl7);
	tc3->addTrafficLight(tl8);

	addRoadTile("roadStraight", "straight6", 0, 4, 180.0f, pRoadGroup);
	addRoadTile("roadCurve", "curve3", -1, 4, 270.0f, pRoadGroup);
	addRoadTile("roadStraight", "straight7", -1, 3, 270.0f, pRoadGroup);


	TrafficLightControl* tc4 = addTrafficControl("roadTJunction", "TJunction4", -1, 2, 180.0f, pRoadGroup);
	TrafficLightFacarde* tl9 = addTrafficLight("trafficLight9", -300, 775, 90, trafficLightGroup);
	TrafficLightFacarde* tl10 = addTrafficLight("trafficLight10", -300, 1125, 180, trafficLightGroup);
	TrafficLightFacarde* tl11 = addTrafficLight("trafficLight11", -650, 775, 0, trafficLightGroup);
	tc4->addTrafficLight(tl9);
	tc4->addTrafficLight(tl10);
	tc4->addTrafficLight(tl11);


	addRoadTile("roadStraight", "straight8", -1, 1, 270.0f, pRoadGroup);
	addRoadTile("roadCurve", "curve4", -1, 0, 0.0f, pRoadGroup);
	addRoadTile("roadStraight", "innerStraight1", 1, 1, 90.0f, pRoadGroup);
	addRoadTile("roadStraight", "innerStraight2", 2, 2, 0.0f, pRoadGroup);
	addRoadTile("roadStraight", "innerStraight3", 1, 3, 90.0f, pRoadGroup);
	addRoadTile("roadStraight", "innerStraight4", 0, 2, 0.0f, pRoadGroup);


	TrafficLightControl* tc5 = addTrafficControl("roadXJunction", "XJunction1", 1, 2, 0.0f, pRoadGroup);
	TrafficLightFacarde* tl12 = addTrafficLight("trafficLight12", 650, 1125, 180, trafficLightGroup);
	TrafficLightFacarde* tl13 = addTrafficLight("trafficLight13", 300, 1125, 270, trafficLightGroup);
	TrafficLightFacarde* tl14 = addTrafficLight("trafficLight14", 650, 775, 90, trafficLightGroup);
	TrafficLightFacarde* tl15 = addTrafficLight("trafficLight15", 300, 775, 0, trafficLightGroup);
	tc5->addTrafficLight(tl12);
	tc5->addTrafficLight(tl13);
	tc5->addTrafficLight(tl14);
	tc5->addTrafficLight(tl15);

}


void createCarOne(osg::Group* pRoadGroup)
{
	raaAnimationPointFinders apfs;
	osg::AnimationPath* ap = new osg::AnimationPath();

	apfs.push_back(raaAnimationPointFinder("straight1", 1, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("straight1", 3, pRoadGroup));

	apfs.push_back(raaAnimationPointFinder("curve4", 3, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("curve4", 4, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("curve4", 5, pRoadGroup));

	apfs.push_back(raaAnimationPointFinder("straight8", 1, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("straight8", 3, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("TJunction4", 4, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("TJunction4", 4, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("straight7", 1, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("straight7", 3, pRoadGroup));

	apfs.push_back(raaAnimationPointFinder("curve3", 3, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("curve3", 4, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("curve3", 5, pRoadGroup));

	apfs.push_back(raaAnimationPointFinder("straight6", 1, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("straight6", 3, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("TJunction3", 4, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("TJunction3", 4, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("straight5", 1, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("straight5", 3, pRoadGroup));

	apfs.push_back(raaAnimationPointFinder("curve2", 3, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("curve2", 4, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("curve2", 5, pRoadGroup));

	apfs.push_back(raaAnimationPointFinder("straight4", 1, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("straight4", 3, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("TJunction2", 4, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("TJunction2", 4, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("straight3", 1, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("straight3", 3, pRoadGroup));

	apfs.push_back(raaAnimationPointFinder("curve1", 3, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("curve1", 4, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("curve1", 5, pRoadGroup));

	apfs.push_back(raaAnimationPointFinder("straight2", 1, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("straight2", 3, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("TJunction1", 4, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("TJunction1", 4, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("straight1", 1, pRoadGroup));



	ap = createAnimationPath(apfs, pRoadGroup);

	osg::ref_ptr<osg::MatrixTransform> matrixTransform{ new osg::MatrixTransform };

	osg::Matrix rotate{ osg::Matrix::rotate(osg::DegreesToRadians(90.0f), osg::Vec3{0.0f, 0.0f, 1.0f}) };
	osg::Matrix translate{ osg::Matrix::translate(0.0f, 0.0f, 3.0f) };

	matrixTransform->setMatrix(translate * rotate);
	matrixTransform->addChild(raaAssetLibrary::getNamedAsset("delta", "car0"));

	raaCarFacarde* pCar{
		new raaCarFacarde{ g_pRoot, matrixTransform->asNode(), ap, 50.0}
	};

	g_pRoot->addChild(pCar->root());
}

void createCarTwo(osg::Group* pRoadGroup)
{
	raaAnimationPointFinders apfs;
	osg::AnimationPath* ap = new osg::AnimationPath();

	apfs.push_back(raaAnimationPointFinder("innerStraight2", 1, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("innerStraight2", 3, pRoadGroup));

	apfs.push_back(raaAnimationPointFinder("XJunction1", 11, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("XJunction1", 12, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("XJunction1", 4, pRoadGroup));
	

	apfs.push_back(raaAnimationPointFinder("innerStraight1", 1, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("innerStraight1", 3, pRoadGroup));

	apfs.push_back(raaAnimationPointFinder("TJunction1", 0, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("TJunction1", 3, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("TJunction1", 4, pRoadGroup));


	apfs.push_back(raaAnimationPointFinder("straight1", 1, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("straight1", 3, pRoadGroup));

	apfs.push_back(raaAnimationPointFinder("curve4", 3, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("curve4", 4, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("curve4", 5, pRoadGroup));

	apfs.push_back(raaAnimationPointFinder("straight8", 1, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("straight8", 3, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("TJunction4", 4, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("TJunction4", 4, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("straight7", 1, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("straight7", 3, pRoadGroup));

	apfs.push_back(raaAnimationPointFinder("curve3", 3, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("curve3", 4, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("curve3", 5, pRoadGroup));

	apfs.push_back(raaAnimationPointFinder("straight6", 1, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("straight6", 3, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("TJunction3", 4, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("TJunction3", 4, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("straight5", 1, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("straight5", 3, pRoadGroup));

	apfs.push_back(raaAnimationPointFinder("curve2", 3, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("curve2", 4, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("curve2", 5, pRoadGroup));

	apfs.push_back(raaAnimationPointFinder("straight4", 1, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("straight4", 3, pRoadGroup));

	
	apfs.push_back(raaAnimationPointFinder("TJunction2", 6, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("TJunction2", 7, pRoadGroup));
	//apfs.push_back(raaAnimationPointFinder("TJunction2", 0, pRoadGroup));

	apfs.push_back(raaAnimationPointFinder("innerStraight2", 1, pRoadGroup));




	ap = createAnimationPath(apfs, pRoadGroup);

	osg::ref_ptr<osg::MatrixTransform> matrixTransform{ new osg::MatrixTransform };

	osg::Matrix rotate{ osg::Matrix::rotate(osg::DegreesToRadians(90.0f), osg::Vec3{0.0f, 0.0f, 1.0f}) };
	osg::Matrix translate{ osg::Matrix::translate(0.0f, 0.0f, 3.0f) };

	matrixTransform->setMatrix(translate * rotate);
	matrixTransform->addChild(raaAssetLibrary::getNamedAsset("delta", "car1"));

	raaCarFacarde* pCar{
		new raaCarFacarde{ g_pRoot, matrixTransform->asNode(), ap, 50.0}
	};

	g_pRoot->addChild(pCar->root());
}

void createCarThree(osg::Group* pRoadGroup)
{
	raaAnimationPointFinders apfs;
	osg::AnimationPath* ap = new osg::AnimationPath();

	apfs.push_back(raaAnimationPointFinder("straight2", 0, pRoadGroup));

	apfs.push_back(raaAnimationPointFinder("curve1", 1, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("curve1", 2, pRoadGroup));

	apfs.push_back(raaAnimationPointFinder("straight3", 0, pRoadGroup));

	apfs.push_back(raaAnimationPointFinder("TJunction2", 9, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("TJunction2", 7, pRoadGroup));

	apfs.push_back(raaAnimationPointFinder("innerStraight2", 1, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("innerStraight2", 3, pRoadGroup));

	apfs.push_back(raaAnimationPointFinder("XJunction1", 11, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("XJunction1", 12, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("XJunction1", 4, pRoadGroup));


	apfs.push_back(raaAnimationPointFinder("innerStraight1", 1, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("innerStraight1", 3, pRoadGroup));

	apfs.push_back(raaAnimationPointFinder("TJunction1", 1, pRoadGroup));
	apfs.push_back(raaAnimationPointFinder("TJunction1", 2, pRoadGroup));

	apfs.push_back(raaAnimationPointFinder("straight2", 0, pRoadGroup));


	
	ap = createAnimationPath(apfs, pRoadGroup);

	osg::ref_ptr<osg::MatrixTransform> matrixTransform{ new osg::MatrixTransform };

	osg::Matrix rotate{ osg::Matrix::rotate(osg::DegreesToRadians(90.0f), osg::Vec3{0.0f, 0.0f, 1.0f}) };
	osg::Matrix translate{ osg::Matrix::translate(0.0f, 0.0f, 3.0f) };

	matrixTransform->setMatrix(translate * rotate);
	matrixTransform->addChild(raaAssetLibrary::getNamedAsset("delta", "car2"));

	raaCarFacarde* pCar{
		new raaCarFacarde{ g_pRoot, matrixTransform->asNode(), ap, 150.0}
	};

	g_pRoot->addChild(pCar->root());
}



int main(int argc, char** argv)
{
	raaAssetLibrary::start();
	raaTrafficSystem::start();

	osgViewer::Viewer viewer;

	for (int i = 0; i < argc; i++)
	{
		if (std::string(argv[i]) == "-d") g_sDataPath = argv[++i];
	}

	// the root of the scene - use for rendering
	g_pRoot = new osg::Group();
	g_pRoot->ref();

	// build asset library - instances or clones of parts can be created from this
	raaAssetLibrary::loadAsset("roadStraight", g_sDataPath + "roadStraight.osgb");
	raaAssetLibrary::loadAsset("roadCurve", g_sDataPath + "roadCurve.osgb");
	raaAssetLibrary::loadAsset("roadTJunction", g_sDataPath + "roadTJunction.osgb");
	raaAssetLibrary::loadAsset("roadXJunction", g_sDataPath + "roadXJunction.osgb");
	raaAssetLibrary::loadAsset("trafficLight", g_sDataPath + "raaTrafficLight.osgb");
	raaAssetLibrary::loadAsset("veyron", g_sDataPath + "car-veyron.osgb");
	raaAssetLibrary::loadAsset("delta", g_sDataPath + "car-delta.osgb");
	raaAssetLibrary::insertAsset("vehicle", buildAnimatedVehicleAsset());

	// add a group node to the scene to hold the road sub-tree
	osg::Group* pRoadGroup = new osg::Group();
	g_pRoot->addChild(pRoadGroup);

	osg::Group* trafficLightGroup = new osg::Group();
	g_pRoot->addChild(trafficLightGroup);


	// Create road
	buildRoad(pRoadGroup, trafficLightGroup);

	// Add all the cars
	createCarOne(pRoadGroup);
	createCarTwo(pRoadGroup);
	createCarThree(pRoadGroup);


	// osg setup stuff
	osg::GraphicsContext::Traits* pTraits = new osg::GraphicsContext::Traits();
	pTraits->x = 20;
	pTraits->y = 20;
	pTraits->width = 600;
	pTraits->height = 480;
	pTraits->windowDecoration = true;
	pTraits->doubleBuffer = true;
	pTraits->sharedContext = 0;

	osg::GraphicsContext* pGC = osg::GraphicsContext::createGraphicsContext(pTraits);
	osgGA::KeySwitchMatrixManipulator* pKeyswitchManipulator = new osgGA::KeySwitchMatrixManipulator();
	pKeyswitchManipulator->addMatrixManipulator('1', "Trackball", new osgGA::TrackballManipulator());
	pKeyswitchManipulator->addMatrixManipulator('2', "Flight", new osgGA::FlightManipulator());
	pKeyswitchManipulator->addMatrixManipulator('3', "Drive", new osgGA::DriveManipulator());
	viewer.setCameraManipulator(pKeyswitchManipulator);
	osg::Camera* pCamera = viewer.getCamera();
	pCamera->setGraphicsContext(pGC);
	pCamera->setViewport(new osg::Viewport(0, 0, pTraits->width, pTraits->height));


	TimeCycleSimulation* timeCycleSimulation = new TimeCycleSimulation(pCamera);
	g_pRoot->addUpdateCallback(timeCycleSimulation);
	

	// add own event handler - this currently switches on an off the animation points
	viewer.addEventHandler(new raaInputController(g_pRoot));


	// add the state manipulator
	viewer.addEventHandler(new osgGA::StateSetManipulator(viewer.getCamera()->getOrCreateStateSet()));

	// add the thread model handler
	viewer.addEventHandler(new osgViewer::ThreadingHandler);

	// add the window size toggle handler
	viewer.addEventHandler(new osgViewer::WindowSizeHandler);

	// add the stats handler
	viewer.addEventHandler(new osgViewer::StatsHandler);

	// add the record camera path handler
	viewer.addEventHandler(new osgViewer::RecordCameraPathHandler);

	// add the LOD Scale handler
	viewer.addEventHandler(new osgViewer::LODScaleHandler);

	// add the screen capture handler
	viewer.addEventHandler(new osgViewer::ScreenCaptureHandler);



	viewer.addEventHandler(new PickHandler());




	// set the scene to render
	viewer.setSceneData(g_pRoot);

	viewer.realize();

	return viewer.run();
}


