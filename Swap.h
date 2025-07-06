#pragma once
#include "Trade.h"

class Swap : public Trade {
public:
	//make necessary change
	Swap(const Date& tradeDate, Date start, Date end, double notional, double rate, int freq); // Removed body

	double Payoff(double marketRate) const; // Removed inline and body

private:
	Date startDate;
	Date endDate;
	double swapNotional;
	double tradeRate;
	int frequency; // use 1 for annual, 2 for semi-annual etc

	double getAnnuity() const; // Declaration only, implementation in .cpp
public: // Added public getters for necessary members
    inline const Date& getEndDate() const { return endDate; }
};
