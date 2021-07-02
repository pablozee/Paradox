#pragma once

#include "contacts.h"
#include "CollideFine.h"
#include "CollideCoarse.h"

class RigidBodyApplication
{

protected:

	// Holds the maximum number of contacts
	const static unsigned maxContacts = 256;

	// Holds the array of contacts
	Contact contacts[maxContacts];

	// Holds the collision data structure for collision detection
	CollisionData cData;

	// Holds the contact resolver
	ContactResolver resolver;

	// Holds the camera angle
	float theta;

	// Holds the camera elevation
	float phi;

	// Holds the position of the mouse at the last frame 
	// of a mouse drag
	int last_x, last_y;

	// True if the contacts should be rendered
	bool renderDebugInfo;

	// True if the simulation is paused
	bool pauseSimulation;

	// Pauses the simulation after the next frame automatically
	bool autoPauseSimulation;

	// Processes the contact generation code
//	virtual void generateContacts() = 0;

	// Processes the objects in the simulation forward in time
//	virtual void updateObjects(double duration) = 0;

	// Finishes drawing the frame, adding debug information
	// as needed
//	void drawDebug();

	// Resets the simulation
//	virtual void reset() = 0;

public:

	// Creates a new application object
	RigidBodyApplication();

	// Display the application
//	virtual void display();

	// Update the objects
	virtual void Update();

	// TODO if necessary - mouse click, mouse drag, key press
};