#include "Body.h"
#include <memory.h>
#include <assert.h>

/* Helper functions from Cyclone Physics System part of the book How To Build a Robust Game Engine
/**
 * Internal function to do an intertia tensor transform by a quaternion.
 * Note that the implementation of this function was created by an
 * automated code-generator and optimizer.
 */
static inline void _transformInertiaTensor(Matrix3& iitWorld,
    const Quaternion& q,
    const Matrix3& iitBody,
    const Matrix4& rotmat)
{
    double t4 = rotmat.data[0] * iitBody.data[0] +
        rotmat.data[1] * iitBody.data[3] +
        rotmat.data[2] * iitBody.data[6];
    double t9 = rotmat.data[0] * iitBody.data[1] +
        rotmat.data[1] * iitBody.data[4] +
        rotmat.data[2] * iitBody.data[7];
    double t14 = rotmat.data[0] * iitBody.data[2] +
        rotmat.data[1] * iitBody.data[5] +
        rotmat.data[2] * iitBody.data[8];
    double t28 = rotmat.data[4] * iitBody.data[0] +
        rotmat.data[5] * iitBody.data[3] +
        rotmat.data[6] * iitBody.data[6];
    double t33 = rotmat.data[4] * iitBody.data[1] +
        rotmat.data[5] * iitBody.data[4] +
        rotmat.data[6] * iitBody.data[7];
    double t38 = rotmat.data[4] * iitBody.data[2] +
        rotmat.data[5] * iitBody.data[5] +
        rotmat.data[6] * iitBody.data[8];
    double t52 = rotmat.data[8] * iitBody.data[0] +
        rotmat.data[9] * iitBody.data[3] +
        rotmat.data[10] * iitBody.data[6];
    double t57 = rotmat.data[8] * iitBody.data[1] +
        rotmat.data[9] * iitBody.data[4] +
        rotmat.data[10] * iitBody.data[7];
    double t62 = rotmat.data[8] * iitBody.data[2] +
        rotmat.data[9] * iitBody.data[5] +
        rotmat.data[10] * iitBody.data[8];

    iitWorld.data[0] = t4 * rotmat.data[0] +
        t9 * rotmat.data[1] +
        t14 * rotmat.data[2];
    iitWorld.data[1] = t4 * rotmat.data[4] +
        t9 * rotmat.data[5] +
        t14 * rotmat.data[6];
    iitWorld.data[2] = t4 * rotmat.data[8] +
        t9 * rotmat.data[9] +
        t14 * rotmat.data[10];
    iitWorld.data[3] = t28 * rotmat.data[0] +
        t33 * rotmat.data[1] +
        t38 * rotmat.data[2];
    iitWorld.data[4] = t28 * rotmat.data[4] +
        t33 * rotmat.data[5] +
        t38 * rotmat.data[6];
    iitWorld.data[5] = t28 * rotmat.data[8] +
        t33 * rotmat.data[9] +
        t38 * rotmat.data[10];
    iitWorld.data[6] = t52 * rotmat.data[0] +
        t57 * rotmat.data[1] +
        t62 * rotmat.data[2];
    iitWorld.data[7] = t52 * rotmat.data[4] +
        t57 * rotmat.data[5] +
        t62 * rotmat.data[6];
    iitWorld.data[8] = t52 * rotmat.data[8] +
        t57 * rotmat.data[9] +
        t62 * rotmat.data[10];
}

/**
 * Inline function that creates a transform matrix from a
 * position and orientation.
 */
