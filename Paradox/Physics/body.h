#pragma once

class Rigidbody
{
public:
	void ClearAccumulators();
	void CalculateDerivedData();
	void Integrate(float duration);
};