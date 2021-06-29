#pragma once
#include "Graphics/core.h"

using namespace DirectX;

#define M_PI 3.14159265358979323846

/**
 * Holds a vector in 3 dimensions. Four data members are allocated
 * to ensure alignment in an array.
 *
 * @note This class contains a lot of inline methods for basic
 * mathematics. The implementations are included in the header
 * file.
 */
class Vector3
{
public:
    /** Holds the value along the x axis. */
    double x;

    /** Holds the value along the y axis. */
    double y;

    /** Holds the value along the z axis. */
    double z;

private:
    /** Padding to ensure 4 word alignment. */
    double pad;

public:
    /** The default constructor creates a zero vector. */
    Vector3() : x(0), y(0), z(0) {}

    /**
     * The explicit constructor creates a vector with the given
     * components.
     */
    Vector3(const double x, const double y, const double z)
        : x(x), y(y), z(z) {}

    const static Vector3 GRAVITY;
    const static Vector3 HIGH_GRAVITY;
    const static Vector3 UP;
    const static Vector3 RIGHT;
    const static Vector3 OUT_OF_SCREEN;
    const static Vector3 X;
    const static Vector3 Y;
    const static Vector3 Z;

    // ... Other Vector3 code as before ...


    double operator[](unsigned i) const
    {
        if (i == 0) return x;
        if (i == 1) return y;
        return z;
    }

    double& operator[](unsigned i)
    {
        if (i == 0) return x;
        if (i == 1) return y;
        return z;
    }

    /** Adds the given vector to this. */
    void operator+=(const Vector3& v)
    {
        x += v.x;
        y += v.y;
        z += v.z;
    }

    /**
     * Returns the value of the given vector added to this.
     */
    Vector3 operator+(const Vector3& v) const
    {
        return Vector3(x + v.x, y + v.y, z + v.z);
    }

    /** Subtracts the given vector from this. */
    void operator-=(const Vector3& v)
    {
        x -= v.x;
        y -= v.y;
        z -= v.z;
    }

    /**
     * Returns the value of the given vector subtracted from this.
     */
    Vector3 operator-(const Vector3& v) const
    {
        return Vector3(x - v.x, y - v.y, z - v.z);
    }

    /** Multiplies this vector by the given scalar. */
    void operator*=(const double value)
    {
        x *= value;
        y *= value;
        z *= value;
    }

    /** Returns a copy of this vector scaled the given value. */
    Vector3 operator*(const double value) const
    {
        return Vector3(x * value, y * value, z * value);
    }

    /**
     * Calculates and returns a component-wise product of this
     * vector with the given vector.
     */
    Vector3 componentProduct(const Vector3& vector) const
    {
        return Vector3(x * vector.x, y * vector.y, z * vector.z);
    }

    /**
     * Performs a component-wise product with the given vector and
     * sets this vector to its result.
     */
    void componentProductUpdate(const Vector3& vector)
    {
        x *= vector.x;
        y *= vector.y;
        z *= vector.z;
    }

    /**
     * Calculates and returns the vector product of this vector
     * with the given vector.
     */
    Vector3 vectorProduct(const Vector3& vector) const
    {
        return Vector3(y * vector.z - z * vector.y,
            z * vector.x - x * vector.z,
            x * vector.y - y * vector.x);
    }

    /**
     * Updates this vector to be the vector product of its current
     * value and the given vector.
     */
    void operator %=(const Vector3& vector)
    {
        *this = vectorProduct(vector);
    }

    /**
     * Calculates and returns the vector product of this vector
     * with the given vector.
     */
    Vector3 operator%(const Vector3& vector) const
    {
        return Vector3(y * vector.z - z * vector.y,
            z * vector.x - x * vector.z,
            x * vector.y - y * vector.x);
    }

    /**
     * Calculates and returns the scalar product of this vector
     * with the given vector.
     */
    double scalarProduct(const Vector3& vector) const
    {
        return x * vector.x + y * vector.y + z * vector.z;
    }

    /**
     * Calculates and returns the scalar product of this vector
     * with the given vector.
     */
    double operator *(const Vector3& vector) const
    {
        return x * vector.x + y * vector.y + z * vector.z;
    }

