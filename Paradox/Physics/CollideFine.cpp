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

unsigned CollisionDetector::SphereAndSphere(const CollisionSphere& one, const CollisionSphere& two, CollisionData* data)
{
	// Make sure we have contacts
	if (data->contactsLeft <= 0) return 0;

	// Cache the sphere positions
	Vector3 positionOne = one.GetAxis(3);
	Vector3 positionTwo = two.GetAxis(3);

	// Find the vector between the objects
	Vector3 midline = positionOne - positionTwo;
	double size = midline.magnitude();

	// See if it is large enough
	if (size <= 0.0f || size >= one.radius + two.radius)
	{
		return 0;
	}

	// We manually create the normal, because we have
	// the size to hand.
	Vector3 normal = midline * ((double)1.0 / size);

	Contact* contact = data->contacts;
	contact->contactNormal = normal;
	contact->contactPoint = positionOne + midline * (double)0.5;
	contact->penetration = (one.radius + two.radius - size);
	contact->SetBodyData(one.body, two.body, data->friction, data->restitution);

	data->AddContacts(1);
	return 1;
}

/**
 * This function checks if the two boxes overlap
 * along the given axis, returning the amount of overlap.
 * The final parameter toCentre is used to pass in the
 * vector between the boxes centre points, to avoid having
 * to recalculate it each time.
 */
static inline double PenetrationOnAxis(
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

	// Return the overlap (i.e. positive indicates
	// overlap, negative indicates separation)
	return oneProject + twoProject - distance;
}

static inline bool TryAxis(
	const CollisionBox& one,
	const CollisionBox& two,
	Vector3 axis,
	const Vector3& toCentre,
	unsigned index,
	double& smallestPenetration,
	unsigned& smallestCase
)
{
	// Make sure we have a normalized axis and don't check
	// almost parallel axes
	if (axis.squareMagnitude() < 0.0001) return true;
	axis.normalise();

	double penetration = PenetrationOnAxis(one, two, axis, toCentre);

	if (penetration < 0) return false;
	if (penetration < smallestPenetration)
	{
		smallestPenetration = penetration;
		smallestCase = index;
	}
	return true;
}

void FillPointFaceBoxBox(
	const CollisionBox& one,
	const CollisionBox& two,
	const Vector3& toCentre,
	CollisionData* data,
	unsigned best,
	double penetration
)
{
	/**
	 * This method is called when we know that a vertex from
	 * box two is in contact with box one
	 */
	Contact* contact = data->contacts;

	/**
	 * We know which axis the collision is on (i.e. best),
	 * but we need to work out which of the two faces on
	 * this axis.
	 */
	Vector3 normal = one.GetAxis(best);
	if (one.GetAxis(best) * toCentre > 0)
	{
		normal = normal * -1.0f;
	}

	/**
	 * Work out which vertex of box two we're colliding with.
	 * Using toCentre doesn't work.
	 */
	Vector3 vertex = two.halfSize;
	if (two.GetAxis(0) * normal < 0) vertex.x = -vertex.x;
	if (two.GetAxis(1) * normal < 0) vertex.y = -vertex.y;
	if (two.GetAxis(2) * normal < 0) vertex.z = -vertex.z;

	// Create the contact data
	contact->contactNormal = normal;
	contact->penetration = penetration;
	contact->contactPoint = two.GetTransform() * vertex;
	contact->SetBodyData(one.body, two.body, data->friction, data->restitution);
}

