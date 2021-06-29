#include "CollideCoarse.h"

BoundingSphereVolume::BoundingSphereVolume(const Vector3& centre, double radius)
{
	BoundingSphereVolume::centre = centre;
	BoundingSphereVolume::radius = radius;
}

BoundingSphereVolume::BoundingSphereVolume(const BoundingSphereVolume& one, const BoundingSphereVolume& two)
{
	Vector3 centreOffset = two.centre - one.centre;
	double distance = centreOffset.squareMagnitude();
	double radiusDiff = two.radius - one.radius;

	// Check if the larger sphere encloses the small one
	if (radiusDiff * radiusDiff >= distance)
	{
		if (one.radius > two.radius)
		{
			centre = one.centre;
			radius = one.radius;
		}
		else
		{
			centre = two.centre;
			radius = two.radius;
		}
	}

	// Otherwise we need to work with partially 
	// overlapping spheres
	else
	{
		distance = sqrt(distance);
		radius = (distance + one.radius + two.radius * ((double)0.5));

		// The new centre is based on one's centre, moved toward
		// two's centre by an amount proportional to the spheres'
		// radii
		centre = one.centre;
		if (distance > 0)
		{
			centre += centreOffset * ((radius - one.radius) / distance);
		}
	}
}

bool BoundingSphereVolume::Overlaps(const BoundingSphereVolume* other) const
{
	double distanceSquared = (centre - other->centre).squareMagnitude();
	return distanceSquared < (radius + other->radius)* (radius + other->radius);
}

double BoundingSphereVolume::GetGrowth(const BoundingSphereVolume& other) const
{
	BoundingSphereVolume newSphere(*this, other);

	// We return a value proportional to the change in surface area of the sphere.
	return newSphere.radius * newSphere.radius - radius * radius;
}

