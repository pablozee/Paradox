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
	Matrix3 inverseInertiaTensor[2];
	body[0]->GetInverseInertiaTensorWorld(&inverseInertiaTensor[0]);
	if (body[1])
		body[1]->GetInverseInertiaTensorWorld(&inverseInertiaTensor[1]);

	// Calculate impuse for each contact axis
	Vector3 impulseContact;

	if (friction == 0.0)
	{
		impulseContact = CalculateFrictionlessImpulse(inverseInertiaTensor);
	}
	else
	{
		// Different impulse calculation as we may have impulses
		// that aren't in the direction of the contact
		impulseContact = CalculateFrictionImpulse(inverseInertiaTensor);
	}

	// Convert impulse to world coordinates
	Vector3 impulse = contactToWorld.transform(impulseContact);

	// Split the impulse into linear and rotational components
	Vector3 impulsiveTorque = relativeContactPosition[0] % impulse;
	rotationChange[0] = inverseInertiaTensor[0].transform(impulsiveTorque);
	velocityChange[0].clear();
	velocityChange[0].addScaledVector(impulse, body[0]->GetInverseMass());

	// Apply the changes
	body[0]->AddVelocity(velocityChange[0]);
	body[0]->AddRotation(velocityChange[0]);
	
	if (body[1])
	{
		// Split the impulse into linear and rotational components
		Vector3 impulsiveTorque = impulse % relativeContactPosition[1];
		rotationChange[1] = inverseInertiaTensor[1].transform(impulsiveTorque);
		velocityChange[1].clear();
		velocityChange[1].addScaledVector(impulse, -body[1]->GetInverseMass());

		// Apply the changes
		body[1]->AddVelocity(velocityChange[0]);
		body[1]->AddRotation(velocityChange[0]);
	}
}

inline Vector3 Contact::CalculateFrictionlessImpulse(Matrix3* inverseInertiaTensor)
{
	Vector3 impulseContact;

	// Build a vector that shows the change in velocity in
	// world space for a unit impulse in the direction of the 
	// contact normal
	Vector3 deltaVelWorld = relativeContactPosition[0] % contactNormal;
	deltaVelWorld = inverseInertiaTensor[0].transform(deltaVelWorld);
	deltaVelWorld = deltaVelWorld % relativeContactPosition[0];

	// Work out the change in velocity in contact coordinates
	double deltaVelocity = deltaVelWorld * contactNormal;

	// Add the linear component of velocity change
	deltaVelocity += body[0]->GetInverseMass();

	if (body[1])
	{
		// Go through the same transformation sequence again
		Vector3 deltaVelWorld = relativeContactPosition[1] % contactNormal;
		deltaVelWorld = inverseInertiaTensor[1].transform(deltaVelWorld);
		deltaVelWorld = deltaVelWorld % relativeContactPosition[1];
	
		// Add the change in velocity due to rotation
		deltaVelocity += deltaVelWorld * contactNormal;

		// Add the change in velocity due to linear motion
		deltaVelocity += body[1]->GetInverseMass();
	}

	// Calculate the required size of the impulse
	impulseContact.x = desiredDeltaVelocity / deltaVelocity;
	impulseContact.y = 0;
	impulseContact.z = 0;

	return impulseContact;
}

inline Vector3 Contact::CalculateFrictionImpulse(Matrix3* inverseInertiaTensor)
{
	Vector3 impulseContact;
	double inverseMass = body[0]->GetInverseMass();

	// The equivalent of a cross product in matrices is multiplication
	// by a skew symmetric matrix - we build the matrix for converting
	// between linear and angular quantities.

	Matrix3 impulseToTorque;
	impulseToTorque.setSkewSymmetric(relativeContactPosition[0]);

	// Build the matrix to convert contact impulse to change in velocity
	// in world coordinates.
	
	Matrix3 deltaVelWorld = impulseToTorque;
	deltaVelWorld *= inverseInertiaTensor[0];
	deltaVelWorld *= impulseToTorque;
	deltaVelWorld *= -1;

	// Check if we need to add body twos' data

	if (body[1])
	{
		// Set the cross product matrix
		
		impulseToTorque.setSkewSymmetric(relativeContactPosition[1]);

		// Calculate the velocity change matrix

		Matrix3 deltaVelWorld2 = impulseToTorque;
		deltaVelWorld *= inverseInertiaTensor[1];
		deltaVelWorld *= impulseToTorque;
		deltaVelWorld *= -1;

		// Add to the total delta velocity

		deltaVelWorld += deltaVelWorld2;

		inverseMass += body[1]->GetInverseMass();
	}

	// Do a change of basis to convert into contact coordinates.

	Matrix3 deltaVelocity = contactToWorld.transpose();
	deltaVelocity *= deltaVelWorld;
	deltaVelocity *= contactToWorld;

	// Add in the linear velocity change
	
	deltaVelocity.data[0] += inverseMass;
	deltaVelocity.data[4] += inverseMass;
	deltaVelocity.data[8] += inverseMass;

	// Invert to get the impulse needed per unit velocity

	Matrix3 impulseMatrix = deltaVelocity.inverse();

	// Find the target velocities to kill

	Vector3 velKill(desiredDeltaVelocity, -contactVelocity.y, -contactVelocity.z);

	// Find the impulse to kill target velocities

	impulseContact = impulseMatrix.transform(velKill);

	// Check for exceeding friction

	double planarImpulse = sqrt(impulseContact.y * impulseContact.y 
							  +
							  impulseContact.z * impulseContact.z);

	if (planarImpulse > impulseContact.x * friction)
	{
		// We need to use dynamic friction
		
		impulseContact.y /= planarImpulse;
		impulseContact.z /= planarImpulse;
		
		impulseContact.x = deltaVelocity.data[0] +
			deltaVelocity.data[1] * friction * impulseContact.y +
			deltaVelocity.data[2] * friction * impulseContact.z;

		impulseContact.x = desiredDeltaVelocity / impulseContact.x;
		impulseContact.y *= friction * impulseContact.x;
		impulseContact.z *= friction * impulseContact.x;
	}

	return impulseContact;
}

