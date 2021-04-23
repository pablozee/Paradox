#pragma once

#include <DirectXMath.h>

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

};