static inline void _calculateTransformMatrix(Matrix4& transformMatrix,
    const Vector3& position,
    const Quaternion& orientation)
{
    transformMatrix.data[0] = 1 - 2 * orientation.j * orientation.j -
        2 * orientation.k * orientation.k;
    transformMatrix.data[1] = 2 * orientation.i * orientation.j -
        2 * orientation.r * orientation.k;
    transformMatrix.data[2] = 2 * orientation.i * orientation.k +
        2 * orientation.r * orientation.j;
    transformMatrix.data[3] = position.x;

    transformMatrix.data[4] = 2 * orientation.i * orientation.j +
        2 * orientation.r * orientation.k;
    transformMatrix.data[5] = 1 - 2 * orientation.i * orientation.i -
        2 * orientation.k * orientation.k;
    transformMatrix.data[6] = 2 * orientation.j * orientation.k -
        2 * orientation.r * orientation.i;
    transformMatrix.data[7] = position.y;

    transformMatrix.data[8] = 2 * orientation.i * orientation.k -
        2 * orientation.r * orientation.j;
    transformMatrix.data[9] = 2 * orientation.j * orientation.k +
        2 * orientation.r * orientation.i;
    transformMatrix.data[10] = 1 - 2 * orientation.i * orientation.i -
        2 * orientation.j * orientation.j;
    transformMatrix.data[11] = position.z;
}

void RigidBody::CalculateDerivedData()
{
    orientation.Normalize();

    // Calculate transform matrix for the body
    _calculateTransformMatrix(transformationMatrix, position, orientation);

    // Calculate inertia tensor in world space
    _transformInertiaTensor(inverseInertiaTensorWorld, orientation, inverseInertiaTensor, transformationMatrix);
};

void RigidBody::Integrate(double duration)
{
    if (!isAwake) return;

    // Calculate linear acceleration from force inputs
    lastFrameAcceleration = acceleration;
    lastFrameAcceleration.addScaledVector(forceAccum, inverseMass);

    // Calculate angular acceleration from torque inputs
    Vector3 angularAcceleration = inverseInertiaTensorWorld.transform(torqueAccum);

    // Adjust velocities
    // Update linear velocity from both acceleration and impulse
    velocity.addScaledVector(lastFrameAcceleration, duration);

    // Update angular velocity from angular acceleration and impulse
    rotation.addScaledVector(angularAcceleration, duration);

    // Impose drag
    velocity *= pow(linearDamping, duration);
    rotation *= pow(angularDamping, duration);

    // Adjust positions
    // Update linear position
    position.addScaledVector(velocity, duration);

    // Update angular position
    orientation.AddScaledVector(rotation, duration);

    // Normalize orientation and update matrices with 
    // new position and orientation

    CalculateDerivedData();

    ClearAccumulators();

    // Update the kinetic energy store, and possibly
    // put the body to sleep
    if (canSleep)
    {
        double currentMotion = velocity.scalarProduct(velocity) + rotation.scalarProduct(rotation);

        double bias = pow(0.5, duration);
        motion = bias * motion + (1 - bias) * currentMotion;

        if (motion < sleepEpsilon) SetAwakeStatus(false);
        else if (motion > 10 * sleepEpsilon) motion = 10 * sleepEpsilon;
    }
};

void RigidBody::SetMass(const double mass)
{
    assert(mass != 0);
    RigidBody::inverseMass = ((double)1.0 / inverseMass);
};

double RigidBody::GetMass() const
{
    if (inverseMass == 0)
    {
        return INFINITY;
    }
    else
    {
        return ((double)1.0 / inverseMass);
    }
};

void RigidBody::SetInverseMass(const double inverseMass)
{
    RigidBody::inverseMass = inverseMass;
};

double RigidBody::GetInverseMass() const
{
    return inverseMass;
};

bool RigidBody::HasFiniteMass() const
{
    return inverseMass >= 0.0f;
};

void RigidBody::SetInertiaTensor(const Matrix3& inertiaTensor)
{
    inverseInertiaTensor.setInverse(inertiaTensor);
};

void RigidBody::GetInertiaTensor(Matrix3* inertiaTensor) const
{
    inertiaTensor->setInverse(inverseInertiaTensor);
};

Matrix3 RigidBody::GetInertiaTensor() const
{
    Matrix3 inertiaTensor;
    GetInertiaTensor(&inertiaTensor);
    return inertiaTensor;
};