void Contact::ApplyPositionChange(Vector3 linearChange[2], Vector3 angularChange[2], double penetration)
{
	const double angularLimit = (double)0.2f;
	double angularMove[2];
	double linearMove[2];

	double totalInertia = 0;
	double linearInertia[2];
	double angularInertia[2];

	// We need to work out the inertia of each object in the direction
	// of the contact normal, due to angular inertia only.
	
	for (unsigned i = 0; i < 2; i++) if (body[i])
	{
		Matrix3 inverseInertiaTensor;
		body[i]->GetInverseInertiaTensorWorld(&inverseInertiaTensor);

		// Use the same procedure as for calculating frictionless
		// velocity change to work out the angular inertia.

		Vector3 angularInertiaWorld =
			relativeContactPosition[i] % contactNormal;
		angularInertiaWorld =
			inverseInertiaTensor.transform(angularInertiaWorld);
		angularInertiaWorld = 
			angularInertiaWorld % relativeContactPosition[i];
		angularInertia[i] =
			angularInertiaWorld * contactNormal;

		// The linear component is simply the inverse mass
		
		linearInertia[i] = body[i]->GetInverseMass();

		// Keep track of the total inertia from all components

		totalInertia += linearInertia[i] + angularInertia[i];

		// We break the loop here so that the totalInertia value is
		// completely calculated (by both iterations) before continuing.
	}

	// Loop through again calculating and applying the changes

	for (unsigned i = 0; i < 2; i++) if (body[i])
	{
		// The linear and angular movements required are in proportion
		// to the two inverse inertias.

		double sign = (i == 0) ? 1 : -1;
		angularMove[i] =
			sign * penetration * (angularInertia[i] / totalInertia);
		linearMove[i] =
			sign * penetration * (linearInertia[i] / totalInertia);
		
		// To avoid angular projections that too great (when mass is 
		// large but inertia tensor is small) limit the angular move.

		Vector3 projection = relativeContactPosition[i];
		projection.addScaledVector(
			contactNormal,
			-relativeContactPosition[i].scalarProduct(contactNormal)
		);

		// Use the small angle approximation for the sine of the angle
		// (i.e. the magnitude would be sine(angularLimit) * projection.magnitude
		// but we approximate sine(angularLimit) to angularLimit).

		double maxMagnitude = angularLimit * projection.magnitude();

		if (angularMove[i] < -maxMagnitude)
		{
			double totalMove = angularMove[i] + linearMove[i];
			angularMove[i] = -maxMagnitude;
			linearMove[i] = totalMove - angularMove[i];
		}
		else if (angularMove[i] > maxMagnitude)
		{
			double totalMove = angularMove[i] + linearMove[i];
			angularMove[i] = maxMagnitude;
			linearMove[i] = totalMove - angularMove[i];
		}

		// We have the linear amount of movement required by
		// turning the rigid body (in angularMove[i]). We now need
		// to calculate the desired rotation to achieve that.

		if (angularMove[i] == 0)
		{
			// Easy case - no angular movement means no rotation.
			angularChange[i].clear();
		}
		else
		{
			// Work out the direction we'd like to rotate in.

			Vector3 targetAngularDirection =
				relativeContactPosition[i].vectorProduct(contactNormal);

			Matrix3 inverseInertiaTensor;
			body[i]->GetInverseInertiaTensorWorld(&inverseInertiaTensor);

			// Work out the direction we'd need to rotate to achieve that
			angularChange[i] =
				inverseInertiaTensor.transform(targetAngularDirection) *
				(angularMove[i] / angularInertia[i]);
		}

		// Velocity change is easier - it is just the linear movement
		// along the contact normal.

		linearChange[i] = contactNormal * linearMove[i];

		// Now we can start to apply the values we've calculated.
		// Apply the linear movement.
		Vector3 pos;
		body[i]->GetPosition(&pos);
		pos.addScaledVector(contactNormal, linearMove[i]);
		body[i]->SetPosition(pos);

		// Add the change in orientation
		Quaternion q;
		body[i]->GetOrientation(&q);
		q.AddScaledVector(angularChange[i], (double)1.0);
		body[i]->SetOrientation(q);

		// We need to calculate the derived data for any body that is
		// asleep, so that the changes are reflected in the objects' data.
		// Otherwise, the resolution will not change the position
		// of the object, and the next collision detection round will
		// have the same penetration.
		if (!body[i]->GetAwakeStatus()) body[i]->CalculateDerivedData();
	}
}