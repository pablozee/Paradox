#pragma once

#include "body.h"
#include <vector>

/**
 * @file
 * 
 * This file contains the interface and sample force generators
 */

/**
 * A force generator can be asked to add force to one or more bodies.
 */
class ForceGenerator
{
public:
	
	/**
	 * Overload this in implementations of the interface to calculate
	 * and update the force applied to the given rigid body
	 */

	virtual void updateForce(RigidBody* body, double duration) = 0;
};

/**
 * A force generator that applies a gravitational force. 
 * One instance can be used for multiple rigid bodies.
 */
class Gravity : public ForceGenerator
{
	/**
	 * Holds the acceleration due to gravity
	 */
	Vector3 gravity;
	
public:

	// Creates the generator with the given acceleration
	Gravity(const Vector3& gravity);

	// Applies the gravitational force to the given rigid body
	virtual void updateForce(RigidBody* body, double duration);
};

/**
 * A force generator that applies a Spring force
 */
class Spring : public ForceGenerator
{
	/**
	 * The point of connection of the spring, in local coordinates
	 */
	Vector3 connectionPoint;

	/**
	 * The point of connection of the spring to the other object,
	 * in that objects' local coordinates.
	 */
	Vector3 otherConnectionPoint;

	// The particle at the other end of the spring.
	RigidBody* other;

	// Holds the spring constant
	double springConstant;

	// Holds the rest length of the spring
	double restLength;

public:

	// Creates a new spring with the given parameters
	Spring(const Vector3 &localConnectionPt,
		   RigidBody *other,
		   const Vector3 &otherConnectionPt,
		   double springConstant,
		   double restLength);

	// Applies the spring force to the given rigid body
	virtual void updateForce(RigidBody* body, double duration);

};

/**
 * A force generator showing a three component explosion effect.
 * This force generator is intended to represent a single explosion
 * effect for multiple rigid bodies. This force generator can also
 * act as a particle force generator.
 */

class Explosion : public ForceGenerator /* public ParticleForceGenerator */
{
	/**
	 * Tracks how long the explosion has been in operation,
	 * used for time-sensitive effects.
	 */
	double timePassed;

public:
	
	/**
	 * Properties of the explosion, these are public because 
	 * there are so many and providing a suitable constructor
	 * would be cumbersome.
	 */

	// The location of the detonation of the weapon
	Vector3 detonation;

	/**
	 * The radius up to which objects implode in the first
	 * stage of the explosion
	 */
	double implosionMaxRadius;

	/**
	 * The radius within which objects don't feel the implosion force.
	 * Objects near to the detonation aren't sucked in by the air implosion.
	 */
	double implosionMinRadius;

	/**
	 * The length of time that objects spend imploding before the
	 * concussion phase kicks in
	 */
	double implosionDuration;

	/**
	 * The maximal force that the implosion can apply. This should be 
	 * relatively small to avoid the implosion pulling objects through
	 * the detonation point and out the other side before the concussion
	 * wave kicks in.
	 */
	double implosionForce;

	/**
	 * The speed that the shock wave is travelling. This is related
	 * to the thickness in the following relationship:
	 * 
	 * thickness >= speed * minimum frame duration
	 */
	double shockwaveSpeed;

	/**
	 * The shock wave applies its force over a range of distances, this
	 * controls how thick. Faster faces require larger thicknesses
	 */
	double shockwaveThickness;

	/**
	 * This is the force that is applied at the very centre of the
	 * concussion wave on an object that is stationary. Objects that
	 * are in front or behind of the wavefront, or that are already
	 * moving outwards, get proportionally less force.
	 * Objects moving towards the centre get proportionally more force.
	 */
	double peakConcussionForce;

	/**
	 * The length of time that the concussion wave is active.
	 * As the wave nears this, the forces it applies reduces.
	 */
	double concussionDuration;

	/**
	 * This is the peak force for stationary objects in the centre of the
	 * chimney. Force calculations for this value are the same as for
	 * peakConcussionForce.
	 */
	double peakConvectionForce;

	// The radius of the chimney cylinder in the xz plane.
	double chimneyRadius;

	// The maximum height of the chimney.
	double chimneyHeight;

	/**
	 * The length of time the convection chimney is active. Typically
	 * this is the longest effect to be in operation, as the heat from
	 * the explosion outlives the shock wave and implosion itself.
	 */
	double convectionDuration;

public:

	/**
	 * Creates a new explosion with sensible default values.
	 */
	Explosion();

	/**
	 * Calculates and applies the force that the explosion has 
	 * on the given rigid body.
	 */
	virtual void updateForce(RigidBody* body, double duration);

	/**
	 * Calculates and applies the force that the explosion has on
	 * the given particle.
	 */

	// virtual void updateForce(Particle *particle, double duration) = 0;
};