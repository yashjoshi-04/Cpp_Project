#ifndef AMERICAN_TRADE_H 
#define AMERICAN_TRADE_H

#include "TreeProduct.h" 
#include "Payoff.h"      
#include "Types.h"       
#include "Market.h"      
#include <string>
#include <algorithm>    // For std::max

class AmericanOption : public TreeProduct {
public:
    AmericanOption(); 
    AmericanOption(OptionType optType,
                   double strikePrice,
                   const Date& expiryDt,
                   const std::string& underlyingInstName,
                   const std::string& discountCurveName = "USD-SOFR", 
                   const std::string& volCurveName = "VOL_CURVE_DEFAULT");

    
    double Pv(const Market& mkt) const override; 
    double Payoff(double S) const override;
    const Date& GetExpiry() const override;
    
    double ValueAtNode(double S, double t, double continuation) const override;

    
    Date getMaturityDate() const override { return GetExpiry(); }
    
    std::string getRateCurveName() const override { return discountCurveName; }
    std::string getVolCurveName() const override { return volatilityCurveName; }

    
    OptionType getOptionType() const { return optionType; }
    double getStrike() const { return strike; }

protected:
    OptionType optionType;
    double strike;
    Date expiryDate; 
    
    std::string discountCurveName;
    std::string volatilityCurveName;
};


#endif // AMERICAN_TRADE_H
//Updated
