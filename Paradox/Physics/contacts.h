#pragma once

class Contact
{

};

class ContactResolver
{
public:
	// Creates new contact resolver with given number of iterations per resolution call
	ContactResolver(unsigned iterations, float velocityEpsilon = 0.01f, float positionEpsilon = 0.01f);

	ContactResolver(unsigned iterations, unsigned positionIterations, float velocityEpsilon = 0.01f, float positionEpsilon = 0.01f);

	void SetIterations(unsigned usedContacts);

	void ResolveContacts(Contact* contacts, unsigned usedContacts, floatDuration);
};

class ContactGenerator
{
public: 
	unsigned AddContact(Contact* nextContact, unsigned limit);
};