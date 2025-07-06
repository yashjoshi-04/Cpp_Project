#pragma once
#include<string>
#include "Date.h"
#include "Market.h"

using namespace std;

class Trade {
public:
    Trade(){};
    Trade(const string& _type, const Date& _tradeDate)
        : tradeType(_type), tradeDate(_tradeDate) {};
    inline string getType() { return tradeType; };
    //virtual double Price(const Market& market) const = 0;
    virtual double Payoff(double marketPrice) const = 0;
    virtual ~Trade() {};

protected:   
    string tradeType;
    Date tradeDate;
};