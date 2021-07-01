#include "PhysicsApp.h"
#include "Timing.h"

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
//	float duration = (float)TimingData::get().lastFrameDuration * 0.001f;
	float duration = 0.03f;
	if (duration < 0.0f) return;
	else if (duration > 0.05f) duration = 0.05f;

	// Exit immediately if we aren't running the simulation
	if (pauseSimulation)
	{
		// TODO May need to put in app.update.
		return;
	}
	else if (autoPauseSimulation)
	{
		pauseSimulation = true;
		autoPauseSimulation = false;
	}

	// Update the objects
//	updateObjects(duration);

	// Perform the contact generation
//	generateContacts();

	// Resolve the detected contacts
	resolver.ResolveContacts(cData.contactArray, cData.contactCount, duration);
}

