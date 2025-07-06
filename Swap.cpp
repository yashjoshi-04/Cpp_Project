#include "Swap.h"
#include "Date.h"
#include "Market.h"
#include <cmath>     // For std::exp
#include <stdexcept> // For std::runtime_error
#include <iostream>  // For cerr

// Constructor
Swap::Swap(const std::string& underlyingName_in,
           const Date& effectiveDate_in,
           const Date& maturityDate_in,
           double notional_in,
           double fixedRate_in,
           int paymentFrequency_in,
           const std::string& fixedLegDiscCurve,
           const std::string& floatLegFcstCurve)
    : Trade("Swap", effectiveDate_in), // Base class: type and valuation/booking date
      underlyingName(underlyingName_in),
      effectiveDate(effectiveDate_in),
      maturityDate(maturityDate_in),
      notional(notional_in), // Positive for Rec Fixed, Negative for Pay Fixed
      fixedRate(fixedRate_in),
      paymentFrequency(paymentFrequency_in),
      fixedLegDiscountCurveName(fixedLegDiscCurve),
      floatLegForecastCurveName(floatLegFcstCurve) {

    if (paymentFrequency <= 0) {
        throw std::invalid_argument("Swap payment frequency must be positive.");
    }
    if (maturityDate <= effectiveDate) {
        throw std::invalid_argument("Swap maturity date must be after effective date.");
    }
    generateSwapSchedule(); // Generate schedule upon construction
}

// Generate fixed leg payment schedule
void Swap::generateSwapSchedule() {
    if (effectiveDate >= maturityDate || paymentFrequency <= 0) {
        // This check is also in constructor, but good for direct calls too
        // Clear schedule and return if invalid params for schedule gen
        fixedLegSchedule.clear();
        return;
    }

    std::string tenorStr;
    if (paymentFrequency == 1) tenorStr = "12M";
    else if (paymentFrequency == 2) tenorStr = "6M";
    else if (paymentFrequency == 4) tenorStr = "3M";
    else if (paymentFrequency == 12) tenorStr = "1M";
    else {
        // For non-standard frequencies, the behavior of dateAddTenor with e.g. "2.4M" is undefined.
        // Throw error or handle with more complex logic if such frequencies are expected.
        throw std::runtime_error("Unsupported payment frequency for schedule generation: " + std::to_string(paymentFrequency));
    }

    fixedLegSchedule.clear();
    Date current_accrual_end_date = effectiveDate;
    // The first period's start is effectiveDate. First payment is at end of first period.
    // The schedule typically stores payment dates (accrual end dates).
    // The prompt's sample code `generateSwapSchedule` in the problem description adds `seed` (startDate) first.
    // And then `maturityDate` last. `swapSchedule.push_back(seed); ... swapSchedule.push_back(maturityDate);`
    // This implies the schedule stores period start and end dates, or just all relevant dates.
    // Let's follow the sample: it adds start date, then accrual end dates, and finally maturity date if not already last.
    
    // The sample code from the prompt for Swap::generateSwapSchedule adds the start date, 
    // then subsequent period end dates, and ensures maturity date is the last date.
    // It is used to calculate tau = (swapSchedule[i] - swapSchedule[i-1]) / 360.0
    // So fixedLegSchedule should store all period boundaries: start, intermediate payment dates, end.

    fixedLegSchedule.push_back(effectiveDate); 
    Date next_payment_date = effectiveDate;
    while (next_payment_date < maturityDate) {
        next_payment_date = dateAddTenor(next_payment_date, tenorStr);
        if (next_payment_date > maturityDate) {
            // If adding the full tenor overshoots, the last payment is at maturityDate.
            // The schedule should include maturityDate as the final date.
            break; // We will add maturityDate explicitly later if it's not the last generated date.
        }
        fixedLegSchedule.push_back(next_payment_date);
    }
    
    // Ensure maturityDate is the last date in the schedule if it wasn't naturally hit
    if (fixedLegSchedule.empty() || fixedLegSchedule.back() < maturityDate) {
        // If the loop didn't add maturityDate (e.g., if it overshot or schedule was empty)
        // or if last date is before maturity (e.g. if dateAddTenor undershot somehow)
        // We need to ensure maturityDate is present.
        // If last date < maturity, remove it if it's an intermediate calc and add maturity.
        // However, if schedule.back() is a valid payment date before maturity, it should stay.
        // The simplest is to check if maturity is already last. If not, add it.
        if (fixedLegSchedule.back() != maturityDate) { // Avoid duplicate if last date is already maturity
             fixedLegSchedule.push_back(maturityDate);
        }
    } else if (fixedLegSchedule.back() > maturityDate) {
        // If the last generated date overshot maturity, replace it with maturityDate.
        fixedLegSchedule.back() = maturityDate;
    }

    // Remove duplicates that might arise if dateAddTenor lands exactly on maturityDate already pushed
    if (fixedLegSchedule.size() >= 2) {
        auto it = std::unique(fixedLegSchedule.begin(), fixedLegSchedule.end());
        fixedLegSchedule.erase(it, fixedLegSchedule.end());
    }

    if (fixedLegSchedule.size() < 2) {
        // A valid schedule for PV calculation needs at least a start and end date.
        // This might happen if effectiveDate == maturityDate, caught by constructor.
        // Or if logic above fails for some edge case.
        fixedLegSchedule.clear(); // Make it clearly invalid
        throw std::runtime_error("Error: Invalid swap schedule generated, less than 2 dates.");
    }
}

