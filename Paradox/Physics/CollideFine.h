#pragma once

/**
 * @file
 * 
 * This file contains the fine grained collision detection system.
 * It is used to return contacts between pairs of primitives.
 * 
 * There are two groups of tests in this file. 
 * Intersection tests use the fastest separating axis method 
 * to check if two objects intersect, and the collision tests
 * generate the contacts. The collision tests typically use the
 * intersection tests as an early out.
 */

#include "Contacts.h"

// Forward declarations of primitive friends
class IntersectionTests;
class CollisionDetector;

/**
 * Represents a primitive to detect collisions against.
 */

class CollisionPrimitive
{
public:
	/**
	 * This class exists to help the collision detector 
	 * and intersection routines, so they should have
	 * access to its' data.
	 */
	friend class IntersectionTests;
	friend class CollisionDetector;

	/**
	 * The rigid body that is represented by this primitive.
	 */
	RigidBody* body;

	/**
	 * The offset of this primitive from the given rigidbody.
	 */
	Matrix4 offset;

	/**
	 * Calculates the internals for the primitive.
	 */
	void CalculateInternals();

	/**
	 * This is a convenience function to allow access to the
	 * axis vectors in the transform for this primitive.
	 */
	Vector3 GetAxis(unsigned index) const
	{
		return transform.getAxisVector(index);
	}

	/**
	 * Returns the resultant transform of the primitive, calculated
	 * from the combined offset of the primitive and the transform
	 * (orientation + position) of the rigid body to which it is
	 * attached.
	 */
	const Matrix4& GetTransform() const
	{
		return transform;
	}

protected:
	/**
	 * The resultant transform of the primitive. This is
	 * calculated by combining the offset of the primitive
	 * with the transform of the rigidbody.
	 */
	Matrix4 transform;
};

/**
 * Represents a rigidbody that can be treated as a sphere
 * for collision detection
 */
class CollisionSphere : public CollisionPrimitive
{
public:
	/**
	 * The radius of the sphere
	 */
	double radius;
};

/**
 * The plane is not a primitive: it doesn't represent
 * another rigidbody. It is used for contacts with the 
 * immovable world geometry.
 */
class CollisionPlane
{
public:
	/**
	 * The plane normal
	 */
	Vector3 direction;

	/**
	 * The distance of the plane from the origin
	 */
	double offset;
};

/**
 * Represents a rigid body that can be treated as an aligned
 * bounding box for collision detection.
 */
class CollisionBox : public CollisionPrimitive
{
public:
	/**
	 * Holds the half-sizes of the box along each of its' local axes.
	 */
	Vector3 halfSize;
};

/**
 * A wrapper class that holds fast intersection tests.
 * These can be used to drive the coarse collision detection system
 * or as an early out in the full collision tests below.
 */
class IntersectionTests
{
public:

	static bool SphereAndHalfSpace(
		const CollisionSphere& sphere,
		const CollisionPlane& plane
	);

	static bool SphereAndSphere(
		const CollisionSphere& one,
		const CollisionSphere& two
	);

	static bool BoxAndBox(
		const CollisionBox& one,
		const CollisionBox& two
	);

	/**
	 * Does an intersection test of an arbitrarily aligned box
	 * and a half space.
	 * 
	 * The box is given as a transform matrix, including position,
	 * and a vector of half-sizes for the extent of the box along
	 * each local axis.
	 * 
	 * The half-space is given as a direction (i.e. unit) vector 
	 * and the offset of the limiting plane from the origin, along
	 * the given direction.
	 */

	static bool BoxAndHalfSpace(
		const CollisionBox &box,
		const CollisionPlane *plane
	)
};

/**
 * A helper structure that contains information for the detector
 * to use in building its' contact data.
 */

struct CollisionData
{
	/**
	 * Holds the base of the collision data: the first contact
	 * in the array. This is used so that the contact pointer (below)
	 * can be incremented each time a contact is detected, while
	 * this pointer points to the first contact found.
	 */
	Contact* contactArray;

	// Holds the contact array to write into
	Contact* contacts;

	// Holds the maximum number of contacts the array can take
	int contactsLeft;

	// Holds the number of contacts found so far
	unsigned contactCount;

	// Holds the friction value to write into any collisions
	double friction;

	// Holds the restitution value to write into any collisions
	double restitution;

	/**
	 * Holds the collision tolerance, even uncolliding objects
	 * this close should have collisions generated.
	 */
	double tolerance;

	/**
	 * Checks if there are more contacts available in the contact data
	 */
	bool HasMoreContacts()
	{
		return contactsLeft > 0;
	}

	/**
	 * Resets the data so that is has no used
	 * contacts recorded
	 */
	void Reset(unsigned maxContacts)
	{
		contactsLeft = maxContacts;
		contactCount = 0;
		contacts = contactArray;
	}

	/**
	 * Notifies the data that the given number of contacts 
	 * have been added
	 */
	void AddContacts(unsigned count)
	{
		// Reduce the number of contacts remaining, add number used
		contactsLeft -= count;
		contactCount += count;

		// Move the array forward
		contacts += count;
	}
};

/**
 * A wrapper class that holds the fine grained collision detection
 * routines.
 * 
 * Each of the functions has the same format: it takes the details
 * of two objects, and a pointer to a contact array to fill.
 * It returns the number of contacts it wrote into the array.
 */
class CollisionDetector
{
public:
	
	static unsigned SphereAndHalfSpace(
		const CollisionSphere& sphere,
		const CollisionPlane& plane,
		CollisionData* data
	);

	static unsigned SphereAndTruePlane(
		const CollisionSphere& sphere,
		const CollisionPlane& plane,
		CollisionData* data
	);

	static unsigned SphereAndSphere(
		const CollisionSphere& one,
		const CollisionSphere& two,
		CollisionData* data
	);

	/**
	 * Does a collision test on a collision box and a plane 
	 * representing a half-space (i.e. the normal of the plan
	 * points out of the half-space).
	 */
	static unsigned BoxAndHalfSpace(
		const CollisionBox& box,
		const CollisionPlane& plane,
		CollisionData* data
	);

	static unsigned BoxAndBox(
		const CollisionBox& one,
		const CollisionBox& two,
		CollisionData* data
	);

	static unsigned BoxAndPoint(
		const CollisionBox& box,
		const Vector3& point,
		CollisionData* data
	);

	static unsigned BoxAndSphere(
		const CollisionBox& box,
		const CollisionSphere& sphere,
		CollisionData* data
	);
};
