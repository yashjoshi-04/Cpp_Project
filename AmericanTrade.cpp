#include "AmericanTrade.h"
#include "Market.h"      
#include "Date.h"        
#include <algorithm>    // For std::max
#include <stdexcept>    
#include <iostream>     // For std::cerr (if used for warnings)

AmericanOption::AmericanOption()
    : TreeProduct(""), 
      optionType(OptionType::Call),
      strike(0.0),
      expiryDate(Date()), 
      discountCurveName("USD-SOFR"),
      volatilityCurveName("VOL_CURVE_DEFAULT") {
    tradeType = "AmericanOption"; 
}

AmericanOption::AmericanOption(OptionType optType,
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
    tradeType = "AmericanOption"; 
}

double AmericanOption::Pv([[maybe_unused]] const Market& mkt) const {
    // American options are valued using numerical methods like binomial trees via a Pricer.
    // This Pv method is a placeholder; actual value comes from Pricer calling ValueAtNode.
    // It could throw, or return a conceptual value if one made sense outside a tree.
    // Returning 0.0 as it's not meant to be the primary valuation path for American options.
    // std::cerr << "Warning: AmericanOption::Pv called directly. Price should be obtained via a Pricer." << std::endl;
    return 0.0; 
}

double AmericanOption::Payoff(double S) const {
    return PAYOFF::VanillaOption(optionType, strike, S);
}

const Date& AmericanOption::GetExpiry() const {
    return expiryDate;
}

double AmericanOption::ValueAtNode(double S, [[maybe_unused]] double t, double continuation) const {
    return std::max(Payoff(S), continuation);
}
