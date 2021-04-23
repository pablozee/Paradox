#pragma once
#include "Graphics/core.h"

using namespace DirectX;

class Quaternion
{
public:
	union 
	{
		struct
		{
			double r;
			double i;
			double j;
			double k;
		};

		double data[4];
	};

	Quaternion() : r(1), i(0), j(0), k(0) {}

	Quaternion(const double r, const double i, const double j, const double k)
		:
		r(r),
		i(i),
		j(j),
		k(k)
	{

	}

	void Normalize()
	{
		double length = r * r + i * i + j * j + k * k;

		// Check for zero length quaternion 
		// If so use no rotation quaternion
		if (length < 0.001f)
		{
			r = 1;
			return;
		}

		length = ((double)1.0f / (double)sqrt(length));
		
		r *= length;
		i *= length;
		j *= length;
		k *= length;
	}

	// Multiplies Quaternion by the given Quaternion
	void operator *=(const Quaternion& multiplier)
	{
		Quaternion q = *this;

		r = q.r * multiplier.r - q.i * multiplier.i - q.j * multiplier.j - q.k * multiplier.k;
		i = q.r * multiplier.i + q.i * multiplier.r + q.j * multiplier.k - q.k * multiplier.j;
		j = q.r * multiplier.j + q.j * multiplier.r + q.k * multiplier.i - q.i * multiplier.k;
		k = q.r * multiplier.k + q.k * multiplier.r + q.i * multiplier.j - q.j * multiplier.k;
	}

	void AddScaledVector(const XMFLOAT3& vector, double scale)
	{
		Quaternion q(0, vector.x * scale, vector.y * scale, vector.z * scale);
		q *= *this;
		
		r += q.r * (0.5);
		i += q.i * (0.5);
		j += q.j * (0.5);
		k += q.k * (0.5);
	}

	void RotateByVector(const XMFLOAT3& vector)
	{
		Quaternion q(0, vector.x, vector.y, vector.z);
		(*this) *= q;
	}
};