    /**
     * Adds the given vector to this, scaled by the given amount.
     */
    void addScaledVector(const Vector3& vector, double scale)
    {
        x += vector.x * scale;
        y += vector.y * scale;
        z += vector.z * scale;
    }

    /** Gets the magnitude of this vector. */
    double magnitude() const
    {
        return sqrt(x * x + y * y + z * z);
    }

    /** Gets the squared magnitude of this vector. */
    double squareMagnitude() const
    {
        return x * x + y * y + z * z;
    }

    /** Limits the size of the vector to the given maximum. */
    void trim(double size)
    {
        if (squareMagnitude() > size * size)
        {
            normalise();
            x *= size;
            y *= size;
            z *= size;
        }
    }

    /** Turns a non-zero vector into a vector of unit length. */
    void normalise()
    {
        double l = magnitude();
        if (l > 0)
        {
            (*this) *= ((double)1) / l;
        }
    }

    /** Returns the normalised version of a vector. */
    Vector3 unit() const
    {
        Vector3 result = *this;
        result.normalise();
        return result;
    }

    /** Checks if the two vectors have identical components. */
    bool operator==(const Vector3& other) const
    {
        return x == other.x &&
            y == other.y &&
            z == other.z;
    }

    /** Checks if the two vectors have non-identical components. */
    bool operator!=(const Vector3& other) const
    {
        return !(*this == other);
    }

    /**
     * Checks if this vector is component-by-component less than
     * the other.
     *
     * @note This does not behave like a single-value comparison:
     * !(a < b) does not imply (b >= a).
     */
    bool operator<(const Vector3& other) const
    {
        return x < other.x&& y < other.y&& z < other.z;
    }

    /**
     * Checks if this vector is component-by-component less than
     * the other.
     *
     * @note This does not behave like a single-value comparison:
     * !(a < b) does not imply (b >= a).
     */
    bool operator>(const Vector3& other) const
    {
        return x > other.x && y > other.y && z > other.z;
    }

    /**
     * Checks if this vector is component-by-component less than
     * the other.
     *
     * @note This does not behave like a single-value comparison:
     * !(a <= b) does not imply (b > a).
     */
    bool operator<=(const Vector3& other) const
    {
        return x <= other.x && y <= other.y && z <= other.z;
    }

    /**
     * Checks if this vector is component-by-component less than
     * the other.
     *
     * @note This does not behave like a single-value comparison:
     * !(a <= b) does not imply (b > a).
     */
    bool operator>=(const Vector3& other) const
    {
        return x >= other.x && y >= other.y && z >= other.z;
    }

    /** Zero all the components of the vector. */
    void clear()
    {
        x = y = z = 0;
    }

    /** Flips all the components of the vector. */
    void invert()
    {
        x = -x;
        y = -y;
        z = -z;
    }

};


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

	void Quaternion::Normalize()
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

	void Quaternion::AddScaledVector(const Vector3& vector, double scale)
	{
		Quaternion q(0, vector.x * scale, vector.y * scale, vector.z * scale);
		q *= *this;
		
		r += q.r * (0.5);
		i += q.i * (0.5);
		j += q.j * (0.5);
		k += q.k * (0.5);
	}

	void Quaternion::RotateByVector(const Vector3& vector)
	{
		Quaternion q(0, vector.x, vector.y, vector.z);
		(*this) *= q;
	}
};


/*
*Holds the value for energy under which a body will be put to
* sleep.This is a global value for the whole solution.By
* default it is 0.1, which is fine for simulation when gravity is
* about 20 units per second squared, masses are about one, and
* other forces are around that of gravity.It may need tweaking
* if your simulation is drastically different to this.
*/

extern double sleepEpsilon;

/**
 * Sets the current sleep epsilon value: the kinetic energy under
 * which a body may be put to sleep. Bodies are put to sleep if
 * they appear to have a stable kinetic energy less than this
 * value. For simulations that often have low values (such as slow
 * moving, or light objects), this may need reducing.
 *
 * The value is global; all bodies will use it.
 *
 * @see sleepEpsilon
 *
 * @see getSleepEpsilon
 *
 * @param value The sleep epsilon value to use from this point
 * on.
 */
