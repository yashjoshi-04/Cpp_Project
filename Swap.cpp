#include "Swap.h"
#include "Date.h"
#include "Market.h"
#include <cmath>     
#include <stdexcept> 
#include <iostream>  
#include <algorithm> // For std::unique, std::sort (if needed for schedule)

Swap::Swap(const std::string& underlyingName_in,
           const Date& effectiveDate_in,
           const Date& maturityDate_in,
           double notional_in,
           double fixedRate_in,
           int paymentFrequency_in,
           const std::string& fixedLegDiscCurve,
           const std::string& floatLegFcstCurve)
    : Trade("Swap", effectiveDate_in), 
      underlyingName(underlyingName_in),
      effectiveDate(effectiveDate_in),
      maturityDate(maturityDate_in),
      notional(notional_in), 
      fixedRate(fixedRate_in),
      paymentFrequency(paymentFrequency_in),
      fixedLegDiscountCurveName(fixedLegDiscCurve),
      floatLegForecastCurveName(floatLegFcstCurve) {

    if (paymentFrequency <= 0) {
        throw std::invalid_argument("Swap payment frequency must be positive.");
    }
    // Corrected comparison: !(effectiveDate < maturityDate) means effectiveDate >= maturityDate
    if (!(effectiveDate < maturityDate)) {
        throw std::invalid_argument("Swap maturity date must be after effective date.");
    }
    generateSwapSchedule(); 
}

void Swap::generateSwapSchedule() {
    // Corrected comparison
    if (!(effectiveDate < maturityDate) || paymentFrequency <= 0) {
        fixedLegSchedule.clear();
        return;
    }

    std::string tenorStr;
    if (paymentFrequency == 1) tenorStr = "12M";
    else if (paymentFrequency == 2) tenorStr = "6M";
    else if (paymentFrequency == 4) tenorStr = "3M";
    else if (paymentFrequency == 12) tenorStr = "1M";
    else {
        throw std::runtime_error("Unsupported payment frequency for swap schedule generation: " + std::to_string(paymentFrequency));
    }

    fixedLegSchedule.clear();
    fixedLegSchedule.push_back(effectiveDate); 
    Date next_payment_date = effectiveDate;
    while (next_payment_date < maturityDate) {
        Date temp_next_payment_date = dateAddTenor(next_payment_date, tenorStr);
        // If adding the full tenor overshoots or lands on maturity, set to maturityDate.
        if (!(temp_next_payment_date < maturityDate) || temp_next_payment_date == maturityDate) { // temp_next_payment_date >= maturityDate
            next_payment_date = maturityDate;
        } else {
            next_payment_date = temp_next_payment_date;
        }
        fixedLegSchedule.push_back(next_payment_date);
        if (next_payment_date == maturityDate) break; // Stop if we hit maturity exactly
    }
    
    // Clean up potential duplicates if maturityDate was hit exactly by dateAddTenor and also added
    if (fixedLegSchedule.size() >= 2) {
        auto it = std::unique(fixedLegSchedule.begin(), fixedLegSchedule.end());
        fixedLegSchedule.erase(it, fixedLegSchedule.end());
    }

    if (fixedLegSchedule.size() < 2 || fixedLegSchedule.back() != maturityDate) {
         // Ensure maturity date is the absolute last date if schedule is valid but doesn't end on it
         if (fixedLegSchedule.size() >=1 && fixedLegSchedule.back() < maturityDate) {
            fixedLegSchedule.push_back(maturityDate);
         } else if (fixedLegSchedule.size() < 2) {
            fixedLegSchedule.clear(); 
            throw std::runtime_error("Error: Invalid swap schedule generated, less than 2 dates or maturity not properly set.");
         }
    }
}

double Swap::getAnnuity(const Market& mkt) const {
    if (fixedLegSchedule.size() < 2) {
        std::cerr << "Warning: Swap schedule not generated or invalid for annuity calculation." << std::endl;
        return 0.0;
    }

    std::shared_ptr<const RateCurve> discCurve = mkt.getCurve(fixedLegDiscountCurveName);
    if (!discCurve || discCurve->isEmpty()) {
        std::cerr << "Error: Discount curve '" << fixedLegDiscountCurveName << "' not found or empty for annuity." << std::endl;
        return 0.0;
    }

    double annuity = 0.0;
    Date valuationDate = mkt.asOf;

    for (size_t i = 1; i < fixedLegSchedule.size(); ++i) {
        const Date& periodStartDate = fixedLegSchedule[i-1];
        const Date& periodEndDate = fixedLegSchedule[i]; 

        // Corrected comparison: periodEndDate <= valuationDate
        if (!(valuationDate < periodEndDate)) { 
            continue;
        }

        double tau = static_cast<double>(periodEndDate.getSerialDate() - periodStartDate.getSerialDate()) / 360.0; 
        if (tau <= 1e-9) continue; 

        double T_to_pmt = (periodEndDate - valuationDate); 
        double df = std::exp(-discCurve->getRate(periodEndDate) * T_to_pmt);
        
        annuity += std::abs(notional) * tau * df;
    }
    return annuity;
}

double Swap::Pv(const Market& mkt) const {
    Date valuationDate = mkt.asOf;
    // Corrected comparison: valuationDate >= maturityDate
    if (!(valuationDate < maturityDate) || fixedLegSchedule.size() < 2) {
        return 0.0; 
    }

    std::shared_ptr<const RateCurve> discCurve = mkt.getCurve(fixedLegDiscountCurveName);
    if (!discCurve || discCurve->isEmpty()) {
        std::cerr << "Error: Discount curve '" << fixedLegDiscountCurveName << "' not found or empty for Swap PV." << std::endl;
        return 0.0;
    }

    double fixedLegPv = 0.0;
    for (size_t i = 1; i < fixedLegSchedule.size(); ++i) {
        const Date& periodStartDate = fixedLegSchedule[i-1];
        const Date& periodEndDate = fixedLegSchedule[i]; 

        // Corrected comparison: periodEndDate <= valuationDate
        if (!(valuationDate < periodEndDate)) { 
            continue;
        }

        double tau = static_cast<double>(periodEndDate.getSerialDate() - periodStartDate.getSerialDate()) / 360.0; 
        if (tau <= 1e-9) continue;

        double T_to_pmt = (periodEndDate - valuationDate); 
        double df = std::exp(-discCurve->getRate(periodEndDate) * T_to_pmt);
        
        double fixed_cashflow = notional * fixedRate * tau;
        fixedLegPv += fixed_cashflow * df;
    }

    double T_maturity = (maturityDate - valuationDate);
    double df_maturity = std::exp(-discCurve->getRate(maturityDate) * T_maturity);
    
    double floatingLegPv = (-this->notional) + (this->notional * df_maturity); 

    return fixedLegPv + floatingLegPv;
}

Date Swap::getMaturityDate() const {
    return maturityDate;
}

std::string Swap::getUnderlyingName() const {
    return underlyingName;
}

std::string Swap::getRateCurveName() const {
    return fixedLegDiscountCurveName;
}

double Swap::Payoff([[maybe_unused]] double marketRate) const {
    return 0.0; 
}
