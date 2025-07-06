#include "MathUtils.h"
#include <stdexcept> // For std::invalid_argument
#include <algorithm> // For std::max <<-- THIS IS THE FIX
#include <cmath>     // For std::exp, std::log, std::sqrt, std::erf, M_SQRT1_2 (already there via MathUtils.h indirectly)

// Cumulative Distribution Function for Standard Normal Distribution
double normalCDF(double x) {
    return 0.5 * (1.0 + std::erf(x * M_SQRT1_2));
}

// Black-Scholes formula for European call/put options
double blackScholesPrice(OptionType optionType, 
                         double S, double K, 
                         double T, double r, 
                         double sigma) {
    if (T < 0) { 
        T = 0.0;
    }
    if (sigma < 0) {
        sigma = std::abs(sigma);
    }
    if (K <= 1e-9) { 
        if (optionType == Call) return std::max(0.0, S - K * std::exp(-r * T)); 
        if (optionType == Put) return std::max(0.0, K * std::exp(-r * T) - S); 
        return 0.0;
    }
    if (S <= 1e-9) { 
         if (optionType == Call) return 0.0;
         if (optionType == Put) return std::max(0.0, K * std::exp(-r * T));
         return 0.0;
    }
    if (T <= 1e-9) { 
        if (optionType == Call) return std::max(0.0, S - K);
        if (optionType == Put) return std::max(0.0, K - S);
        return 0.0; 
    }
    if (sigma <= 1e-9) { 
        double forwardPrice = S; 
        double discountedStrike = K * std::exp(-r * T);
        if (optionType == Call) return std::max(0.0, forwardPrice - discountedStrike);
        if (optionType == Put) return std::max(0.0, discountedStrike - forwardPrice);
        return 0.0;
    }

    double d1 = (std::log(S / K) + (r + 0.5 * sigma * sigma) * T) / (sigma * std::sqrt(T));
    double d2 = d1 - sigma * std::sqrt(T);

    if (optionType == Call) {
        return S * normalCDF(d1) - K * std::exp(-r * T) * normalCDF(d2);
    } else if (optionType == Put) {
        return K * std::exp(-r * T) * normalCDF(-d2) - S * normalCDF(-d1);
    } else {
        // For BinaryCall/BinaryPut, a simple BS formula isn't standard here.
        // Typically, they are priced as limits of call/put spreads or have their own formulas.
        // Returning 0 for them if this function is called for those types.
        // Or throw std::invalid_argument("Unsupported option type for this Black-Scholes formula.");
        return 0.0; 
    }
}
