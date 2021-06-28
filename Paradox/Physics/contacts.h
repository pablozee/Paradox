#pragma once
#include "Body.h"

//Forward declaration
class ContactResolver;

// The contact has no callable functions, it just holds the contact details
// To resolve a set of contacts, use the contact resolver class
class Contact
{
	friend class ContactResolver;

public:
	
	RigidBody* body[2];
	double friction;
	double restitution;
	Vector3 contactPoint;
	// Direction of the normal in world coordinates
	Vector3 contactNormal;
	double penetration;

	void SetBodyData(RigidBody* one, RigidBody* two, double friction, double restitution);

protected:
	Matrix3 contactToWorld;
	Vector3 contactVelocity;
	double desiredDeltaVelocity;
	Vector3 relativeContactPosition[2];

protected:
	// Called before the resolution algorithm tries to do any resolution
	// should never need to be called manually
	void CalculateInternals(double duration);

	// Reverses the contact by swapping the two rigid bodies and 
	// reversing the contact normal. The internal values should then 
	// be recalculated using CalculateInternals 
	void SwapBodies();

	// Updates awake state of rigid bodies in contact
	// A body will be made awake if it is in contact with a body
	// that is awake
	void MatchAwakeState();

	void CalculateDesiredDeltaVelocity(double duration);

	// Calculates and returns the velocity of the contact
	// point on the given body
	Vector3 CalculateLocalVelocity(unsigned bodyIndex, double duration);

	// Calculate orthonormal basis for the contact point,
	// based on the primary friction direction (for anisotropic friction) 
	// or a random orientation (for isotropic friction)
	void CalculateContactBasis();

	// Applies impulse to the given body with the velocity change stored
	// in the given variable
	void ApplyImpulse(const Vector3& impulse, RigidBody* body, Vector3* velocityChange, Vector3* rotationChange);

	// Performs an inertia-weighted impulse based resolution of this contact alone
	void ApplyVelocityChange(Vector3 velocityChange[2], Vector3 rotationChange[2]);

	// Performs an inertia-weighted penetration resolution of this contact alone
	void ApplyPositionChange(Vector3 linearChange[2], Vector3 angularChange[2], double penetration);

	// Calculates the impulse needed to resolve this contact, given that the contact
	// has no friction. A pair of inertia tensors, one for each contact object,
	// is specified to save calculation time.
	Vector3 CalculateFrictionlessImpulse(Matrix3* inverseInertiaTensor);

	// Calculates the impulse needed to resolve this contact, given that the contact
	// has non-zero coefficient of friction. A pair of inertia tensors, one for each contact object,
	// is specified to save calculation time.
	Vector3 CalculateFrictionImpulse(Matrix3* inverseInertiaTensor);
};

class ContactResolver
{
public:
	// Creates new contact resolver with given number of iterations per resolution call
	ContactResolver(unsigned iterations, double velocityEpsilon = 0.01, double positionEpsilon = 0.01);

	ContactResolver(unsigned iterations, unsigned positionIterations, double velocityEpsilon = 0.01, double positionEpsilon = 0.01);

	bool isResolverValid()
	{
		return (velocityIterations > 0) &&
			   (positionIterations > 0) &&
			   (positionEpsilon >= 0.0f) &&
			   (positionEpsilon >= 0.0f);
	}

	void SetIterations(unsigned iterations);
	void SetIterations(unsigned velocityIterations, unsigned positionIterations);
	
	void SetEpsilon(double velocityEpsilon, double positionEpsilon);

	// Contacts that cannot interact with each other should be 
	// passed to seperate calls to ResolveContacts as the resolution algorithm
	// takes much longer for lots of contacts than it does for the 
	// same number of contacts in small sets
	void ResolveContacts(Contact* contacts, unsigned numContacts, double duration);

protected:
	// Configures internal data of contacts before processing 
	// and makes sure the correct set of bodies is made alive
	void PrepareContacts(Contact* contacts, unsigned numContacts, double duration);


	//Resolves the velocity issues with the given array of constraints,
	// using the given number of iterations.
	void AdjustVelocities(Contact* contactArray,
						  unsigned numContacts,
						  double duration);

	// Resolves the positional issues with the given array of constraints,
	// using the given number of iterations.
	void AdjustPositions(Contact* contacts,
						 unsigned numContacts,
						 double duration);

protected:
	unsigned velocityIterations;
	unsigned positionIterations;

	// Velocities smaller than velocity epsilon considered as 0
	// Too small an epsilon may lead to an unstable simulation
	// Too high an epsilon may lead to bodies interpenetrating visually
	double velocityEpsilon;

	// To avoid instability, penetrations smaller than this value 
	// are considered to be not interpenetrating
	double positionEpsilon;

public:
	unsigned velocityIterationsUsed;
	unsigned positionIterationsUsed;

private:
	bool validSettings;
};

class ContactGenerator
{
public: 
	// Fills the given contact structure with the generated contact
	// The contact pointer should point to the first available contact
	// in a contact array
	virtual unsigned AddContact(Contact* nextContact, unsigned limit) = 0;
};