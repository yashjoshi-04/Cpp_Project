#ifndef SWAP_H
#define SWAP_H

#include "Trade.h"
#include "Market.h"
#include <string>
#include <vector>

class Swap : public Trade {
public:
    // Constructor
    Swap(const std::string& underlyingName, // e.g., "USD-SOFR-VS-FIXED"
         const Date& effectiveDate,      // Start date of the swap (tradeDate from base can be booking date)
         const Date& maturityDate,
         double notional,                // Positive for receive fixed, negative for pay fixed
         double fixedRate,               // The fixed rate of the swap
         int paymentFrequency,          // Payments per year for the fixed leg (e.g., 1 for annual, 2 for semi-annual)
         const std::string& fixedLegDiscountCurve, // Discount curve for fixed leg & annuity
         const std::string& floatLegForecastCurve  // Potentially a different curve for forecasting float leg rates (e.g. par rate)
         );

    // Override virtual functions from Trade
    double Pv(const Market& mkt) const override;
    double Payoff(double marketRate) const override; // For a swap, payoff at a point in time isn't typical, PV is key.
    Date getMaturityDate() const override;
    std::string getUnderlyingName() const override; 
    std::string getRateCurveName() const override;  // Primary discount curve (e.g., for fixed leg)
    // getVolCurveName() will return empty string as swaps are not directly sensitive to vol in this model.

    // Swap specific methods
    double getNotional() const { return notional; }
    double getFixedRate() const { return fixedRate; }
    int getPaymentFrequency() const { return paymentFrequency; }

private:
    void generateSwapSchedule(); // Generates fixed leg payment schedule
    // Annuity calculation based on the fixed leg's schedule and discount factors
    double getAnnuity(const Market& mkt) const; 

    std::string underlyingName;
    Date effectiveDate; // Swap actual start date
    Date maturityDate;  // Swap end date
    double notional;    // Can be negative for payer swaps
    double fixedRate;   // Coupon rate of the fixed leg
    int paymentFrequency; // Number of fixed payments per year
    
    std::string fixedLegDiscountCurveName;
    std::string floatLegForecastCurveName; // May be same as fixedLegDiscountCurveName

    std::vector<Date> fixedLegSchedule; // Populated by generateSwapSchedule
};

#endif // SWAP_H

//Updated
