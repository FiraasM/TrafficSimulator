
#include "raaCollisionTarget.h"
#include "raaTrafficSystem.h"

raaCollisionTargets raaTrafficSystem::sm_lColliders;
TrafficLightFacardes raaTrafficSystem::sm_trafficLights;
TrafficLightControls raaTrafficSystem::sm_trafficLightControls;

void raaTrafficSystem::start()
{
	sm_lColliders.clear();
	sm_trafficLights.clear();
	sm_trafficLightControls.clear();
}

void raaTrafficSystem::end()
{
	sm_lColliders.clear();
	sm_trafficLights.clear();
	sm_trafficLightControls.clear();
}

void raaTrafficSystem::addTarget(raaCollisionTarget* pTarget)
{
	if (pTarget && std::find(sm_lColliders.begin(), sm_lColliders.end(), pTarget) == sm_lColliders.end()) sm_lColliders.push_back(pTarget);
}

void raaTrafficSystem::removeTarget(raaCollisionTarget* pTarget)
{
	if (pTarget && std::find(sm_lColliders.begin(), sm_lColliders.end(), pTarget) != sm_lColliders.end()) sm_lColliders.remove(pTarget);
}

void raaTrafficSystem::addTarget(TrafficLightFacarde* pTarget)
{
	if (pTarget && std::find(sm_trafficLights.begin(), sm_trafficLights.end(), pTarget) == sm_trafficLights.end()) sm_trafficLights.push_back(pTarget);
}

void raaTrafficSystem::removeTarget(TrafficLightFacarde* pTarget)
{
	if (pTarget && std::find(sm_trafficLights.begin(), sm_trafficLights.end(), pTarget) != sm_trafficLights.end()) sm_trafficLights.remove(pTarget);
}

void raaTrafficSystem::addTarget(TrafficLightControl* pTarget)
{
	if (pTarget && std::find(sm_trafficLightControls.begin(), sm_trafficLightControls.end(), pTarget) == sm_trafficLightControls.end()) sm_trafficLightControls.push_back(pTarget);
}

void raaTrafficSystem::removeTarget(TrafficLightControl* pTarget)
{
	if (pTarget && std::find(sm_trafficLightControls.begin(), sm_trafficLightControls.end(), pTarget) != sm_trafficLightControls.end()) sm_trafficLightControls.remove(pTarget);
}

const raaCollisionTargets& raaTrafficSystem::colliders()
{
	return sm_lColliders;
}

const TrafficLightFacardes& raaTrafficSystem::trafficLights() {
	return sm_trafficLights;

}

const TrafficLightControls& raaTrafficSystem::trafficLightControls() {
	return sm_trafficLightControls;
}