void RigidBody::GetInertiaTensorWorld(Matrix3* inertiaTensor) const
{
    inertiaTensor->setInverse(inverseInertiaTensorWorld);
};

Matrix3 RigidBody::GetInertiaTensorWorld() const
{
    Matrix3 inertiaTensor;
    GetInertiaTensorWorld(&inertiaTensor);
    return inertiaTensor;
};

void RigidBody::SetInverseInertiaTensor(const Matrix3& inverseInertiaTensor)
{
    RigidBody::inverseInertiaTensor = inverseInertiaTensor;
};

void RigidBody::GetInverseInertiaTensor(Matrix3* inverseInertiaTensor) const
{
    *inverseInertiaTensor = RigidBody::inverseInertiaTensor;
};

Matrix3 RigidBody::GetInverseInertiaTensor() const
{
    return inverseInertiaTensor;
};

void RigidBody::GetInverseInertiaTensorWorld(Matrix3* inverseInertiaTensor) const
{
    *inverseInertiaTensor = inverseInertiaTensorWorld;
};

Matrix3 RigidBody::GetInverseInertiaTensorWorld() const
{
    return inverseInertiaTensorWorld;
};


void RigidBody::SetDamping(const double linearDamping, const double angularDamping)
{
    RigidBody::linearDamping = linearDamping;
    RigidBody::angularDamping = angularDamping;
};

void RigidBody::SetLinearDamping(const double linearDamping)
{
    RigidBody::linearDamping = linearDamping;
};

double RigidBody::GetLinearDamping() const
{
    return linearDamping;
};

void RigidBody::SetAngularDamping(const double angularDamping)
{
    RigidBody::angularDamping = angularDamping;
};

double RigidBody::GetAngularDamping() const
{
    return angularDamping;
};

void RigidBody::SetPosition(const Vector3& position)
{
    RigidBody::position = position;
};

void RigidBody::SetPosition(const double x, const double y, const double z)
{
    position.x = x;
    position.y = y;
    position.z = z;
};

void RigidBody::GetPosition(Vector3* position) const
{
    *position = RigidBody::position;
};

Vector3 RigidBody::GetPosition() const
{
    return position;
};

void RigidBody::SetOrientation(const Quaternion& orientation)
{
    RigidBody::orientation = orientation;
    RigidBody::orientation.Normalize();
};

void RigidBody::SetOrientation(const double r, const double i, const double j, const double k)
{
    orientation.r = r;
    orientation.i = i;
    orientation.j = j;
    orientation.k = k;
    orientation.Normalize();
};

void RigidBody::GetOrientation(Quaternion* orientation) const
{
    *orientation = RigidBody::orientation;
};

Quaternion RigidBody::GetOrientation() const
{
    return orientation;
};

void RigidBody::GetOrientation(Matrix3* matrix) const
{
    GetOrientation(matrix->data);
};

void RigidBody::GetOrientation(double matrix[9]) const
{
    matrix[0] = transformationMatrix.data[0];
    matrix[1] = transformationMatrix.data[1];
    matrix[2] = transformationMatrix.data[2];

    matrix[3] = transformationMatrix.data[4];
    matrix[4] = transformationMatrix.data[5];
    matrix[5] = transformationMatrix.data[6];

    matrix[6] = transformationMatrix.data[8];
    matrix[7] = transformationMatrix.data[9];
    matrix[8] = transformationMatrix.data[10];
};

void RigidBody::GetTransform(Matrix4* transform) const
{
    memcpy(transform, &transformationMatrix.data, sizeof(Matrix4));
};

void RigidBody::GetTransform(double matrix[16]) const
{
    memcpy(matrix, &transformationMatrix.data, sizeof(double) * 12);
    matrix[12] = matrix[13] = matrix[14] = 0;
    matrix[15] = 1;
};

