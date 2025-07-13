#pragma once

#include <list>

typedef std::list<class raaCollisionTarget*> raaCollisionTargets;
typedef std::list<class TrafficLightFacarde*> TrafficLightFacardes;
typedef std::list<class TrafficLightControl*> TrafficLightControls;


class raaTrafficSystem
{
public:
	static void start();
	static void end();
	static void addTarget(raaCollisionTarget* pTarget);
	static void removeTarget(raaCollisionTarget* pTarget);
	static void addTarget(TrafficLightFacarde* pTarget);
	static void removeTarget(TrafficLightFacarde* pTarget);	
	static void addTarget(TrafficLightControl* pTarget);
	static void removeTarget(TrafficLightControl* pTarget);
	static const raaCollisionTargets& colliders();
	static const TrafficLightFacardes& trafficLights();
	static const TrafficLightControls& trafficLightControls();
protected:
	static raaCollisionTargets sm_lColliders;
	static TrafficLightFacardes sm_trafficLights;
	static TrafficLightControls sm_trafficLightControls;
};