void setSleepEpsilon(double value);

/**
 * Gets the current value of the sleep epsilon parameter.
 *
 * @see sleepEpsilon
 *
 * @see setSleepEpsilon
 *
 * @return The current value of the parameter.
 */
double getSleepEpsilon();

class Matrix4
{
public:
    /**
     * Holds the transform matrix data in array form.
     */
    double data[12];

    // ... Other Matrix4 code as before ...


    /**
     * Creates an identity matrix.
     */
    Matrix4()
    {
        data[1] = data[2] = data[3] = data[4] = data[6] =
            data[7] = data[8] = data[9] = data[11] = 0;
        data[0] = data[5] = data[10] = 1;
    }

    /**
     * Sets the matrix to be a diagonal matrix with the given coefficients.
     */
    void setDiagonal(double a, double b, double c)
    {
        data[0] = a;
        data[5] = b;
        data[10] = c;
    }

    /**
     * Returns a matrix which is this matrix multiplied by the given
     * other matrix.
     */
    Matrix4 operator*(const Matrix4& o) const
    {
        Matrix4 result;
        result.data[0] = (o.data[0] * data[0]) + (o.data[4] * data[1]) + (o.data[8] * data[2]);
        result.data[4] = (o.data[0] * data[4]) + (o.data[4] * data[5]) + (o.data[8] * data[6]);
        result.data[8] = (o.data[0] * data[8]) + (o.data[4] * data[9]) + (o.data[8] * data[10]);

        result.data[1] = (o.data[1] * data[0]) + (o.data[5] * data[1]) + (o.data[9] * data[2]);
        result.data[5] = (o.data[1] * data[4]) + (o.data[5] * data[5]) + (o.data[9] * data[6]);
        result.data[9] = (o.data[1] * data[8]) + (o.data[5] * data[9]) + (o.data[9] * data[10]);

        result.data[2] = (o.data[2] * data[0]) + (o.data[6] * data[1]) + (o.data[10] * data[2]);
        result.data[6] = (o.data[2] * data[4]) + (o.data[6] * data[5]) + (o.data[10] * data[6]);
        result.data[10] = (o.data[2] * data[8]) + (o.data[6] * data[9]) + (o.data[10] * data[10]);

        result.data[3] = (o.data[3] * data[0]) + (o.data[7] * data[1]) + (o.data[11] * data[2]) + data[3];
        result.data[7] = (o.data[3] * data[4]) + (o.data[7] * data[5]) + (o.data[11] * data[6]) + data[7];
        result.data[11] = (o.data[3] * data[8]) + (o.data[7] * data[9]) + (o.data[11] * data[10]) + data[11];

        return result;
    }

    /**
     * Transform the given vector by this matrix.
     *
     * @param vector The vector to transform.
     */
    Vector3 operator*(const Vector3& vector) const
    {
        return Vector3(
            vector.x * data[0] +
            vector.y * data[1] +
            vector.z * data[2] + data[3],

            vector.x * data[4] +
            vector.y * data[5] +
            vector.z * data[6] + data[7],

            vector.x * data[8] +
            vector.y * data[9] +
            vector.z * data[10] + data[11]
        );
    }

    /**
     * Transform the given vector by this matrix.
     *
     * @param vector The vector to transform.
     */
    Vector3 transform(const Vector3& vector) const
    {
        return (*this) * vector;
    }

    /**
     * Returns the determinant of the matrix.
     */
    double getDeterminant() const;

    /**
     * Sets the matrix to be the inverse of the given matrix.
     *
     * @param m The matrix to invert and use to set this.
     */
    void setInverse(const Matrix4& m);

    /** Returns a new matrix containing the inverse of this matrix. */
    Matrix4 inverse() const
    {
        Matrix4 result;
        result.setInverse(*this);
        return result;
    }

    /**
     * Inverts the matrix.
     */
    void invert()
    {
        setInverse(*this);
    }

