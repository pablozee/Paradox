#pragma once

#include "../ParadoxMath.h"

using namespace DirectX;

class Rigidbody
{
public:
	void ClearAccumulators();
	void CalculateDerivedData();
	void Integrate(double duration);
	
protected:
	double inverseMass;
	XMFLOAT3X3 inverseInertiaTensor;
	double linearDamping;
	double angularDamping;
	XMFLOAT3 position;
	Quaternion orientation;
	XMFLOAT3 velocity;
	XMFLOAT3 rotation;
	
	// Derived Data:
	XMFLOAT3X3 inverseInertiaTensorWorld;
	double motion;
	bool isAwake;
	bool canSleep;
	XMFLOAT4X4 transformationMatrix;

	// Force and Torque Accumulators:
	XMFLOAT3 forceAccum;
	XMFLOAT3 torqueAccum;
	XMFLOAT3 acceleration;
	XMFLOAT3 lastFrameAcceleration;

public:
};