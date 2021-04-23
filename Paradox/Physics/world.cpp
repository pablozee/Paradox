#include <cstdlib>
#include "world.h"

World::World(unsigned maxContacts, unsigned iterations)
	:
	firstBody(NULL),
	resolver(iterations),
	firstContactGenerator(NULL),
	maxContacts(maxContacts)
{
	contacts = new Contact[maxContacts];
	calculateResolverIterations = (iterations == 0);
}

World::~World()
{
	delete[] contacts;
}

void World::StartFrame()
{
	BodyRegistration* currentBodyReg = firstBody;

	while (currentBodyReg)
	{
		currentBodyReg->body->ClearAccumulators();
		currentBodyReg->body->CalculateDerivedData();

		currentBodyReg = currentBodyReg->next;
	}
}

unsigned World::GenerateContacts()
{
	unsigned limit = maxContacts;
	Contact* nextContact = contacts;

	ContactGenRegistration* currentContactGenReg = firstContactGenerator;

	while (currentContactGenReg)
	{
		unsigned used = currentContactGenReg->generator->AddContact(nextContact, limit);
		limit -= used;
		nextContact += used;

		// Check if we've run out of contacts to fill, 
		// meaning we're missing contacts
		if (limit <= 0) break;

		currentContactGenReg = currentContactGenReg->next;
	}

	// Return number of contacts used
	return maxContacts - limit;
}

void World::RunPhysics(double duration)
{
	BodyRegistration* currentBodyReg = firstBody;

	while (currentBodyReg)
	{
		// Remove all forces from the accumulator
		currentBodyReg->body->Integrate(duration);

		currentBodyReg = currentBodyReg->next;
	}

	unsigned usedContacts = GenerateContacts();

	// Process generated contacts
	if (calculateResolverIterations) resolver.SetIterations(usedContacts * 4);
	resolver.ResolveContacts(contacts, usedContacts, duration);
}