    /**
     * Transform the given direction vector by this matrix.
     *
     * @note When a direction is converted between frames of
     * reference, there is no translation required.
     *
     * @param vector The vector to transform.
     */
    Vector3 transformDirection(const Vector3& vector) const
    {
        return Vector3(
            vector.x * data[0] +
            vector.y * data[1] +
            vector.z * data[2],

            vector.x * data[4] +
            vector.y * data[5] +
            vector.z * data[6],

            vector.x * data[8] +
            vector.y * data[9] +
            vector.z * data[10]
        );
    }

    /**
     * Transform the given direction vector by the
     * transformational inverse of this matrix.
     *
     * @note This function relies on the fact that the inverse of
     * a pure rotation matrix is its transpose. It separates the
     * translational and rotation components, transposes the
     * rotation, and multiplies out. If the matrix is not a
     * scale and shear free transform matrix, then this function
     * will not give correct results.
     *
     * @note When a direction is converted between frames of
     * reference, there is no translation required.
     *
     * @param vector The vector to transform.
     */
    Vector3 transformInverseDirection(const Vector3& vector) const
    {
        return Vector3(
            vector.x * data[0] +
            vector.y * data[4] +
            vector.z * data[8],

            vector.x * data[1] +
            vector.y * data[5] +
            vector.z * data[9],

            vector.x * data[2] +
            vector.y * data[6] +
            vector.z * data[10]
        );
    }

    /**
     * Transform the given vector by the transformational inverse
     * of this matrix.
     *
     * @note This function relies on the fact that the inverse of
     * a pure rotation matrix is its transpose. It separates the
     * translational and rotation components, transposes the
     * rotation, and multiplies out. If the matrix is not a
     * scale and shear free transform matrix, then this function
     * will not give correct results.
     *
     * @param vector The vector to transform.
     */
    Vector3 transformInverse(const Vector3& vector) const
    {
        Vector3 tmp = vector;
        tmp.x -= data[3];
        tmp.y -= data[7];
        tmp.z -= data[11];
        return Vector3(
            tmp.x * data[0] +
            tmp.y * data[4] +
            tmp.z * data[8],

            tmp.x * data[1] +
            tmp.y * data[5] +
            tmp.z * data[9],

            tmp.x * data[2] +
            tmp.y * data[6] +
            tmp.z * data[10]
        );
    }

    /**
     * Gets a vector representing one axis (i.e. one column) in the matrix.
     *
     * @param i The row to return. Row 3 corresponds to the position
     * of the transform matrix.
     *
     * @return The vector.
     */
    Vector3 getAxisVector(int i) const
    {
        return Vector3(data[i], data[i + 4], data[i + 8]);
    }

    /**
     * Sets this matrix to be the rotation matrix corresponding to
     * the given quaternion.
     */
    void setOrientationAndPos(const Quaternion& q, const Vector3& pos)
    {
        data[0] = 1 - (2 * q.j * q.j + 2 * q.k * q.k);
        data[1] = 2 * q.i * q.j + 2 * q.k * q.r;
        data[2] = 2 * q.i * q.k - 2 * q.j * q.r;
        data[3] = pos.x;

        data[4] = 2 * q.i * q.j - 2 * q.k * q.r;
        data[5] = 1 - (2 * q.i * q.i + 2 * q.k * q.k);
        data[6] = 2 * q.j * q.k + 2 * q.i * q.r;
        data[7] = pos.y;

        data[8] = 2 * q.i * q.k + 2 * q.j * q.r;
        data[9] = 2 * q.j * q.k - 2 * q.i * q.r;
        data[10] = 1 - (2 * q.i * q.i + 2 * q.j * q.j);
        data[11] = pos.z;
    }

    /**
     * Fills the given array with this transform matrix, so it is
     * usable as an open-gl transform matrix. OpenGL uses a column
     * major format, so that the values are transposed as they are
     * written.
     */
    void fillGLArray(float array[16]) const
    {
        array[0] = (float)data[0];
        array[1] = (float)data[4];
        array[2] = (float)data[8];
        array[3] = (float)0;

        array[4] = (float)data[1];
        array[5] = (float)data[5];
        array[6] = (float)data[9];
        array[7] = (float)0;

        array[8] = (float)data[2];
        array[9] = (float)data[6];
        array[10] = (float)data[10];
        array[11] = (float)0;

        array[12] = (float)data[3];
        array[13] = (float)data[7];
        array[14] = (float)data[11];
        array[15] = (float)1;
    }
};