// Get Annuity of the fixed leg
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

    // Loop through payment periods. Schedule contains period boundary dates.
    // Payment is made at fixedLegSchedule[i], for period (fixedLegSchedule[i-1], fixedLegSchedule[i])
    for (size_t i = 1; i < fixedLegSchedule.size(); ++i) {
        const Date& periodStartDate = fixedLegSchedule[i-1];
        const Date& periodEndDate = fixedLegSchedule[i]; // This is the payment date

        if (periodEndDate <= valuationDate) { // Payment date is in the past or today, skip
            continue;
        }

        // Year fraction for the period (tau)
        // The prompt's sample uses (endDate - startDate) / 360.0
        double tau = (periodEndDate - periodStartDate); // This uses our Date::operator- which is days/365
                                                      // To match sample: (endDate.getSerialDate() - startDate.getSerialDate()) / 360.0;
        tau = static_cast<double>(periodEndDate.getSerialDate() - periodStartDate.getSerialDate()) / 360.0; // As per sample
        if (tau <= 1e-9) continue; // Skip zero-length periods

        double T_to_pmt = (periodEndDate - valuationDate); // Time from valuation to payment
        double df = std::exp(-discCurve->getRate(periodEndDate) * T_to_pmt);
        
        // Annuity formula from prompt: sum of (N * coupon period(i) * DF (i))
        // Here notional is the trade notional. If it's Pay Fixed, notional is < 0.
        // Annuity is typically defined with positive notional.
        // The sample code uses `notional` directly which implies it's `abs(notional)` or specific to fixed leg's notional.
        // Let's use abs(notional) for standard annuity definition.
        annuity += std::abs(notional) * tau * df;
    }
    return annuity;
}

