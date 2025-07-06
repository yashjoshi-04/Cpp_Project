#ifndef EUROPEAN_TRADE_H // Standard include guard
#define EUROPEAN_TRADE_H

#include "TreeProduct.h" // Base class
#include "Payoff.h"      // For PAYOFF::VanillaOption
#include "Types.h"       // For OptionType
#include "Market.h"      // For Market in Pv
#include <string>
#include <algorithm>    // For std::max (though not needed for European ValueAtNode)

class EuropeanOption : public TreeProduct {
public:
    EuropeanOption(); // Default constructor
    EuropeanOption(OptionType optType,
                   double strikePrice,
                   const Date& expiryDt,
                   const std::string& underlyingInstName,
                   const std::string& discountCurveName = "USD-SOFR", // Default curve names
                   const std::string& volCurveName = "VOL_CURVE_DEFAULT");

    // Virtual functions from Trade / TreeProduct
    double Pv(const Market& mkt) const override;
    double Payoff(double S) const override;
    const Date& GetExpiry() const override;
    double ValueAtNode(double S, double t, double continuation) const override;

    // Overrides from Trade base
    Date getMaturityDate() const override { return GetExpiry(); }
    // getUnderlyingName() is inherited from TreeProduct and should be fine if TreeProduct implements it well.
    // If TreeProduct::getUnderlying() needs to be overridden or is not suitable, implement here.
    // std::string getUnderlyingName() const override { return underlying; } // If 'underlying' is a member here or in TreeProduct
    std::string getRateCurveName() const override { return discountCurveName; }
    std::string getVolCurveName() const override { return volatilityCurveName; }

    // Specific getters for EuropeanOption properties
    OptionType getOptionType() const { return optionType; }
    double getStrike() const { return strike; }

protected:
    OptionType optionType;
    double strike;
    Date expiryDate; // TreeProduct also has an expiryDate, ensure consistency or pick one source
    // underlying is in TreeProduct
    std::string discountCurveName;
    std::string volatilityCurveName;
};

// EuroCallSpread is more complex, we'll focus on EuropeanOption first as per main requirements.
// If EuroCallSpread is needed, it would also need Pv, getMaturityDate, etc.
/*
class EuroCallSpread : public EuropeanOption { // Or directly from TreeProduct
public:
    EuroCallSpread(double k1, double k2, const Date& expiry, 
                   const std::string& underlyingInstName,
                   const std::string& discountCurveName = "USD-SOFR", 
                   const std::string& volCurveName = "VOL_CURVE_DEFAULT");

    double Pv(const Market& mkt) const override;
    double Payoff(double S) const override;
    // GetExpiry, ValueAtNode, getMaturityDate might be inherited or need overrides

private:
    double strike1;
    double strike2;
    // expiryDate inherited if EuropeanOption is base
    // discountCurveName, volatilityCurveName should also be members
};
*/

#endif // EUROPEAN_TRADE_H
//Updated