/**
 * Holds an inertia tensor, consisting of a 3x3 row-major matrix.
 * This matrix is not padding to produce an aligned structure, since
 * it is most commonly used with a mass (single double) and two
 * damping coefficients to make the 12-element characteristics array
 * of a rigid body.
 */
class Matrix3
{
public:
    /**
     * Holds the tensor matrix data in array form.
     */
    double data[9];

    // ... Other Matrix3 code as before ...

    /**
     * Creates a new matrix.
     */
    Matrix3()
    {
        data[0] = data[1] = data[2] = data[3] = data[4] = data[5] =
            data[6] = data[7] = data[8] = 0;
    }

    /**
     * Creates a new matrix with the given three vectors making
     * up its columns.
     */
    Matrix3(const Vector3& compOne, const Vector3& compTwo,
        const Vector3& compThree)
    {
        setComponents(compOne, compTwo, compThree);
    }

    /**
     * Creates a new matrix with explicit coefficients.
     */
    Matrix3(double c0, double c1, double c2, double c3, double c4, double c5,
        double c6, double c7, double c8)
    {
        data[0] = c0; data[1] = c1; data[2] = c2;
        data[3] = c3; data[4] = c4; data[5] = c5;
        data[6] = c6; data[7] = c7; data[8] = c8;
    }

    /**
     * Sets the matrix to be a diagonal matrix with the given
     * values along the leading diagonal.
     */
    void setDiagonal(double a, double b, double c)
    {
        setInertiaTensorCoeffs(a, b, c);
    }

    /**
     * Sets the value of the matrix from inertia tensor values.
     */
    void setInertiaTensorCoeffs(double ix, double iy, double iz,
        double ixy = 0, double ixz = 0, double iyz = 0)
    {
        data[0] = ix;
        data[1] = data[3] = -ixy;
        data[2] = data[6] = -ixz;
        data[4] = iy;
        data[5] = data[7] = -iyz;
        data[8] = iz;
    }

    /**
     * Sets the value of the matrix as an inertia tensor of
     * a rectangular block aligned with the body's coordinate
     * system with the given axis half-sizes and mass.
     */
    void setBlockInertiaTensor(const Vector3& halfSizes, double mass)
    {
        Vector3 squares = halfSizes.componentProduct(halfSizes);
        setInertiaTensorCoeffs(0.3f * mass * (squares.y + squares.z),
            0.3f * mass * (squares.x + squares.z),
            0.3f * mass * (squares.x + squares.y));
    }

    /**
     * Sets the matrix to be a skew symmetric matrix based on
     * the given vector. The skew symmetric matrix is the equivalent
     * of the vector product. So if a,b are vectors. a x b = A_s b
     * where A_s is the skew symmetric form of a.
     */
    void setSkewSymmetric(const Vector3 vector)
    {
        data[0] = data[4] = data[8] = 0;
        data[1] = -vector.z;
        data[2] = vector.y;
        data[3] = vector.z;
        data[5] = -vector.x;
        data[6] = -vector.y;
        data[7] = vector.x;
    }

    /**
     * Sets the matrix values from the given three vector components.
     * These are arranged as the three columns of the vector.
     */
    void setComponents(const Vector3& compOne, const Vector3& compTwo,
        const Vector3& compThree)
    {
        data[0] = compOne.x;
        data[1] = compTwo.x;
        data[2] = compThree.x;
        data[3] = compOne.y;
        data[4] = compTwo.y;
        data[5] = compThree.y;
        data[6] = compOne.z;
        data[7] = compTwo.z;
        data[8] = compThree.z;

    }

