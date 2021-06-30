#include "CollideFine.h"
#include <memory.h>
#include <assert.h>
#include <cstdlib>
#include <cstdio>

void CollisionPrimitive::CalculateInternals()
{
	transform = body->GetTransform() * offset;
}

bool IntersectionTests::SphereAndHalfSpace(const CollisionSphere& sphere, const CollisionPlane& plane)
{
	// Find the distance from the origin
	double ballDistance = plane.direction * sphere.GetAxis(3) - sphere.radius;

	// Check for the intersection
	return ballDistance <= plane.offset;
}

bool IntersectionTests::SphereAndSphere(const CollisionSphere& one, const CollisionSphere& two)
{
	// Find the vector between the objects
	Vector3 midline = one.GetAxis(3) - two.GetAxis(3);

	// See if it is large enough
	return midline.squareMagnitude() <
		(one.radius + two.radius) * (one.radius + two.radius);
}

static inline double TransformToAxis(const CollisionBox& box, const Vector3& axis)
{
	return
		box.halfSize.x * abs(axis * box.GetAxis(0)) +
		box.halfSize.y * abs(axis * box.GetAxis(1)) +
		box.halfSize.z * abs(axis * box.GetAxis(2));
}

/**
 * This function checks if the two boxes overlap along
 * the given axis. The final parameter toCentre is used
 * to pass in the vector between the boxes centre points,
 * to avoid having to recalculate it each time.
 */
static inline bool OverlapOnAxis(
	const CollisionBox& one,
	const CollisionBox& two,
	const Vector3& axis,
	const Vector3& toCentre
)
{
	// Project the half-size of one onto axis
	double oneProject = TransformToAxis(one, axis);
	double twoProject = TransformToAxis(two, axis);

	// Project this onto the axis
	double distance = abs(toCentre * axis);

	// Check for overlap
	return (distance < oneProject + twoProject);
}

/**
 * This preprocessor definition is only used as a convenience
 * in the BoxAndBox intersection method
 */
#define TEST_OVERLAP(axis) OverlapOnAxis(one, two, (axis), toCentre)

bool IntersectionTests::BoxAndBox(const CollisionBox& one, const CollisionBox& two)
{
	// Find the vector between the two centres
	Vector3 toCentre = two.GetAxis(3) - one.GetAxis(3);

	return (

		// Check on box one's axes first
		TEST_OVERLAP(one.GetAxis(0)) &&
		TEST_OVERLAP(one.GetAxis(1)) &&
		TEST_OVERLAP(one.GetAxis(2)) &&

		// And on two's
		TEST_OVERLAP(two.GetAxis(0)) &&
		TEST_OVERLAP(two.GetAxis(1)) &&
		TEST_OVERLAP(two.GetAxis(2)) &&

		// Now on the cross products
		TEST_OVERLAP(one.GetAxis(0) % two.GetAxis(0)) &&
		TEST_OVERLAP(one.GetAxis(0) % two.GetAxis(1)) &&
		TEST_OVERLAP(one.GetAxis(0) % two.GetAxis(2)) &&
		TEST_OVERLAP(one.GetAxis(1) % two.GetAxis(0)) &&
		TEST_OVERLAP(one.GetAxis(1) % two.GetAxis(1)) &&
		TEST_OVERLAP(one.GetAxis(1) % two.GetAxis(2)) &&
		TEST_OVERLAP(one.GetAxis(2) % two.GetAxis(0)) &&
		TEST_OVERLAP(one.GetAxis(2) % two.GetAxis(1)) &&
		TEST_OVERLAP(one.GetAxis(2) % two.GetAxis(2))

		);
}
#undef TEST_OVERLAP

bool IntersectionTests::BoxAndHalfSpace(const CollisionBox& box, const CollisionPlane &plane)
{
	// Work out the projected radius of the box onto the plane direction
	double projectedRadius = TransformToAxis(box, plane.direction);

	// Work out how far the box is from the origin
	double boxDistance =
		plane.direction * box.GetAxis(3) - projectedRadius;

	// Check for the intersection
	return boxDistance <= plane.offset;
}

unsigned CollisionDetector::SphereAndTruePlane(const CollisionSphere& sphere, const CollisionPlane& plane, CollisionData* data)
{
	// Make sure we have contacts
	if (data->contactsLeft <= 0) return 0;

	// Cache the sphere position
	Vector3 position = sphere.GetAxis(3);

	// Find the distance from the plane
	double centreDistance = plane.direction * position - plane.offset;

	// Check if we're within radius
	if (centreDistance * centreDistance > sphere.radius * sphere.radius)
	{
		return 0;
	}

	// Check which side of the plane we're on
	Vector3 normal = plane.direction;
	double penetration = -centreDistance;
	if (centreDistance < 0)
	{
		normal *= -1;
		penetration = -penetration;
	}
	penetration += sphere.radius;

	// Create the contact - it has a normal in the plane direction
	Contact* contact = data->contacts;
	contact->contactNormal = normal;
	contact->penetration = penetration;
	contact->contactPoint = position - plane.direction * centreDistance;
	contact->SetBodyData(sphere.body, NULL, data->friction, data->restitution);

	data->AddContacts(1);
	return 1;
}

unsigned CollisionDetector::SphereAndHalfSpace(const CollisionSphere& sphere, const CollisionPlane& plane, CollisionData* data)
{
	// Make sure we have contacts
	if (data->contactsLeft <= 0) return 0;

	// Cache the sphere position
	Vector3 position = sphere.GetAxis(3);

	// Find the distance from the plane
	double ballDistance = plane.direction * position - sphere.radius - plane.offset;

	if (ballDistance >= 0) return 0;

	// Create the contact - it has a normal in the plane direction
	Contact* contact = data->contacts;
	contact->contactNormal = plane.direction;
	contact->penetration = -ballDistance;
	contact->contactPoint = position - plane.direction * (ballDistance + sphere.radius);
	contact->SetBodyData(sphere.body, NULL, data->friction, data->restitution);

	data->AddContacts(1);
	return 1;
}

