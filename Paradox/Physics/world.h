#pragma once

#include "body.h"
#include "contacts.h"
#include <complex>

class World
{
	bool calculateResolverIterations;

	// Linked list of bodies
	struct BodyRegistration
	{
		Rigidbody *body;
		BodyRegistration* next;
	};

	// Head of list of bodies
	BodyRegistration* firstBody;

	ContactResolver resolver;

	struct ContactGenRegistration
	{
		ContactGenerator* generator;
		ContactGenRegistration* next;
	};

	ContactGenRegistration* firstContactGenerator;

	Contact* contacts;

	unsigned maxContacts;

public:
	World(unsigned maxContacts, unsigned iterations = 0);
	~World();

	// Calls each of the registered contact generators to report 
	// their contacts. Returns total number of generated contacts.

	unsigned generateContacts();

	void runPhysics(float duration);

	// Initializes the world for a simulation frame.
	// Clears the force and torque accumulators for bodies in the world
	// After calling this, the bodies can have their forces and torques
	// for this frame added.
	void startFrame();
};