    /**
     * Transform the given vector by this matrix.
     *
     * @param vector The vector to transform.
     */
    Vector3 operator*(const Vector3& vector) const
    {
        return Vector3(
            vector.x * data[0] + vector.y * data[1] + vector.z * data[2],
            vector.x * data[3] + vector.y * data[4] + vector.z * data[5],
            vector.x * data[6] + vector.y * data[7] + vector.z * data[8]
        );
    }

    /**
     * Transform the given vector by this matrix.
     *
     * @param vector The vector to transform.
     */
    Vector3 transform(const Vector3& vector) const
    {
        return (*this) * vector;
    }

    /**
     * Transform the given vector by the transpose of this matrix.
     *
     * @param vector The vector to transform.
     */
    Vector3 transformTranspose(const Vector3& vector) const
    {
        return Vector3(
            vector.x * data[0] + vector.y * data[3] + vector.z * data[6],
            vector.x * data[1] + vector.y * data[4] + vector.z * data[7],
            vector.x * data[2] + vector.y * data[5] + vector.z * data[8]
        );
    }

    /**
     * Gets a vector representing one row in the matrix.
     *
     * @param i The row to return.
     */
    Vector3 getRowVector(int i) const
    {
        return Vector3(data[i * 3], data[i * 3 + 1], data[i * 3 + 2]);
    }

    /**
     * Gets a vector representing one axis (i.e. one column) in the matrix.
     *
     * @param i The row to return.
     *
     * @return The vector.
     */
    Vector3 getAxisVector(int i) const
    {
        return Vector3(data[i], data[i + 3], data[i + 6]);
    }

    /**
     * Sets the matrix to be the inverse of the given matrix.
     *
     * @param m The matrix to invert and use to set this.
     */
    void setInverse(const Matrix3& m)
    {
        double t4 = m.data[0] * m.data[4];
        double t6 = m.data[0] * m.data[5];
        double t8 = m.data[1] * m.data[3];
        double t10 = m.data[2] * m.data[3];
        double t12 = m.data[1] * m.data[6];
        double t14 = m.data[2] * m.data[6];

        // Calculate the determinant
        double t16 = (t4 * m.data[8] - t6 * m.data[7] - t8 * m.data[8] +
            t10 * m.data[7] + t12 * m.data[5] - t14 * m.data[4]);

        // Make sure the determinant is non-zero.
        if (t16 == (double)0.0f) return;
        double t17 = 1 / t16;

        data[0] = (m.data[4] * m.data[8] - m.data[5] * m.data[7]) * t17;
        data[1] = -(m.data[1] * m.data[8] - m.data[2] * m.data[7]) * t17;
        data[2] = (m.data[1] * m.data[5] - m.data[2] * m.data[4]) * t17;
        data[3] = -(m.data[3] * m.data[8] - m.data[5] * m.data[6]) * t17;
        data[4] = (m.data[0] * m.data[8] - t14) * t17;
        data[5] = -(t6 - t10) * t17;
        data[6] = (m.data[3] * m.data[7] - m.data[4] * m.data[6]) * t17;
        data[7] = -(m.data[0] * m.data[7] - t12) * t17;
        data[8] = (t4 - t8) * t17;
    }

    /** Returns a new matrix containing the inverse of this matrix. */
    Matrix3 inverse() const
    {
        Matrix3 result;
        result.setInverse(*this);
        return result;
    }

    /**
     * Inverts the matrix.
     */
    void invert()
    {
        setInverse(*this);
    }

    /**
     * Sets the matrix to be the transpose of the given matrix.
     *
     * @param m The matrix to transpose and use to set this.
     */
    void setTranspose(const Matrix3& m)
    {
        data[0] = m.data[0];
        data[1] = m.data[3];
        data[2] = m.data[6];
        data[3] = m.data[1];
        data[4] = m.data[4];
        data[5] = m.data[7];
        data[6] = m.data[2];
        data[7] = m.data[5];
        data[8] = m.data[8];
    }

    /** Returns a new matrix containing the transpose of this matrix. */
    Matrix3 transpose() const
    {
        Matrix3 result;
        result.setTranspose(*this);
        return result;
    }

