#include "../ParadoxMath.h"

const Vector3 Vector3::GRAVITY = Vector3(0, -9.81, 0);
const Vector3 Vector3::HIGH_GRAVITY = Vector3(0, -19.62, 0);
const Vector3 Vector3::UP = Vector3(0, 1, 0);
const Vector3 Vector3::RIGHT = Vector3(1, 0, 0);
const Vector3 Vector3::OUT_OF_SCREEN = Vector3(0, 0, 1);
const Vector3 Vector3::X = Vector3(0, 1, 0);
const Vector3 Vector3::Y = Vector3(1, 0, 0);
const Vector3 Vector3::Z = Vector3(0, 0, 1);

/*
 * Definition of the sleep epsilon extern.
 */
double sleepEpsilon = ((double)0.3);

/*
 * Functions to change sleepEpsilon.
 */
void setSleepEpsilon(double value)
{
    sleepEpsilon = value;
}

double getSleepEpsilon()
{
    return sleepEpsilon;
}

double Matrix4::getDeterminant() const
{
    return -data[8] * data[5] * data[2] +
        data[4] * data[9] * data[2] +
        data[8] * data[1] * data[6] -
        data[0] * data[9] * data[6] -
        data[4] * data[1] * data[10] +
        data[0] * data[5] * data[10];
}

void Matrix4::setInverse(const Matrix4& m)
{
    // Make sure the determinant is non-zero.
    double det = getDeterminant();
    if (det == 0) return;
    det = ((double)1.0) / det;

    data[0] = (-m.data[9] * m.data[6] + m.data[5] * m.data[10]) * det;
    data[4] = (m.data[8] * m.data[6] - m.data[4] * m.data[10]) * det;
    data[8] = (-m.data[8] * m.data[5] + m.data[4] * m.data[9]) * det;

    data[1] = (m.data[9] * m.data[2] - m.data[1] * m.data[10]) * det;
    data[5] = (-m.data[8] * m.data[2] + m.data[0] * m.data[10]) * det;
    data[9] = (m.data[8] * m.data[1] - m.data[0] * m.data[9]) * det;

    data[2] = (-m.data[5] * m.data[2] + m.data[1] * m.data[6]) * det;
    data[6] = (+m.data[4] * m.data[2] - m.data[0] * m.data[6]) * det;
    data[10] = (-m.data[4] * m.data[1] + m.data[0] * m.data[5]) * det;

    data[3] = (m.data[9] * m.data[6] * m.data[3]
        - m.data[5] * m.data[10] * m.data[3]
        - m.data[9] * m.data[2] * m.data[7]
        + m.data[1] * m.data[10] * m.data[7]
        + m.data[5] * m.data[2] * m.data[11]
        - m.data[1] * m.data[6] * m.data[11]) * det;
    data[7] = (-m.data[8] * m.data[6] * m.data[3]
        + m.data[4] * m.data[10] * m.data[3]
        + m.data[8] * m.data[2] * m.data[7]
        - m.data[0] * m.data[10] * m.data[7]
        - m.data[4] * m.data[2] * m.data[11]
        + m.data[0] * m.data[6] * m.data[11]) * det;
    data[11] = (m.data[8] * m.data[5] * m.data[3]
        - m.data[4] * m.data[9] * m.data[3]
        - m.data[8] * m.data[1] * m.data[7]
        + m.data[0] * m.data[9] * m.data[7]
        + m.data[4] * m.data[1] * m.data[11]
        - m.data[0] * m.data[5] * m.data[11]) * det;
}

Matrix3 Matrix3::linearInterpolate(const Matrix3& a, const Matrix3& b, double prop)
{
    Matrix3 result;
    for (unsigned i = 0; i < 9; i++) {
        result.data[i] = a.data[i] * (1 - prop) + b.data[i] * prop;
    }
    return result;
}
