#ifndef TRADE_FACTORY_H
#define TRADE_FACTORY_H

#include <string>
#include <memory> // For std::shared_ptr, std::make_shared
#include "Trade.h"   // For std::shared_ptr<Trade>
#include "Date.h"
#include "Types.h"   // For OptionType

// Specific trade types needed for concrete factories
#include "Bond.h"
#include "Swap.h"
#include "EuropeanTrade.h" // For EuropeanOption
#include "AmericanTrade.h" // For AmericanOption

// Abstract Trade Factory
class TradeFactory {
public:
    virtual ~TradeFactory() = default;
    // A more generic factory method might take a map or struct of parameters.
    // For this project, using a method with common params and some specific ones.
    // `strike_or_coupon`: For options, it's strike. For bonds, it's coupon. For swaps, it's fixed rate.
    // `price_or_dummy`: For bonds, it could be a clean/dirty price if loading from a source that provides it.
    //                   For options/swaps, it might not be used directly in construction if Pv is calculated.
    // `underlying_or_bondName`: For options, underlying. For bonds, bond name. For swaps, descriptive name.
    // `discount_curve_name`, `vol_curve_name`: For options and bonds/swaps to find their market data.
    virtual std::shared_ptr<Trade> createTrade(
        const std::string& underlying_or_bondName,
        const Date& tradeDate,      // Booking date
        const Date& startDate,        // Effective/Issue/Accrual Start Date
        const Date& endDate,          // Maturity/Expiry Date
        double notional,
        double strike_or_coupon_or_fixedRate, // Primary rate/strike parameter
        int frequency,                // Coupon/Payment frequency for Bonds/Swaps; 0 for options
        OptionType optionType,        // Call, Put, or None for non-options
        const std::string& discount_curve_name,
        const std::string& vol_curve_name,
        const std::string& float_leg_forecast_curve_name = "" // Specific to swaps
    ) = 0;
};

// Concrete Factory for Bonds
class BondFactory : public TradeFactory {
public:
    std::shared_ptr<Trade> createTrade(
        const std::string& underlying_or_bondName,
        const Date& tradeDate,
        const Date& startDate,
        const Date& endDate,
        double notional,
        double strike_or_coupon_or_fixedRate, // This is couponRate for Bond
        int frequency,
        OptionType optionType, // Not used for Bond, can be ignored or assert type is None
        const std::string& discount_curve_name,
        const std::string& vol_curve_name, // Not used for Bond
        const std::string& float_leg_forecast_curve_name = ""
    ) override {
        // `tradeDate` from params is booking date, `startDate` is issueDate for Bond constructor
        return std::make_shared<Bond>(
            underlying_or_bondName, // instrumentName
            startDate,              // issueDate for Bond constructor
            endDate,                // maturityDate
            notional,
            strike_or_coupon_or_fixedRate, // couponRate
            frequency,
            discount_curve_name
        );
    }
};

// Concrete Factory for Swaps
class SwapFactory : public TradeFactory {
public:
    std::shared_ptr<Trade> createTrade(
        const std::string& underlying_or_bondName, // This is a descriptive name for Swap
        const Date& tradeDate,      // Booking date
        const Date& startDate,        // EffectiveDate for Swap constructor
        const Date& endDate,          // MaturityDate
        double notional,
        double strike_or_coupon_or_fixedRate, // This is fixedRate for Swap
        int frequency,
        OptionType optionType, // Not used for Swap
        const std::string& discount_curve_name, // Used for fixed leg and discounting
        const std::string& vol_curve_name, // Not used for Swap
        const std::string& float_leg_forecast_curve_name = "" // Can be same as discount_curve or different
    ) override {
        std::string float_curve_to_use = float_leg_forecast_curve_name.empty() ? discount_curve_name : float_leg_forecast_curve_name;
        return std::make_shared<Swap>(
            underlying_or_bondName, // underlyingName for Swap
            startDate,              // effectiveDate
            endDate,                // maturityDate
            notional,
            strike_or_coupon_or_fixedRate, // fixedRate
            frequency,
            discount_curve_name,     // fixedLegDiscountCurveName
            float_curve_to_use       // floatLegForecastCurveName
        );
    }
};

// Concrete Factory for European Options
class EuropeanOptionFactory : public TradeFactory {
public:
    std::shared_ptr<Trade> createTrade(
        const std::string& underlying_or_bondName, // This is underlyingInstrumentName for Option
        const Date& tradeDate,      // Booking date
        const Date& startDate,        // Not directly used by EuropeanOption constructor (expiry is key)
        const Date& endDate,          // This is expiryDate for Option
        double notional,           // Can represent number of contracts/shares
        double strike_or_coupon_or_fixedRate, // This is strike for Option
        int frequency,            // Not used for Option (can be 0)
        OptionType optionType,
        const std::string& discount_curve_name,
        const std::string& vol_curve_name,
        const std::string& float_leg_forecast_curve_name = ""
    ) override {
        // `notional` for options usually means number of shares/units, not a monetary value for PV directly.
        // The EuropeanOption constructor doesn't take notional. If needed, it should be added or handled differently.
        // For now, assuming notional is informational or used outside the direct pricing model of a single option unit.
        return std::make_shared<EuropeanOption>(
            optionType,
            strike_or_coupon_or_fixedRate, // strikePrice
            endDate,                       // expiryDate
            underlying_or_bondName,        // underlyingInstrumentName
            discount_curve_name,
            vol_curve_name
        );
    }
};

// Concrete Factory for American Options
class AmericanOptionFactory : public TradeFactory {
public:
    std::shared_ptr<Trade> createTrade(
        const std::string& underlying_or_bondName, // This is underlyingInstrumentName for Option
        const Date& tradeDate,
        const Date& startDate,       // Not directly used by AmericanOption constructor
        const Date& endDate,         // This is expiryDate for Option
        double notional,          // Similar to EuropeanOption, notional is informational here
        double strike_or_coupon_or_fixedRate, // This is strike for Option
        int frequency,           // Not used for Option
        OptionType optionType,
        const std::string& discount_curve_name,
        const std::string& vol_curve_name,
        const std::string& float_leg_forecast_curve_name = ""
    ) override {
        return std::make_shared<AmericanOption>(
            optionType,
            strike_or_coupon_or_fixedRate, // strikePrice
            endDate,                       // expiryDate
            underlying_or_bondName,        // underlyingInstrumentName
            discount_curve_name,
            vol_curve_name
        );
    }
};

#endif // TRADE_FACTORY_H
