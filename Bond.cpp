#include "Bond.h"
#include "Date.h"   // For dateAddTenor, operator-, etc.
#include "Market.h" // For accessing RateCurve
#include <cmath>   // For std::exp
#include <vector>
#include <stdexcept> // For std::runtime_error
#include <iostream>  // For cerr

// Constructor
Bond::Bond(const std::string& instrumentName_in,
           const Date& issueDate_in, 
           const Date& maturityDt_in,
           double principalVal_in,
           double annualCouponRate_in,
           int freq_in,
           const std::string& discCurveName_in)
    : Trade("Bond", issueDate_in), // Base class constructor
      instrumentName(instrumentName_in),
      issueDate(issueDate_in),
      maturityDate(maturityDt_in),
      principal(principalVal_in),
      couponRate(annualCouponRate_in),
      couponFrequency(freq_in),
      discountCurveName(discCurveName_in) {

    if (couponFrequency <= 0 && couponRate > 1e-9) { // Use epsilon for double comparison
        throw std::invalid_argument("Bond coupon frequency must be positive if coupon rate is non-zero.");
    }
    if (principal <= 1e-9) {
        throw std::invalid_argument("Bond principal must be positive.");
    }
    if (maturityDate <= issueDate) {
        // Allow maturityDate == issueDate for zero-coupon bonds that mature on issue (edge case, but not strictly invalid)
        // More critical is maturityDate vs valuationDate for PV.
    }
}

// Get Maturity Date
Date Bond::getMaturityDate() const {
    return maturityDate;
}

// Get Underlying Name
std::string Bond::getUnderlyingName() const {
    return instrumentName;
}

// Get Discount Curve Name
std::string Bond::getRateCurveName() const {
    return discountCurveName;
}

double Bond::Payoff(double marketPrice) const {
    // This is not typically how a bond's value is determined if using DCF for Pv.
    // For compliance with the abstract base, returning principal.
    return principal; 
}

double Bond::Pv(const Market& mkt) const {
    Date valuationDate = mkt.asOf;

    if (valuationDate >= maturityDate) {
        return 0.0; // Bond has matured or is maturing today
    }

    std::shared_ptr<const RateCurve> rateCurve = mkt.getCurve(discountCurveName);
    if (!rateCurve || rateCurve->isEmpty()) {
        std::cerr << "Error: Discount curve '" << discountCurveName << "' not found or is empty in market for bond " << instrumentName << std::endl;
        return 0.0; 
    }

    double pv = 0.0;
    double couponAmountPerPeriod = 0.0;
    if (couponFrequency > 0 && couponRate > 1e-9) {
        couponAmountPerPeriod = (couponRate / static_cast<double>(couponFrequency)) * principal;
    }

    // Generate coupon payment dates
    std::vector<Date> paymentDates;
    if (couponFrequency > 0 && couponRate > 1e-9) {
        Date currentPaymentDate = issueDate; // Start from issue date to determine schedule
        std::string tenorPeriod;
        if (couponFrequency == 1) tenorPeriod = "12M";
        else if (couponFrequency == 2) tenorPeriod = "6M";
        else if (couponFrequency == 4) tenorPeriod = "3M";
        else if (couponFrequency == 12) tenorPeriod = "1M";
        else {
            // For non-standard frequencies, this simplified schedule might be inaccurate.
            // A more robust solution would use specific day count conventions and roll rules.
            long approxDaysInPeriod = 365 / couponFrequency;
            while(currentPaymentDate < maturityDate){
                currentPaymentDate.setFromSerial(currentPaymentDate.getSerialDate() + approxDaysInPeriod);
                if(currentPaymentDate > valuationDate && currentPaymentDate <= maturityDate) {
                    paymentDates.push_back(currentPaymentDate);
                }
            }
        }
        
        if (!tenorPeriod.empty()){ // Standard frequencies
             // First coupon date could be issueDate + period, or a specific first coupon date
             // For simplicity, assume regular payments starting from a point aligned with issue + period logic
            currentPaymentDate = issueDate; 
            while(currentPaymentDate < maturityDate){
                currentPaymentDate = dateAddTenor(currentPaymentDate, tenorPeriod);
                if(currentPaymentDate > valuationDate && currentPaymentDate <= maturityDate) {
                    paymentDates.push_back(currentPaymentDate);
                }
            }
        }
    }

    // Discount coupon payments
    for (const Date& pmtDate : paymentDates) {
        double T = (pmtDate - valuationDate); // Year fraction using overloaded operator-(Date,Date)/365.0
        if (T < -1e-9) continue; // Skip if somehow payment date is before valuation date (should be filtered by pmtDate > valuationDate check)
        if (T < 1e-9) T = 0.0; // Effectively today
        
        double zeroRate = rateCurve->getRate(pmtDate);
        double df = std::exp(-zeroRate * T);
        pv += couponAmountPerPeriod * df;
    }

    // Discount principal repayment at maturity
    double T_maturity = (maturityDate - valuationDate);
    if (T_maturity < -1e-9) T_maturity = 0.0; // Should not happen if valuationDate < maturityDate
    if (T_maturity < 1e-9 && T_maturity > -1e-9) T_maturity = 0.0; // If maturing today, T=0

    double zeroRate_maturity = rateCurve->getRate(maturityDate);
    double df_maturity = std::exp(-zeroRate_maturity * T_maturity);
    pv += principal * df_maturity;
    
    return pv;
}

//Updated
