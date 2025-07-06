#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <cmath> // For std::abs, std::exp, std::sqrt, std::log, std::erf, M_SQRT1_2
#include "Types.h" // For OptionType

// Define M_SQRT1_2 if not available (e.g. on MSVC before C++17 standard library fully supports it)
#if (defined(_WIN32) || defined(_WIN64)) && !defined(M_SQRT1_2)
#define M_SQRT1_2 0.70710678118654752440 // 1/sqrt(2)
#endif

double normalCDF(double x);

double blackScholesPrice(OptionType optionType, 
                         double S, double K, 
                         double T, double r, 
                         double sigma);

#endif // MATH_UTILS_H
//Updated