static inline Vector3 ContactPoint(
	const Vector3& pOne,
	const Vector3& dOne,
	double oneSize,
	const Vector3& pTwo,
	const Vector3& dTwo,
	double twoSize,

	/**
	 * If this is true and the contact point is outside the edge
	 * (in the case of an edge-face contact) then we use ones'
	 * midpoint, otherwise we use twos'.
	 */
	bool useOne
)
{
	Vector3 toSt, cOne, cTwo;
	real dpStaOne, dpStaTwo, dpOneTwo, smOne, smTwo;
	real denom, mua, mub;

	smOne = dOne.squareMagnitude();
	smTwo = dTwo.squareMagnitude();
	dpOneTwo = dTwo * dOne;

	toSt = pOne - pTwo;
	dpStaOne = dOne * toSt;
	dpStaTwo = dTwo * toSt;

	denom = smOne * smTwo - dpOneTwo * dpOneTwo;

	// Zero denominator indicates parrallel lines
	if (real_abs(denom) < 0.0001f) {
		return useOne ? pOne : pTwo;
	}

	mua = (dpOneTwo * dpStaTwo - smTwo * dpStaOne) / denom;
	mub = (smOne * dpStaTwo - dpOneTwo * dpStaOne) / denom;

	// If either of the edges has the nearest point out
	// of bounds, then the edges aren't crossed, we have
	// an edge-face contact. Our point is on the edge, which
	// we know from the useOne parameter.
	if (mua > oneSize ||
		mua < -oneSize ||
		mub > twoSize ||
		mub < -twoSize)
	{
		return useOne ? pOne : pTwo;
	}
	else
	{
		cOne = pOne + dOne * mua;
		cTwo = pTwo + dTwo * mub;

		return cOne * 0.5 + cTwo * 0.5;
	}
}

// This preprocessor definition is only used as a convenience 
// in the BoxAndBox contact generation method.
#define CHECK_OVERLAP(axis, index) \
	if (!TryAxis(one, two, (axis), toCentre, (index), penetration, best)) return 0;

unsigned CollisionDetector::BoxAndBox(const CollisionBox& one, const CollisionBox& two, CollisionData* data)
{
	// if (!IntersectionTests::BoxAndBox(one, two)) return 0;

	// Find the vector between the two centres
	Vector3 toCentre = two.GetAxis(3) - one.GetAxis(3);

	// We start assuming there is no contact
	double penetration = INFINITY;
	unsigned best = 0xffffff;

	// Now we check each axes, returning if it gives us
   // a separating axis, and keeping track of the axis with
   // the smallest penetration otherwise.
	CHECK_OVERLAP(one.GetAxis(0), 0);
	CHECK_OVERLAP(one.GetAxis(1), 1);
	CHECK_OVERLAP(one.GetAxis(2), 2);

	CHECK_OVERLAP(two.GetAxis(0), 3);
	CHECK_OVERLAP(two.GetAxis(1), 4);
	CHECK_OVERLAP(two.GetAxis(2), 5);

	// Store the best axis-major, in case we run into almost
	// parallel edge collisions later
	unsigned bestSingleAxis = best;

	CHECK_OVERLAP(one.GetAxis(0) % two.GetAxis(0), 6);
	CHECK_OVERLAP(one.GetAxis(0) % two.GetAxis(1), 7);
	CHECK_OVERLAP(one.GetAxis(0) % two.GetAxis(2), 8);
	CHECK_OVERLAP(one.GetAxis(1) % two.GetAxis(0), 9);
	CHECK_OVERLAP(one.GetAxis(1) % two.GetAxis(1), 10);
	CHECK_OVERLAP(one.GetAxis(1) % two.GetAxis(2), 11);
	CHECK_OVERLAP(one.GetAxis(2) % two.GetAxis(0), 12);
	CHECK_OVERLAP(one.GetAxis(2) % two.GetAxis(1), 13);
	CHECK_OVERLAP(one.GetAxis(2) % two.GetAxis(2), 14);

	// Make sure we've got a result
	assert(best != 0xffffff);

	/**
	 * We now know there's a collision and we know which
	 * of the axes gave the smallest penetration. We now
	 * can deal with it in different ways depending on the
	 * case.
	 */
	if (best < 3)
	{
		// We've got a vertex of box two on a face of box one
		FillPointFaceBoxBox(one, two, toCentre, data, best, penetration);
		data->AddContacts(1);
		return 1;
	}
	else if (best < 6)
	{
		/**
		 * We've got a vertex of box one on a face of box two.
		 * We use the same algorithm as above, but swap around
		 * one and two (and therefore also the vector between
		 * their centres).
		 */
		FillPointFaceBoxBox(two, one, toCentre * -1.0f, data, best - 3, penetration);
		data->AddContacts(1);
		return 1;
	}
	else
	{
		// We've got an edge-edge contact. Find out which axes
		best -= 6;
		unsigned oneAxisIndex = best / 3;
		unsigned twoAxisIndex = best % 3;
		Vector3 oneAxis = one.GetAxis(oneAxisIndex);
		Vector3 twoAxis = two.GetAxis(twoAxisIndex);
		Vector3 axis = oneAxis % twoAxis;
		axis.normalise();

		// The axis should point from box one to box two
		if (axis * toCentre > 0) axis = axis * -1.0f;

		/**
		 * We have the axes, but not the edges: each axis has 4 edges
		 * parallel to it, we need to find which of the 4 for each
		 * object. We do that by finding the point in the centre of 
		 * the edge. We know that its' component in the direction of
		 * the box's collision axis is zero (it's a mid-point) and we 
		 * determine which of the extremes in each of the other axes
		 * is closest.
		 */
		Vector3 ptOnOneEdge = one.halfSize;
		Vector3 ptOnTwoEdge = two.halfSize;

		for (unsigned i = 0; i < 3; i++)
		{
			if (i == oneAxisIndex) ptOnOneEdge[i] = 0;
			else if (one.GetAxis(i) * axis > 0) ptOnOneEdge[i] = -ptOnOneEdge[i];

			if (i == twoAxisIndex) ptOnTwoEdge[i] = 0;
			else if (two.GetAxis(i) * axis < 0) ptOnTwoEdge[i] = -ptOnTwoEdge[i];
		}

		// Move them into world coordinates (they are already oriented correctly
		// since they have been derived from the axes).
		ptOnOneEdge = one.transform * ptOnOneEdge;
		ptOnTwoEdge = two.transform * ptOnTwoEdge;

		// So we have a point and a direction for the colliding edges.
		// We need to find out the point of closest approach of the 
		// two line-segments.
		Vector3 vertex = ContactPoint(ptOnOneEdge, oneAxis, one.halfSize[oneAxisIndex],
									  ptOnTwoEdge, twoAxis, two.halfSize[twoAxisIndex],
									  bestSingleAxis > 2);

		// We can fill the contact
		Contact* contact = data->contacts;

		contact->penetration = penetration;
		contact->contactNormal = axis;
		contact->contactPoint = vertex;
		contact->SetBodyData(one.body, two.body, data->friction, data->restitution);
		data->AddContacts(1);
		return 1;
	}
	return 0;
}
#undef CHECK_OVERLAP

