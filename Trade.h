#pragma once // Often used, equivalent to #ifndef TRADE_H #define TRADE_H ... #endif

#ifndef TRADE_H
#define TRADE_H

#include <string>
#include <memory> // For std::shared_ptr later if needed for composition
#include "Date.h"
#include "Market.h" // Market reference for Pv function

// Forward declare Pricer if it's used in a way that causes circular dependency
// class Pricer; 

class Trade {
public:
    Trade() : tradeType("UnknownTrade"), tradeDate(Date()) {}; // Default constructor
    Trade(const std::string& type, const Date& date)
        : tradeType(type), tradeDate(date) {};
    
    virtual ~Trade() = default; // Important for base class with virtual functions

    std::string getType() const { return tradeType; } // Made const
    Date getTradeDate() const { return tradeDate; } // Made const

    // Pure virtual function for Present Value calculation
    virtual double Pv(const Market& mkt) const = 0;

    // Virtual function for payoff (might be specific to options or certain trade types)
    // For some trades like Bonds/Swaps, Payoff might not be as relevant as cashflow generation for Pv.
    // If it's only for options, it could be in a more derived base like OptionTrade.
    // Keeping it as per existing structure for now.
    virtual double Payoff(double marketPrice) const = 0; 

    // Virtual function to get the maturity date of the trade
    virtual Date getMaturityDate() const = 0;

    // Virtual functions to get relevant market data names for risk calculation
    // Provide default empty string implementations; derivatives can override.
    virtual std::string getUnderlyingName() const { return ""; }
    virtual std::string getRateCurveName() const { return "USD-SOFR"; } // Example default, override as needed
    virtual std::string getVolCurveName() const { return ""; }   // Example default, override as needed

protected:   
    std::string tradeType;
    Date tradeDate;
    // Consider adding a unique trade ID if managing many trades
    // std::string tradeId;
};

#endif // TRADE_H

//Updated
