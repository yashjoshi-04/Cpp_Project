#pragma once
#include "Trade.h"
#include "Market.h"

class Bond : public Trade {

public:
	Bond(const std::string& name, const Date& tradeDate, const Date& start, const Date& end,
		double notional, double couponRate, int couponFreq, double price) : Trade("BondTrade", tradeDate) {
		bondName = name;
		startDate = start;
		endDate = end;
		bondNotional = notional;
		this->couponRate = couponRate;
		frequency = couponFreq;
		tradePrice = price;
	}

	double Payoff(double yield) const; // implement this as theoretical price

private:
	std::string bondName;
	double bondNotional;
	double couponRate;
	double tradePrice;
	int frequency; // coupon frequency per year, e.g., 2 for semi-annual
	Date startDate;
	Date endDate;

public: // Added public getters for necessary members
	inline const Date& getStartDate() const { return startDate; }
	inline const Date& getEndDate() const { return endDate; }
	inline double getNotional() const { return bondNotional; }
	inline double getCouponRate() const { return couponRate; }
	inline int getCouponFreq() const { return frequency; }
	inline const std::string& getInstrumentName() const { return bondName; } // Already present
	inline double getPrice() const { return tradePrice; } // Getter for MTM price
};