unsigned CollisionDetector::BoxAndPoint(const CollisionBox& box, const Vector3& point, CollisionData* data)
{
	// Transform the point into box coordinates
	Vector3 relPt = box.transform.transformInverse(point);

	Vector3 normal;

	/**
	 * Check each axis, looking for the axis on which the penetration
	 * is least deep
	 */
	double min_depth = box.halfSize.x - abs(relPt.x);
	if (min_depth < 0) return 0;
	normal = box.GetAxis(0) * ((relPt.x < 0) ? -1 : 1);

	double depth = box.halfSize.y - abs(relPt.y);
	if (depth < 0) return 0;
	else if (depth < min_depth)
	{
		min_depth = depth;
		normal = box.GetAxis(1) * ((relPt.y < 0) ? -1 : 1);
	}

	depth = box.halfSize.z - abs(relPt.z);
	if (depth < 0) return 0;
	else if (depth < min_depth)
	{
		min_depth = depth;
		normal = box.GetAxis(2) * ((relPt.z < 0) ? -1 : 1);
	}

	// Compile the contact
	Contact* contact = data->contacts;
	contact->contactNormal = normal;
	contact->contactPoint = point;
	contact->penetration = min_depth;

	/**
	 * Note that we don't know what rigid body the point belongs to so we just use
	 * NULL. Where this is called this value can be left or filled in.
	 */
	contact->SetBodyData(box.body, NULL, data->friction, data->restitution);

	data->AddContacts(1);
	return 1;
}