    /**
     * Returns a matrix which is this matrix multiplied by the given
     * other matrix.
     */
    Matrix3 operator*(const Matrix3& o) const
    {
        return Matrix3(
            data[0] * o.data[0] + data[1] * o.data[3] + data[2] * o.data[6],
            data[0] * o.data[1] + data[1] * o.data[4] + data[2] * o.data[7],
            data[0] * o.data[2] + data[1] * o.data[5] + data[2] * o.data[8],

            data[3] * o.data[0] + data[4] * o.data[3] + data[5] * o.data[6],
            data[3] * o.data[1] + data[4] * o.data[4] + data[5] * o.data[7],
            data[3] * o.data[2] + data[4] * o.data[5] + data[5] * o.data[8],

            data[6] * o.data[0] + data[7] * o.data[3] + data[8] * o.data[6],
            data[6] * o.data[1] + data[7] * o.data[4] + data[8] * o.data[7],
            data[6] * o.data[2] + data[7] * o.data[5] + data[8] * o.data[8]
        );
    }

    /**
     * Multiplies this matrix in place by the given other matrix.
     */
    void operator*=(const Matrix3& o)
    {
        double t1;
        double t2;
        double t3;

        t1 = data[0] * o.data[0] + data[1] * o.data[3] + data[2] * o.data[6];
        t2 = data[0] * o.data[1] + data[1] * o.data[4] + data[2] * o.data[7];
        t3 = data[0] * o.data[2] + data[1] * o.data[5] + data[2] * o.data[8];
        data[0] = t1;
        data[1] = t2;
        data[2] = t3;

        t1 = data[3] * o.data[0] + data[4] * o.data[3] + data[5] * o.data[6];
        t2 = data[3] * o.data[1] + data[4] * o.data[4] + data[5] * o.data[7];
        t3 = data[3] * o.data[2] + data[4] * o.data[5] + data[5] * o.data[8];
        data[3] = t1;
        data[4] = t2;
        data[5] = t3;

        t1 = data[6] * o.data[0] + data[7] * o.data[3] + data[8] * o.data[6];
        t2 = data[6] * o.data[1] + data[7] * o.data[4] + data[8] * o.data[7];
        t3 = data[6] * o.data[2] + data[7] * o.data[5] + data[8] * o.data[8];
        data[6] = t1;
        data[7] = t2;
        data[8] = t3;
    }

    /**
     * Multiplies this matrix in place by the given scalar.
     */
    void operator*=(const double scalar)
    {
        data[0] *= scalar; data[1] *= scalar; data[2] *= scalar;
        data[3] *= scalar; data[4] *= scalar; data[5] *= scalar;
        data[6] *= scalar; data[7] *= scalar; data[8] *= scalar;
    }

    /**
     * Does a component-wise addition of this matrix and the given
     * matrix.
     */
    void operator+=(const Matrix3& o)
    {
        data[0] += o.data[0]; data[1] += o.data[1]; data[2] += o.data[2];
        data[3] += o.data[3]; data[4] += o.data[4]; data[5] += o.data[5];
        data[6] += o.data[6]; data[7] += o.data[7]; data[8] += o.data[8];
    }

    /**
     * Sets this matrix to be the rotation matrix corresponding to
     * the given quaternion.
     */
    void setOrientation(const Quaternion& q)
    {
        data[0] = 1 - (2 * q.j * q.j + 2 * q.k * q.k);
        data[1] = 2 * q.i * q.j + 2 * q.k * q.r;
        data[2] = 2 * q.i * q.k - 2 * q.j * q.r;
        data[3] = 2 * q.i * q.j - 2 * q.k * q.r;
        data[4] = 1 - (2 * q.i * q.i + 2 * q.k * q.k);
        data[5] = 2 * q.j * q.k + 2 * q.i * q.r;
        data[6] = 2 * q.i * q.k + 2 * q.j * q.r;
        data[7] = 2 * q.j * q.k - 2 * q.i * q.r;
        data[8] = 1 - (2 * q.i * q.i + 2 * q.j * q.j);
    }

    /**
     * Interpolates a couple of matrices.
     */
    static Matrix3 linearInterpolate(const Matrix3& a, const Matrix3& b, double prop);
};


