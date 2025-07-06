#include "Swap.h"
#include <cmath>
#include <vector>
#include <numeric>

// Constructor implementation
Swap::Swap(const Date& tradeDate, Date start, Date end, double notional, double rate, int freq) 
    : Trade("SwapTrade", tradeDate), startDate(start), endDate(end), swapNotional(notional), tradeRate(rate), frequency(freq)
{
    // All members initialized in the initializer list
}

// Implement getAnnuity() where assuming zero rate is 4% per annum for discounting.
double Swap::getAnnuity() const {
    double annuity = 0.0;
    double zeroRate = 0.04; // 4% per annum for discounting as per requirement

    std::vector<Date> paymentDates;
    Date tempDate = startDate;
    while (tempDate - endDate <= 0) {
        paymentDates.push_back(tempDate);
        // Advance date by coupon frequency
        tempDate.month += (12 / frequency);
        if (tempDate.month > 12) {
            tempDate.year += (tempDate.month / 12);
            tempDate.month = (tempDate.month % 12 == 0) ? 12 : (tempDate.month % 12);
        }
    }

    for (const auto& paymentDate : paymentDates) {
        if (paymentDate - tradeDate > 0) { // Only consider future cash flows
            double timeToPayment = paymentDate - tradeDate; // Time in years
            double yearFraction = (1.0 / frequency); // Year fraction for each period
            annuity += yearFraction * std::exp(-zeroRate * timeToPayment);
        }
    }
    return annuity;
}

// Implement Payoff, using npv = annuity * (traded rate - market swap rate);
double Swap::Payoff(double marketRate) const { // Removed inline
    double annuity = getAnnuity();
    return annuity * swapNotional * (tradeRate - marketRate);
}
