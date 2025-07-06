#ifndef BOND_H // Standard include guard
#define BOND_H

#include "Trade.h"
#include "Market.h" // For Market in Pv
#include <string>
#include <vector>   // For cashflow schedule if generated internally

class Bond : public Trade {
public:
    // Constructor
    Bond(const std::string& instrumentName, 
         const Date& issueDate,         // Typically the trade date for a new issue
         const Date& maturityDate,
         double principal,             // Notional or Principal
         double couponRate,            // Annual coupon rate (e.g., 0.05 for 5%)
         int couponFrequency,         // Number of coupon payments per year (e.g., 1 for annual, 2 for semi-annual)
         const std::string& discountCurveName); // Name of the curve to use for discounting

    // Override virtual functions from Trade
    double Pv(const Market& mkt) const override;
    double Payoff(double marketPrice) const override; // May not be used if Pv is cashflow based
    Date getMaturityDate() const override;
    std::string getUnderlyingName() const override; // Bond name itself
    std::string getRateCurveName() const override;  // Discount curve for this bond

    // Specific getters for Bond properties (optional, but can be useful)
    double getPrincipal() const { return principal; }
    double getCouponRate() const { return couponRate; }
    int getCouponFrequency() const { return couponFrequency; }

private:
    std::string instrumentName; // e.g., "US Treasury Bond 2.5% 2030"
    Date issueDate;        // Could be same as tradeDate
    Date maturityDate;
    double principal;
    double couponRate;       // Annual rate
    int couponFrequency;    // Payments per year
    std::string discountCurveName; // Name of the rate curve to use for discounting

    // Helper to generate cashflow dates and amounts (optional internal detail)
    // struct Cashflow { Date paymentDate; double amount; };
    // std::vector<Cashflow> generateCashflows(const Date& settlementDate) const;
};

#endif // BOND_H
//Updated
