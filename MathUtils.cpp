#include "MathUtils.h"
#include <stdexcept> // For std::invalid_argument
#include <algorithm> 

// Cumulative Distribution Function for Standard Normal Distribution
double normalCDF(double x) {
    // Using std::erf (error function) from <cmath>
    // erf(x / sqrt(2)) / 2 + 0.5
    return 0.5 * (1.0 + std::erf(x * M_SQRT1_2));
}

// Black-Scholes formula for European call/put options
double blackScholesPrice(OptionType optionType, 
                         double S, double K, 
                         double T, double r, 
                         double sigma) {
    if (T < 0) { // Option expired
        // std::cerr << "Warning: Negative time to maturity (T=" << T << ") in Black-Scholes. Returning 0." << std::endl;
        T = 0.0;
    }
    if (sigma < 0) {
        // std::cerr << "Warning: Negative volatility (sigma=" << sigma << ") in Black-Scholes. Using abs(sigma)." << std::endl;
        sigma = std::abs(sigma);
    }
    if (K <= 1e-9) { // Strike is zero or negative
        if (optionType == Call) return std::max(0.0, S - K * std::exp(-r * T)); // Effectively S if K=0 and no dividends
        if (optionType == Put) return std::max(0.0, K * std::exp(-r * T) - S); // Effectively 0 if K=0 and no dividends
        return 0.0;
    }
    if (S <= 1e-9) { // Spot is zero or negative
         if (optionType == Call) return 0.0;
         if (optionType == Put) return std::max(0.0, K * std::exp(-r * T));
         return 0.0;
    }
    if (T <= 1e-9) { // Option at expiry
        if (optionType == Call) return std::max(0.0, S - K);
        if (optionType == Put) return std::max(0.0, K - S);
        return 0.0; // Or handle other types
    }
    if (sigma <= 1e-9) { // No volatility
        double forwardPrice = S; // Assuming no dividend yield for simplicity: S * exp((r-q)*T)
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
        throw std::invalid_argument("Unsupported option type for Black-Scholes formula.");
    }
}

//Updated
