#include "EuropeanTrade.h"
#include "Market.h"      
#include "Date.h"        
#include "MathUtils.h"   
#include <cmath>        
#include <stdexcept>    
#include <iostream>     // For std::cerr
#include <algorithm>    // For std::max, not directly used in European ValueAtNode but good for consistency with American

EuropeanOption::EuropeanOption()
    : TreeProduct(""), 
      optionType(OptionType::Call), 
      strike(0.0), 
      expiryDate(Date()), 
      discountCurveName("USD-SOFR"),
      volatilityCurveName("VOL_CURVE_DEFAULT") {
    tradeType = "EuropeanOption"; 
}

EuropeanOption::EuropeanOption(OptionType optType,
                               double strikePrice,
                               const Date& expiryDt,
                               const std::string& underlyingInstName,
                               const std::string& discCurveName,
                               const std::string& volCurveName)
    : TreeProduct(underlyingInstName), 
      optionType(optType),
      strike(strikePrice),
      expiryDate(expiryDt),
      discountCurveName(discCurveName),
      volatilityCurveName(volCurveName) {
    tradeType = "EuropeanOption"; 
}

double EuropeanOption::Pv(const Market& mkt) const {
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

    double S = mkt.getStockPrice(underlying); 
    if (S < 0) S = 0; // Guard against negative stock price

    double K = this->strike;
    Date T_expiry = this->expiryDate;
    
    // Corrected comparison: !(valuationDate < T_expiry) means valuationDate >= T_expiry
    if (!(valuationDate < T_expiry)) { 
        return Payoff(S); 
    }

    double T = (T_expiry - valuationDate); 
    double r = rateCurve->getRate(T_expiry); 
    double sigma = volCurve->getVol(T_expiry); 

    if (S <= 1e-9) { 
        if (optionType == OptionType::Call) return 0.0;
        if (optionType == OptionType::Put) {
            if (K <= 1e-9) return 0.0;
            return K * std::exp(-r * T);
        }
        return 0.0; 
    }
    if (sigma <= 1e-9) { 
        double discounted_S = S; 
        double discounted_K = K * std::exp(-r * T);
        double intrinsic_at_expiry_no_vol = 0.0;
        if (optionType == OptionType::Call) intrinsic_at_expiry_no_vol = std::max(0.0, discounted_S - discounted_K);
        else if (optionType == OptionType::Put) intrinsic_at_expiry_no_vol = std::max(0.0, discounted_K - discounted_S);
        return intrinsic_at_expiry_no_vol; 
    }
    if (T <= 1e-9) { 
         return Payoff(S); 
    }

    double d1 = (std::log(S / K) + (r + 0.5 * sigma * sigma) * T) / (sigma * std::sqrt(T));
    double d2 = d1 - sigma * std::sqrt(T);

    double pv = 0.0;
    if (optionType == OptionType::Call) {
        pv = S * normalCDF(d1) - K * std::exp(-r * T) * normalCDF(d2);
    } else if (optionType == OptionType::Put) {
        pv = K * std::exp(-r * T) * normalCDF(-d2) - S * normalCDF(-d1);
    } else {
        std::cerr << "Warning: Black-Scholes PV calculation in EuropeanOption::Pv only supports Call/Put for now." << std::endl;
        pv = 0.0; 
    }
    return pv;
}

double EuropeanOption::Payoff(double S) const {
    return PAYOFF::VanillaOption(optionType, strike, S);
}

const Date& EuropeanOption::GetExpiry() const {
    return expiryDate;
}

double EuropeanOption::ValueAtNode([[maybe_unused]] double S, [[maybe_unused]] double t, double continuation) const {
    return continuation;
}
