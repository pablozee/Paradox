#include "body.h"
#include <memory.h>
#include <assert.h>

/* Helper functions from Cyclone Physics System part of the book How To Build a Robust Game Engine
 * --------------------------------------------------------------------------
 * INTERNAL OR HELPER FUNCTIONS:
 * --------------------------------------------------------------------------
 */

/**
 * Internal function to do an intertia tensor transform by a quaternion.
 * Note that the implementation of this function was created by an
 * automated code-generator and optimizer.
 */

static inline void _transformInertiaTensor(XMFLOAT3X3& iitWorld,
    const Quaternion& q,
    const XMFLOAT3X3& iitBody,
    const XMFLOAT4X4& rotmat)
{
    double t4 = rotmat._11 * iitBody._11 +
        rotmat._12 * iitBody._21 +
        rotmat._13 * iitBody._31;
    double t9 = rotmat._11 * iitBody._12 +
        rotmat._12 * iitBody._22 +
        rotmat._13 * iitBody._32;
    double t14 = rotmat._11 * iitBody._13 +
        rotmat._12 * iitBody._23 +
        rotmat._13 * iitBody._33;
    double t28 = rotmat._21 * iitBody._11 +
        rotmat._22 * iitBody._21 +
        rotmat._23 * iitBody._31;
    double t33 = rotmat._21 * iitBody._12 +
        rotmat._22 * iitBody._22 +
        rotmat._23 * iitBody._32;
    double t38 = rotmat._21 * iitBody._13 +
        rotmat._22 * iitBody._23 +
        rotmat._23 * iitBody._33;
    double t52 = rotmat._31 * iitBody._11 +
        rotmat._32 * iitBody._21 +
        rotmat._33 * iitBody._31;
    double t57 = rotmat._31 * iitBody._12 +
        rotmat._32 * iitBody._22 +
        rotmat._33 * iitBody._32;
    double t62 = rotmat._31 * iitBody._13 +
        rotmat._32 * iitBody._23 +
        rotmat._33 * iitBody._33;

    iitWorld._11 = t4 * rotmat._11 +
        t9 * rotmat._12 +
        t14 * rotmat._13;
    iitWorld._12 = t4 * rotmat._21 +
        t9 * rotmat._22 +
        t14 * rotmat._23;
    iitWorld._13 = t4 * rotmat._31 +
        t9 * rotmat._32 +
        t14 * rotmat._33;
    iitWorld._21 = t28 * rotmat._11 +
        t33 * rotmat._12 +
        t38 * rotmat._13;
    iitWorld._22 = t28 * rotmat._21 +
        t33 * rotmat._22 +
        t38 * rotmat._23;
    iitWorld._23 = t28 * rotmat._31 +
        t33 * rotmat._32 +
        t38 * rotmat._33;
    iitWorld._31 = t52 * rotmat._11 +
        t57 * rotmat._12 +
        t62 * rotmat._13;
    iitWorld._32 = t52 * rotmat._21 +
        t57 * rotmat._22 +
        t62 * rotmat._23;
    iitWorld._33 = t52 * rotmat._31 +
        t57 * rotmat._32 +
        t62 * rotmat._33;
}

/**
 * Inline function that creates a transform matrix from a
 * position and orientation.
 */
static inline void _calculateTransformMatrix(XMFLOAT4X4& transformMatrix,
    const XMFLOAT3& position,
    const Quaternion& orientation)
{
    transformMatrix._11 = 1 - 2 * orientation.j * orientation.j -
        2 * orientation.k * orientation.k;
    transformMatrix._12 = 2 * orientation.i * orientation.j -
        2 * orientation.r * orientation.k;
    transformMatrix._13 = 2 * orientation.i * orientation.k +
        2 * orientation.r * orientation.j;
    transformMatrix._14 = position.x;

    transformMatrix._21 = 2 * orientation.i * orientation.j +
        2 * orientation.r * orientation.k;
    transformMatrix._22 = 1 - 2 * orientation.i * orientation.i -
        2 * orientation.k * orientation.k;
    transformMatrix._23 = 2 * orientation.j * orientation.k -
        2 * orientation.r * orientation.i;
    transformMatrix._24 = position.y;

    transformMatrix._31 = 2 * orientation.i * orientation.k -
        2 * orientation.r * orientation.j;
    transformMatrix._32 = 2 * orientation.j * orientation.k +
        2 * orientation.r * orientation.i;
    transformMatrix._33 = 1 - 2 * orientation.i * orientation.i -
        2 * orientation.j * orientation.j;
    transformMatrix._34 = position.z;
}

void Rigidbody::ClearAccumulators()
{
};

void Rigidbody::CalculateDerivedData()
{

};

void Rigidbody::ClearAccumulators()
{

};

void Rigidbody::CalculateDerivedData()
{

};

void Rigidbody::CalculateDerivedData()
{

};

void Rigidbody::ClearAccumulators()
{

};

void Rigidbody::CalculateDerivedData()
{

};

void Rigidbody::CalculateDerivedData()
{

};

void Rigidbody::ClearAccumulators()
{

};

void Rigidbody::CalculateDerivedData()
{

};

void Rigidbody::CalculateDerivedData()
{

};

void Rigidbody::ClearAccumulators()
{

};

void Rigidbody::CalculateDerivedData()
{

};

void Rigidbody::CalculateDerivedData()
{

};

void Rigidbody::ClearAccumulators()
{

};

void Rigidbody::CalculateDerivedData()
{

};

void Rigidbody::CalculateDerivedData()
{

};

void Rigidbody::ClearAccumulators()
{

};

void Rigidbody::CalculateDerivedData()
{

};

void Rigidbody::CalculateDerivedData()
{

};

void Rigidbody::ClearAccumulators()
{

};

void Rigidbody::CalculateDerivedData()
{

};

void Rigidbody::CalculateDerivedData()
{

};

void Rigidbody::ClearAccumulators()
{

};

void Rigidbody::CalculateDerivedData()
{

};

void Rigidbody::CalculateDerivedData()
{

};

void Rigidbody::ClearAccumulators()
{

};

void Rigidbody::CalculateDerivedData()
{

};

void Rigidbody::CalculateDerivedData()
{

};

void Rigidbody::ClearAccumulators()
{

};

void Rigidbody::CalculateDerivedData()
{

};

void Rigidbody::CalculateDerivedData()
{

};

void Rigidbody::ClearAccumulators()
{

};

void Rigidbody::CalculateDerivedData()
{

};