// PV calculation for Swap
double Swap::Pv(const Market& mkt) const {
    // Using the cash flow discounting method from the prompt's sample code for Swap::Pv
    Date valuationDate = mkt.asOf;
    if (valuationDate >= maturityDate || fixedLegSchedule.size() < 2) {
        return 0.0; // Swap matured or schedule invalid
    }

    std::shared_ptr<const RateCurve> discCurve = mkt.getCurve(fixedLegDiscountCurveName);
    // For par rate, it could be floatLegForecastCurveName, but sample PV doesn't use par rate method explicitly.
    // The sample PV uses the same discount curve for both legs implicitly.
    if (!discCurve || discCurve->isEmpty()) {
        std::cerr << "Error: Discount curve '" << fixedLegDiscountCurveName << "' not found or empty for Swap PV." << std::endl;
        return 0.0;
    }

    // Fixed Leg PV calculation
    double fixedLegPv = 0.0;
    for (size_t i = 1; i < fixedLegSchedule.size(); ++i) {
        const Date& periodStartDate = fixedLegSchedule[i-1];
        const Date& periodEndDate = fixedLegSchedule[i]; // Payment date for the period

        if (periodEndDate <= valuationDate) { // Payment in the past
            continue;
        }

        double tau = static_cast<double>(periodEndDate.getSerialDate() - periodStartDate.getSerialDate()) / 360.0; // Year fraction as per sample
        if (tau <= 1e-9) continue;

        double T_to_pmt = (periodEndDate - valuationDate); // Time from valuation to payment
        double df = std::exp(-discCurve->getRate(periodEndDate) * T_to_pmt);
        
        // Fixed leg cash flow: notional * fixedRate * tau
        // `notional` is positive for Rec Fixed, negative for Pay Fixed.
        double fixed_cashflow = notional * fixedRate * tau;
        fixedLegPv += fixed_cashflow * df;
    }

    // Floating Leg PV calculation (simplified as per prompt's sample)
    // Float Leg PV = Notional_float - Notional_float * Discount Factor (at end date of floating leg)
    // If our `notional` is for fixed leg (Rec Fixed if >0), then float leg is Pay Float (notional_float = -this.notional)
    double floatLegNotional = -this->notional; // If fixed leg is Rec, float is Pay, and vice-versa.
    
    double T_maturity = (maturityDate - valuationDate);
    double df_maturity = std::exp(-discCurve->getRate(maturityDate) * T_maturity);
    
    // The sample code had: fltPv = (-notional + notional * df);
    // If notional was fixed leg notional (e.g. +1m for Rec Fixed), then -notional is Pay Float principal.
    // So, -notional * (1 - df_maturity) for a Pay Float leg.
    // This is equivalent to: (-notional) + notional * df_maturity.
    // Let's use the formula structure: Payer leg PV = N * (DF_end - 1), Receiver leg PV = N * (1 - DF_end)
    // If our notional is +1m (Rec Fixed), then float leg is Pay Float, notional_float = -1m.
    // PV Pay Float = (-1m) * (DF_maturity - 1) = 1m * (1 - DF_maturity)
    // If our notional is -1m (Pay Fixed), then float leg is Rec Float, notional_float = +1m.
    // PV Rec Float = (1m) * (1 - DF_maturity)
    // So, PV Float = abs(notional) * (1 - DF_maturity) * sign_of_float_leg_notional
    // sign_of_float_leg_notional is -sign(this.notional)
    // PV Float = abs(notional) * (1 - DF_maturity) * (-sign(this.notional))
    // This is equivalent to: this.notional * (DF_maturity - 1) 
    // Or, using the sample's variable `fltPv = (-notional + notional * df_maturity)` where `notional` is fixed leg notional.
    // This means if `notional` > 0 (Rec Fixed), then `fltPv` = `(-N + N*df)` -> paying float leg.
    // If `notional` < 0 (Pay Fixed), then `fltPv` = `(-(-N) + (-N)*df)` = `(N - N*df)` -> receiving float leg.
    // This matches the convention. So we use the sample's formula directly.
    double floatingLegPv = (-this->notional) + (this->notional * df_maturity); 

    return fixedLegPv + floatingLegPv;
}

// Get Maturity Date
Date Swap::getMaturityDate() const {
    return maturityDate;
}

// Get Underlying Name
std::string Swap::getUnderlyingName() const {
    return underlyingName;
}

// Get Primary Discount Curve Name (used for fixed leg and annuity)
std::string Swap::getRateCurveName() const {
    return fixedLegDiscountCurveName;
}

// Payoff function - Less relevant for standard swap PV using DCF.
// Could represent MTM change for a given market rate move if marketRate is some par rate.
// For compliance, return 0 or a simple value.
double Swap::Payoff(double marketRate) const {
    // Not directly used for DCF based PV.
    // Could be (fixedRate - marketRate) * Annuity if marketRate is a comparable par rate.
    return 0.0; 
}
//Updated
