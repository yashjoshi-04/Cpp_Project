#include "EuropeanTrade.h"
#include "Market.h"      // For Market access in Pv (e.g., for rates, vols if doing BS here)
#include "Date.h"        // For Date operations (e.g. operator- for TTM)
#include "MathUtils.h"   // For normalCDF (if implementing Black-Scholes here for comparison)
#include <cmath>        // For std::log, std::exp, std::sqrt
#include <stdexcept>    // For std::runtime_error

// --- EuropeanOption Method Implementations ---

// Default constructor
EuropeanOption::EuropeanOption()
    : TreeProduct(""), // Call TreeProduct constructor with empty underlying
      optionType(Call), 
      strike(0.0), 
      expiryDate(Date()), // Default date
      discountCurveName("USD-SOFR"),
      volatilityCurveName("VOL_CURVE_DEFAULT") {
    tradeType = "EuropeanOption"; // Set trade type from Trade base class
}

EuropeanOption::EuropeanOption(OptionType optType,
                               double strikePrice,
                               const Date& expiryDt,
                               const std::string& underlyingInstName,
                               const std::string& discCurveName,
                               const std::string& volCurveName)
    : TreeProduct(underlyingInstName), // Pass underlying to TreeProduct constructor
      optionType(optType),
      strike(strikePrice),
      expiryDate(expiryDt),
      discountCurveName(discCurveName),
      volatilityCurveName(volCurveName) {
    tradeType = "EuropeanOption"; // Set trade type from Trade base class
    // `underlying` member is in TreeProduct, initialized by TreeProduct's constructor.
}

// Present Value: For EuropeanOption, the main pricing (Binomial Tree) will be handled by a Pricer class.
// This Pv method can implement Black-Scholes for direct comparison as requested in the write-up.
// The Pricer will call Payoff, GetExpiry, ValueAtNode.
double EuropeanOption::Pv(const Market& mkt) const {
    // This method will return the Black-Scholes price for comparison purposes.
    // The 50-step binomial tree pricing is handled by the CRRBinomialTreePricer class.

    std::shared_ptr<const RateCurve> rateCurve = mkt.getCurve(discountCurveName);
    std::shared_ptr<const VolCurve> volCurve = mkt.getVolCurve(volatilityCurveName);
    Date valuationDate = mkt.asOf;

    if (!rateCurve || rateCurve->isEmpty()) {
        std::cerr << "Error: Rate curve '" << discountCurveName << "' not found or empty for BS pricing of " << underlying << std::endl;
        return 0.0;
    }
    if (!volCurve || volCurve->isEmpty()) {
        std::cerr << "Error: Vol curve '" << volatilityCurveName << "' not found or empty for BS pricing of " << underlying << std::endl;
        return 0.0;
    }

    double S = mkt.getStockPrice(underlying); // `underlying` is a member of TreeProduct
    if (S <= 1e-9) { // Stock price is zero or negative
        // For call, value is 0. For put, value is K*exp(-rT) if K > 0, else 0.
        if (optionType == Call) return 0.0;
        if (optionType == Put) {
            if (strike <= 1e-9) return 0.0;
            double T = (expiryDate - valuationDate); 
            if (T < 0) T = 0; // Expired
            double r = rateCurve->getRate(expiryDate); // Risk-free rate to expiry
            return strike * std::exp(-r * T);
        }
        return 0.0; // Other binary types etc.
    }

    double K = this->strike;
    Date T_expiry = this->expiryDate;
    
    if (valuationDate >= T_expiry) { // Option expired
        return Payoff(S); // Intrinsic value at expiry (usually 0 if S didn't cross K appropriately)
    }

    double T = (T_expiry - valuationDate); // Time to maturity in years (uses Date::operator-)
    double r = rateCurve->getRate(T_expiry); // Risk-free rate to expiry
    double sigma = volCurve->getVol(T_expiry); // Volatility to expiry

    if (sigma <= 1e-9) { // Volatility is zero
        // Option value is intrinsic value discounted, as there's no uncertainty.
        double discounted_S = S * std::exp(0); // No dividend yield assumed for this simple BS
        double discounted_K = K * std::exp(-r * T);
        double intrinsic_at_expiry_no_vol = 0.0;
        if (optionType == Call) intrinsic_at_expiry_no_vol = std::max(0.0, discounted_S - discounted_K);
        else if (optionType == Put) intrinsic_at_expiry_no_vol = std::max(0.0, discounted_K - discounted_S);
        // Binaries would be 0 or exp(-rT)
        return intrinsic_at_expiry_no_vol; 
    }
    if (T <= 1e-9) { // Effectively expired or on expiry day
         return Payoff(S); // Price is intrinsic value
    }

    double d1 = (std::log(S / K) + (r + 0.5 * sigma * sigma) * T) / (sigma * std::sqrt(T));
    double d2 = d1 - sigma * std::sqrt(T);

    double pv = 0.0;
    if (optionType == Call) {
        pv = S * normalCDF(d1) - K * std::exp(-r * T) * normalCDF(d2);
    } else if (optionType == Put) {
        pv = K * std::exp(-r * T) * normalCDF(-d2) - S * normalCDF(-d1);
    } else {
        // Binary options, etc., not implemented here for BS, but Payoff structure exists.
        // This Pv is for standard Call/Put BS. Binaries are usually priced differently or via limits of spreads.
        std::cerr << "Warning: Black-Scholes PV calculation in EuropeanOption::Pv only supports Call/Put." << std::endl;
        pv = 0.0; // Or throw
    }
    return pv;
}

// Payoff function (already in .h, but definition belongs in .cpp)
double EuropeanOption::Payoff(double S) const {
    return PAYOFF::VanillaOption(optionType, strike, S);
}

// GetExpiry function (already in .h, definition in .cpp)
const Date& EuropeanOption::GetExpiry() const {
    return expiryDate;
}

// ValueAtNode for European option: value is simply the continuation value (discounted expected future value)
// No early exercise.
double EuropeanOption::ValueAtNode(double S, double t, double continuation) const {
    // S and t (current stock price and time at node) are not used for European value at node if continuation is already calculated.
    // This is consistent with the simple `return continuation;` in the original header.
    return continuation;
}

// Note: EuroCallSpread would be implemented similarly if required.
// Added and Updated