Matrix4 RigidBody::GetTransform() const
{
    return transformationMatrix;
};

Vector3 RigidBody::GetPointInLocalSpace(const Vector3& point) const
{
    return transformationMatrix.transformInverse(point);
};

Vector3 RigidBody::GetPointInWorldSpace(const Vector3& point) const
{
    return transformationMatrix.transform(point);
};

Vector3 RigidBody::GetDirectionInLocalSpace(const Vector3& direction) const
{
    return transformationMatrix.transformInverseDirection(direction);
};

Vector3 RigidBody::GetDirectionInWorldSpace(const Vector3& direction) const
{
    return transformationMatrix.transformDirection(direction);
};

void RigidBody::SetVelocity(const Vector3& velocity)
{
    RigidBody::velocity = velocity;
};

void RigidBody::SetVelocity(const double x, const double y, const double z)
{
    velocity.x = x;
    velocity.y = y;
    velocity.z = z;
};

void RigidBody::GetVelocity(Vector3* velocity) const
{
    *velocity = RigidBody::velocity;
};

Vector3 RigidBody::GetVelocity() const
{
    return velocity;
};

void RigidBody::AddVelocity(const Vector3& deltaVelocity)
{
    velocity += deltaVelocity;
};
void RigidBody::SetRotation(const Vector3& rotation)
{
    RigidBody::rotation = rotation;
};

void RigidBody::SetRotation(const double x, const double y, const double z)
{
    rotation.x = x;
    rotation.y = y;
    rotation.z = z;
};

void RigidBody::GetRotation(Vector3* rotation)
{
    *rotation = RigidBody::rotation;
};

Vector3 RigidBody::GetRotation() const
{
    return rotation;
};

void RigidBody::AddRotation(const Vector3& deltaRotation)
{
    rotation += deltaRotation;
};

void RigidBody::SetAwakeStatus(const bool awake)
{
    if (awake)
    {
        isAwake = true;

        // Add some motion to stop RB falling asleep immediately
        motion = sleepEpsilon * 2.0f;
    }
    else
    {
        isAwake = false;
        velocity.clear();
        rotation.clear();
    }
};

void RigidBody::SetCanSleep(const bool canSleep)
{
    RigidBody::canSleep = canSleep;

    if (!canSleep && !isAwake) SetAwakeStatus();
}

void RigidBody::GetLastFrameAcceleration(Vector3* acceleration) const
{
    *acceleration = lastFrameAcceleration;
}

Vector3 RigidBody::GetLastFrameAcceleration() const
{
    return acceleration;
}

void RigidBody::ClearAccumulators()
{
    forceAccum.clear();
    torqueAccum.clear();
}

void RigidBody::AddForce(const Vector3& force)
{
    forceAccum += force;
    isAwake = true;
}

void RigidBody::AddForceAtBodyPoint(const Vector3& force, const Vector3& point)
{
    // Convert to coordinates to world space
    Vector3 wsPoint = GetPointInWorldSpace(point);
    AddForceAtPoint(force, point);
}

void RigidBody::AddForceAtPoint(const Vector3& force, const Vector3& point)
{
    // Convert to coordinates relative to center of mass
    Vector3 pt = point;
    pt -= position;

    forceAccum += force;
    torqueAccum += pt % force;

    isAwake = true;
}

void RigidBody::AddTorque(const Vector3& torque)
{
    torqueAccum += torque;
    isAwake = true;
}

void RigidBody::SetAcceleration(const Vector3& acceleration)
{
    RigidBody::acceleration = acceleration;
}

void RigidBody::SetAcceleration(const double x, const double y, const double z)
{
    acceleration.x = x;
    acceleration.y = y;
    acceleration.z = z;
}

void RigidBody::GetAcceleration(Vector3* acceleration) const
{
    *acceleration = RigidBody::acceleration;
}

Vector3 RigidBody::GetAcceleration() const
{
    return acceleration;
}