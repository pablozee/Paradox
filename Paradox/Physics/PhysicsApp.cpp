#include "PhysicsApp.h"

RigidBodyApplication::RigidBodyApplication()
	:
	theta(0.0f),
	phi(15.0f),
	resolver(maxContacts * 8),
	renderDebugInfo(false),
	pauseSimulation(true),
	autoPauseSimulation(false)
{
	cData.contactArray = contacts;
}

void RigidBodyApplication::update()
{
	// Find the duration of the last frame in seconds
	float duration = (float)TimingData::get().lastFrameDuration * 0.001f;
	if (duration < 0.0f) return;
	else if (duration > 0.05f) duration = 0.05f;
}

