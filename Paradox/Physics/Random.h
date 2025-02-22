#pragma once

#include "../ParadoxMath.h"

/**
    * Keeps track of one random stream: i.e. a seed and its output.
    * This is used to get random numbers. Rather than a funcion, this
    * allows there to be several streams of repeatable random numbers
    * at the same time. Uses the RandRotB algorithm.
    */
class Random
{
public:
    /**
     * left bitwise rotation
     */

    unsigned rotl(unsigned n, unsigned r);
    /**
     * right bitwise rotation
     */
    unsigned rotr(unsigned n, unsigned r);

    /**
     * Creates a new random number stream with a seed based on
     * timing data.
     */
    Random();

    /**
     * Creates a new random stream with the given seed.
     */
    Random(unsigned seed);

    /**
     * Sets the seed value for the random stream.
     */
    void seed(unsigned seed);

    /**
     * Returns the next random bitstring from the stream. This is
     * the fastest method.
     */
    unsigned randomBits();

    /**
     * Returns a random floating point number between 0 and 1.
     */
    double randomDouble();

    /**
     * Returns a random floating point number between 0 and scale.
     */
    double randomDouble(double scale);

    /**
     * Returns a random floating point number between min and max.
     */
    double randomDouble(double min, double max);

    /**
     * Returns a random integer less than the given value.
     */
    unsigned randomInt(unsigned max);

    /**
     * Returns a random binomially distributed number between -scale
     * and +scale.
     */
    double randomBinomial(double scale);

    /**
     * Returns a random vector where each component is binomially
     * distributed in the range (-scale to scale) [mean = 0.0f].
     */
    Vector3 randomVector(double scale);

    /**
     * Returns a random vector where each component is binomially
     * distributed in the range (-scale to scale) [mean = 0.0f],
     * where scale is the corresponding component of the given
     * vector.
     */
    Vector3 randomVector(const Vector3& scale);

    /**
     * Returns a random vector in the cube defined by the given
     * minimum and maximum vectors. The probability is uniformly
     * distributed in this region.
     */
    Vector3 randomVector(const Vector3& min, const Vector3& max);

    /**
     * Returns a random vector where each component is binomially
     * distributed in the range (-scale to scale) [mean = 0.0f],
     * except the y coordinate which is zero.
     */
    Vector3 randomXZVector(double scale);

    /**
     * Returns a random orientation (i.e. normalized) quaternion.
     */
    Quaternion randomQuaternion();

private:
    // Internal mechanics
    int p1, p2;
    unsigned buffer[17];
};