unsigned CollisionDetector::BoxAndSphere(const CollisionBox& box, const CollisionSphere& sphere, CollisionData* data)
{
	// Transform the centre of the sphere into box coordinates
	Vector3 centre = sphere.GetAxis(3);
	Vector3 relCentre = box.GetTransform.transformInverse(centre);

	// Early out check to see if we can exclude the contact
	if (abs(relCentre.x) - sphere.radius > box.halfSize.x ||
		abs(relCentre.y) - sphere.radius > box.halfSize.y ||
		abs(relCentre.z) - sphere.radius > box.halfSize.z)
	{
		return 0;
	}

	Vector3 closestPt(0, 0, 0);
	double dist;

	// Clamp each coordinate to the box
	dist = relCentre.x;
	if (dist > box.halfSize.x) dist = box.halfSize.x;
	if (dist < -box.halfSize.x) dist = -box.halfSize.x;
	closestPt.x = dist;

	dist = relCentre.y;
	if (dist > box.halfSize.y) dist = box.halfSize.y;
	if (dist < -box.halfSize.y) dist = -box.halfSize.y;
	closestPt.y = dist;

	dist = relCentre.z;
	if (dist > box.halfSize.z) dist = box.halfSize.z;
	if (dist < -box.halfSize.z) dist = -box.halfSize.z;
	closestPt.z = dist;

	// Check we're in contact
	dist = (closestPt - relCentre).squareMagnitude();
	if (dist > sphere.radius * sphere.radius) return 0;

	// Compile the contact
	Vector3 closestPtWorld = box.transform.transform(closestPt);

	Contact* contact = data->contacts;
	contact->contactNormal = (closestPtWorld - centre);
	contact->contactNormal.normalise();
	contact->contactPoint = closestPtWorld;
	contact->penetration = sphere.radius - sqrt(dist);
	contact->SetBodyData(box.body, sphere.body, data->friction, data->restitution);

	data->AddContacts(1);
	return 1;
}

unsigned CollisionDetector::BoxAndHalfSpace(const CollisionBox& box, const CollisionPlane& plane, CollisionData* data)
{
	// Make sure we have contacts
	if (data->contactsLeft <= 0) return 0;

	// Check for intersection
	if (!IntersectionTests::BoxAndHalfSpace(box, plane))
	{
		return 0;
	}

	/**
	 * We have an intersection so find the intersection points.
	 * We can make do with only checking vertices. If the box is
	 * resting on a plane or on an edge, it will be reported as four
	 * or two contact points.
	 */

	 // Go through each combination of + and - for each half-size
	static double mults[8][3] = { {1,1,1},{-1,1,1},{1,-1,1},{-1,-1,1},
							   {1,1,-1},{-1,1,-1},{1,-1,-1},{-1,-1,-1} };

	Contact* contact = data->contacts;
	unsigned contactsUsed = 0;
	for (unsigned i = 0; i < 8; i++)
	{
		// Calculate the position of each vertex
		Vector3 vertexPos(mults[i][0], mults[i][1], mults[i][2]);
		vertexPos.componentProductUpdate(box.halfSize);
		vertexPos = box.transform.transform(vertexPos);

		// Calculate the distance from the plane
		double vertexDistance = vertexPos * plane.direction;

		// Compare this to the planes' distance
		if (vertexDistance <= plane.offset)
		{
			// Create the contact data.

			/**
			 * The contact point is halfway between the vertex and the plane.
			 * We multiply the direction by half the separation distance and 
			 * add the vertex location.
			 */
			contact->contactPoint = plane.direction;
			contact->contactPoint *= (vertexDistance - plane.offset);
			contact->contactPoint += vertexPos;
			contact->contactNormal = plane.direction;
			contact->penetration = plane.offset - vertexDistance;

			// Write the appropriate data
			contact->SetBodyData(box.body, NULL, data->friction, data->restitution);

			// Move onto the next contact
			contact++;
			contactsUsed++;
			if (contactsUsed == (unsigned)data->contactsLeft) return contactsUsed;
		}
	}

	data->AddContacts(contactsUsed);
	return contactsUsed;
}