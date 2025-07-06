#include "AmericanTrade.h"
#include "Market.h"      // For Market (though Pv here might be placeholder)
#include "Date.h"        // For Date operations
#include <algorithm>    // For std::max
#include <stdexcept>    // For std::logic_error

// --- AmericanOption Method Implementations ---

// Default constructor
AmericanOption::AmericanOption()
    : TreeProduct(""), // Call TreeProduct constructor
      optionType(Call),
      strike(0.0),
      expiryDate(Date()), // Default date
      discountCurveName("USD-SOFR"),
      volatilityCurveName("VOL_CURVE_DEFAULT") {
    tradeType = "AmericanOption"; // Set trade type from Trade base class
}

AmericanOption::AmericanOption(OptionType optType,
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
    tradeType = "AmericanOption"; // Set trade type
}

// Present Value: For AmericanOption, pricing is done by a Pricer (e.g., Binomial Tree).
// This Pv method is a placeholder as there's no simple analytical formula like BS for Americans.
// The Pricer will call Payoff, GetExpiry, ValueAtNode.
double AmericanOption::Pv(const Market& mkt) const {
    // American options are typically valued using numerical methods like binomial trees.
    // This Pv method itself doesn't perform that calculation; the Pricer does.
    // Returning 0.0 or indicating it needs a specific pricer is appropriate here.
    // For the sake of having a value, and knowing it will be overridden by Pricer's calculation:
    // std::cerr << "Warning: AmericanOption::Pv called directly. Should be priced via a Pricer for tree valuation. Returning 0 for now." << std::endl;
    // Or, one could throw an error:
    // throw std::logic_error("AmericanOption::Pv should not be called directly; use a Pricer (e.g., Binomial Tree).");
    // For the purpose of this project, it's understood the CRRBinomialTreePricer will be used.
    // This function will not be the primary source of its PV in that workflow.
    return 0.0; // Placeholder, actual value comes from Pricer
}

// Payoff function
double AmericanOption::Payoff(double S) const {
    return PAYOFF::VanillaOption(optionType, strike, S);
}

// GetExpiry function
const Date& AmericanOption::GetExpiry() const {
    return expiryDate;
}

// ValueAtNode for American option: includes early exercise decision
double AmericanOption::ValueAtNode(double S, double t, double continuation) const {
    // t (current time at node) is not explicitly used here but is part of the tree pricer's context.
    return std::max(Payoff(S), continuation);
}

// Note: AmerCallSpread, if defined, would be implemented similarly.
