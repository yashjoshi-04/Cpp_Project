#include "Bond.h"
#include "Date.h"   
#include "Market.h" 
#include <cmath>   
#include <vector>
#include <stdexcept> 
#include <iostream>  
#include <algorithm> // For std::sort if generating complex schedules and sorting them

Bond::Bond(const std::string& instrumentName_in,
           const Date& issueDate_in, 
           const Date& maturityDt_in,
           double principalVal_in,
           double annualCouponRate_in,
           int freq_in,
           const std::string& discCurveName_in)
    : Trade("Bond", issueDate_in), 
      instrumentName(instrumentName_in),
      issueDate(issueDate_in),
      maturityDate(maturityDt_in),
      principal(principalVal_in),
      couponRate(annualCouponRate_in),
      couponFrequency(freq_in),
      discountCurveName(discCurveName_in) {

    if (couponFrequency <= 0 && couponRate > 1e-9) { 
        throw std::invalid_argument("Bond coupon frequency must be positive if coupon rate is non-zero.");
    }
    if (principal <= 1e-9) {
        throw std::invalid_argument("Bond principal must be positive.");
    }
    // It's okay for maturityDate == issueDate (zero coupon)
    // Valuation date vs maturity is checked in Pv()
}

Date Bond::getMaturityDate() const {
    return maturityDate;
}

std::string Bond::getUnderlyingName() const {
    return instrumentName;
}

std::string Bond::getRateCurveName() const {
    return discountCurveName;
}

double Bond::Payoff([[maybe_unused]] double marketPrice) const {
    return principal; 
}

double Bond::Pv(const Market& mkt) const {
    Date valuationDate = mkt.asOf;

    // Corrected comparison: !(valuationDate < maturityDate) is equivalent to valuationDate >= maturityDate
    if (!(valuationDate < maturityDate)) {
        return 0.0; 
    }

    std::shared_ptr<const RateCurve> rateCurve = mkt.getCurve(discountCurveName);
    if (!rateCurve || rateCurve->isEmpty()) {
        std::cerr << "Error: Discount curve '" << discountCurveName << "' not found or empty in market for bond " << instrumentName << std::endl;
        return 0.0; 
    }

    double pv = 0.0;
    double couponAmountPerPeriod = 0.0;
    if (couponFrequency > 0 && couponRate > 1e-9) {
        couponAmountPerPeriod = (couponRate / static_cast<double>(couponFrequency)) * principal;
    }

    std::vector<Date> paymentDates;
    if (couponFrequency > 0 && couponRate > 1e-9) {
        Date currentScheduleDate = issueDate;
        std::string tenorPeriod;
        bool standardFreq = true;
        if (couponFrequency == 1) tenorPeriod = "12M";
        else if (couponFrequency == 2) tenorPeriod = "6M";
        else if (couponFrequency == 4) tenorPeriod = "3M";
        else if (couponFrequency == 12) tenorPeriod = "1M";
        else {
            standardFreq = false;
            long approxDaysInPeriod = 365 / couponFrequency;
            if (approxDaysInPeriod <=0) approxDaysInPeriod = 1; // Avoid infinite loop
            Date prevDate = issueDate;
            currentScheduleDate = issueDate; // Start generating from issue date
            while (!(maturityDate < currentScheduleDate) && !(currentScheduleDate == maturityDate)) { // while currentScheduleDate <= maturityDate
                 Date nextPotentialDate = currentScheduleDate;
                 nextPotentialDate.setFromSerial(currentScheduleDate.getSerialDate() + approxDaysInPeriod);
                 if (!(nextPotentialDate < prevDate) && nextPotentialDate != prevDate) { // ensure forward movement
                    currentScheduleDate = nextPotentialDate;
                 } else { // Stuck, advance by one day
                    currentScheduleDate.setFromSerial(currentScheduleDate.getSerialDate() + 1);
                 }
                 prevDate = currentScheduleDate;

                 if (!(currentScheduleDate < valuationDate) && currentScheduleDate <= maturityDate) { 
                    paymentDates.push_back(currentScheduleDate);
                 }
                 if (currentScheduleDate == maturityDate) break;
            }
        }
        
        if (standardFreq){
            currentScheduleDate = issueDate; 
            while (!(maturityDate < currentScheduleDate)) { // while currentScheduleDate <= maturityDate
                Date nextPaymentDate = dateAddTenor(currentScheduleDate, tenorPeriod);
                // If next payment date exactly matches or overshoots maturity, adjust to maturity if not already there
                if (!(nextPaymentDate < maturityDate) || nextPaymentDate == maturityDate) { // nextPaymentDate >= maturityDate
                    if (!(maturityDate < valuationDate) && maturityDate != valuationDate) { // if maturityDate > valuationDate
                         // Add maturity date if it's not already the last payment date and it's a future date
                        if (paymentDates.empty() || paymentDates.back() < maturityDate) {
                            paymentDates.push_back(maturityDate);
                        }
                    }
                    break; // Exit loop after handling maturity
                }
                currentScheduleDate = nextPaymentDate;
                if (!(currentScheduleDate < valuationDate) && currentScheduleDate != valuationDate) { // currentScheduleDate > valuationDate
                    paymentDates.push_back(currentScheduleDate);
                }
            }
        }
        // Ensure unique sorted dates if logic above could produce disorder (unlikely with tenor adds)
        // std::sort(paymentDates.begin(), paymentDates.end());
        // paymentDates.erase(std::unique(paymentDates.begin(), paymentDates.end()), paymentDates.end());
    }

    for (const Date& pmtDate : paymentDates) {
        // This check is redundant if paymentDates only includes future dates, but safe
        if (!(pmtDate < valuationDate) && pmtDate != valuationDate) { // pmtDate > valuationDate
            double T = (pmtDate - valuationDate); 
            if (T < 0) T = 0; 
            double zeroRate = rateCurve->getRate(pmtDate);
            double df = std::exp(-zeroRate * T);
            pv += couponAmountPerPeriod * df;
        }
    }

    // Discount principal repayment at maturity
    // This should always happen if bond hasn't matured, even if no interim coupons
    if (!(maturityDate < valuationDate) && maturityDate != valuationDate) { // maturityDate > valuationDate
        double T_maturity = (maturityDate - valuationDate); 
        double zeroRate_maturity = rateCurve->getRate(maturityDate);
        double df_maturity = std::exp(-zeroRate_maturity * T_maturity);
        pv += principal * df_maturity;
    }
    
    return pv;
}
