#include "Contacts.h"
#include <memory.h>
#include <assert.h>

void Contact::SetBodyData(RigidBody* one, RigidBody* two, double friction, double restitution)
{
	Contact::body[0] = one;
	Contact::body[1] = two;
	Contact::friction = friction;
	Contact::restitution = restitution;
}

void Contact::MatchAwakeState()
{
	// Collisions with the world never cause a body to wake up
	if (!body[1]) return;

	bool body0Awake = body[0]->GetAwakeStatus();
	bool body1Awake = body[1]->GetAwakeStatus();

	// Only wake up the sleeping one
	if (body0Awake ^ body1Awake) {
		if (body0Awake) body[1]->SetAwakeStatus();
		else body[0]->SetAwakeStatus();
	}
}

// Swaps bodies in the current contact
// Changes direction of the contact normal
// Call CalculateInternals afterwards if
// manually calling this function
void Contact::SwapBodies()
{
	contactNormal *= -1;

	RigidBody* temp = body[0];
	body[0] = body[1];
	body[1] = temp;
}

// Construct an orthonormal 3x3 matrix for the contact
// Each vector is a column, column major
// (Matrix transforms contact space into world space)
// The x direction is generated from the contact normal,
// and the y and z directions are constructed at right angles to it
inline void Contact::CalculateContactBasis()
{
	Vector3 contactTangent[2];

	// Check whether the Z-axis is nearer to the X axis or Y axis
	if (abs(contactNormal.x) > abs(contactNormal.y))
	{
		// Scaling factor to ensure the results are normalized
		const double scalingFactor = 1.0 / sqrt(contactNormal.z * contactNormal.z +
												contactNormal.x * contactNormal.x);

		// The new X-axis is at right angles to the world Y-axis
		contactTangent[0].x = contactNormal.z * scalingFactor;
		contactTangent[0].y = 0;
		contactTangent[0].z = -contactNormal.x * scalingFactor;

		// The new Y-axis is at right angles to the new X- and Z- axes
		contactTangent[1].x = contactNormal.y * contactTangent[0].x;
		contactTangent[1].y = contactNormal.z * contactTangent[0].x -
			contactNormal.x * contactTangent[0].z;
		contactTangent[1].z = -contactNormal.y * contactTangent[0].x;
	}
	else
	{
		// Scaling factor to ensure the results are normalized
		const double scalingFactor = 1.0 / sqrt(contactNormal.z * contactNormal.z +
			contactNormal.y * contactNormal.y);

		// The new X-axis is at right angles to world X-axis
		contactTangent[0].x = 0;
		contactTangent[0].y = -contactNormal.z * scalingFactor;
		contactTangent[0].z = contactNormal.y * scalingFactor;

		// The new Y-axis is at right angles to the new X- and Z- axes
		contactTangent[1].x = contactNormal.y * contactTangent[0].z -
							  contactNormal.z * contactTangent[0].y;
		contactTangent[1].y = -contactNormal.x * contactTangent[0].z;
		contactTangent[1].z = contactNormal.x * contactTangent[0].z;
	}

	// Make a matrix from the three vectors
	contactToWorld.setComponents(contactNormal, contactTangent[0], contactTangent[1]);
}

Vector3 Contact::CalculateLocalVelocity(unsigned bodyIndex, double duration)
{
	RigidBody* thisBody = body[bodyIndex];

	// Work out the velocity of the contact point
	Vector3 velocity = thisBody->GetRotation() % relativeContactPosition[bodyIndex];
	velocity += thisBody->GetVelocity();

	// Convert velocity into contact-coordinates
	Vector3 contactVelocity = contactToWorld.transformTranspose(velocity);

	// Calculate the amount of velocity due to forces without reactions
	Vector3 accVelocity = thisBody->GetLastFrameAcceleration() * duration;

	// Convert velocity into contact-coordinates
	accVelocity = contactToWorld.transformTranspose(accVelocity);

	// We ignore any component of acceleration in the contact normal direction,
	// we are only interested in planar acceleration
	accVelocity.x = 0;

	// Add the planar velocities - if there's enough friction they 
	// will be removed during velocity resolution
	contactVelocity += accVelocity;

	return contactVelocity;
}

void Contact::CalculateDesiredDeltaVelocity(double duration)
{
	const static double velocityLimit = 0.25;

	// Calculate the acceleration induced velocity accumulated this frame
	double velocityFromAcc = 0;

	if (body[0]->GetAwakeStatus())
	{
		velocityFromAcc += body[0]->GetLastFrameAcceleration() * duration * contactNormal;
	}

	if (body[1] && body[1]->GetAwakeStatus())
	{
		velocityFromAcc += body[1]->GetLastFrameAcceleration() * duration * contactNormal;
	}

	// If the velocity is very slow, limit the restitution
	double thisRestitution = restitution;

	if (abs(contactVelocity.x) < velocityLimit)
	{
		thisRestitution = 0.0;
	}

	// Combine the bounce velocity with the removed acceleration velocity
	desiredDeltaVelocity = -contactVelocity.x - thisRestitution * (contactVelocity.x - velocityFromAcc);
}

void Contact::CalculateInternals(double duration)
{
	// Check if the first object is NULL, and swap if it is
	if (!body[0]) SwapBodies();
	assert(body[0]);

	// Calculate a set of axes at the contact point
	CalculateContactBasis();

	// Store the relative position of the contact relative to each body
	relativeContactPosition[0] = contactPoint - body[0]->GetPosition();
	
	if (body[1])
	{
		relativeContactPosition[1] = contactPoint - body[1]->GetPosition();
	}

	// Find the relative velocity of the bodies at the contact point
	contactVelocity = CalculateLocalVelocity(0, duration);
	if (body[1])
	{
		contactVelocity -= CalculateLocalVelocity(1, duration);
	}

	// Calculate the desired change in velocity for resolution 
	CalculateDesiredDeltaVelocity(duration);
}

void Contact::ApplyVelocityChange(Vector3 velocityChange[2], Vector3 rotationChange[2])
{
	// Get hold of the inverse mass and inverse inertia tensor, both in world coordinates
}