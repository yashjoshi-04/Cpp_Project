#include "MathUtils.h"
#include <cmath> // Ensure it's here too for freestanding cpp if needed

double normalCDF(double x) {
    // Abramowitz and Stegun formula 26.2.17
    // Constants
    double a1 =  0.254829592;
    double a2 = -0.284496736;
    double a3 =  1.421413741;
    double a4 = -1.453152027;
    double a5 =  1.061405429;
    double p  =  0.3275911;

    int sign = 1;
    if (x < 0.0)
        sign = -1;
    x = std::abs(x);

    // A&S formula
    double t = 1.0 / (1.0 + p * x);
    double y = 1.0 - (((((a5 * t + a4) * t) + a3) * t + a2) * t + a1) * t * std::exp(-x * x / 2.0);

    return 0.5 * (1.0 + sign * y);
}
