#ifndef TRADE_FACTORY_H
#define TRADE_FACTORY_H

#include <string>
#include <memory> 
#include "Trade.h"   
#include "Date.h"
#include "Types.h"   

#include "Bond.h"
#include "Swap.h"
#include "EuropeanTrade.h" 
#include "AmericanTrade.h" 

class TradeFactory {
public:
    virtual ~TradeFactory() = default;
    virtual std::shared_ptr<Trade> createTrade(
        const std::string& underlying_or_bondName,
        const Date& tradeDate,      
        const Date& startDate,        
        const Date& endDate,          
        double notional,
        double strike_or_coupon_or_fixedRate, 
        int frequency,                
        OptionType optionType,        
        const std::string& discount_curve_name,
        const std::string& vol_curve_name,
        const std::string& float_leg_forecast_curve_name = "" 
    ) = 0;
};

class BondFactory : public TradeFactory {
public:
    std::shared_ptr<Trade> createTrade(
        const std::string& underlying_or_bondName,
        const Date& /*tradeDate*/,      // Unused in Bond constructor directly, issueDate is startDate
        const Date& startDate,
        const Date& endDate,
        double notional,
        double strike_or_coupon_or_fixedRate, 
        int frequency,
        OptionType /*optionType*/, // Unused by Bond constructor
        const std::string& discount_curve_name,
        const std::string& /*vol_curve_name*/, // Unused by Bond constructor
        const std::string& /*float_leg_forecast_curve_name*/ // Unused by Bond constructor
    ) override {
        return std::make_shared<Bond>(
            underlying_or_bondName, 
            startDate,              
            endDate,                
            notional,
            strike_or_coupon_or_fixedRate, 
            frequency,
            discount_curve_name
        );
    }
};

class SwapFactory : public TradeFactory {
public:
    std::shared_ptr<Trade> createTrade(
        const std::string& underlying_or_bondName, 
        const Date& /*tradeDate*/,      // Unused in Swap constructor, effectiveDate is startDate
        const Date& startDate,        
        const Date& endDate,          
        double notional,
        double strike_or_coupon_or_fixedRate, 
        int frequency,
        OptionType /*optionType*/, // Unused by Swap constructor
        const std::string& discount_curve_name, 
        const std::string& /*vol_curve_name*/, // Unused by Swap constructor
        const std::string& float_leg_forecast_curve_name 
    ) override {
        std::string float_curve_to_use = float_leg_forecast_curve_name.empty() ? discount_curve_name : float_leg_forecast_curve_name;
        return std::make_shared<Swap>(
            underlying_or_bondName, 
            startDate,              
            endDate,                
            notional,
            strike_or_coupon_or_fixedRate, 
            frequency,
            discount_curve_name,     
            float_curve_to_use       
        );
    }
};

class EuropeanOptionFactory : public TradeFactory {
public:
    std::shared_ptr<Trade> createTrade(
        const std::string& underlying_or_bondName, 
        const Date& /*tradeDate*/,      // Unused by EuropeanOption constructor
        const Date& /*startDate*/,        // Unused by EuropeanOption constructor
        const Date& endDate,          
        double /*notional*/,           // Notional currently unused by EuropeanOption constructor
        double strike_or_coupon_or_fixedRate, 
        int /*frequency*/,            // Unused by EuropeanOption constructor
        OptionType optionType,
        const std::string& discount_curve_name,
        const std::string& vol_curve_name,
        const std::string& /*float_leg_forecast_curve_name*/ // Unused
    ) override {
        return std::make_shared<EuropeanOption>(
            optionType,
            strike_or_coupon_or_fixedRate, 
            endDate,                       
            underlying_or_bondName,        
            discount_curve_name,
            vol_curve_name
        );
    }
};

class AmericanOptionFactory : public TradeFactory {
public:
    std::shared_ptr<Trade> createTrade(
        const std::string& underlying_or_bondName, 
        const Date& /*tradeDate*/,      // Unused by AmericanOption constructor
        const Date& /*startDate*/,       // Unused by AmericanOption constructor
        const Date& endDate,         
        double /*notional*/,          // Notional currently unused by AmericanOption constructor
        double strike_or_coupon_or_fixedRate, 
        int /*frequency*/,           // Unused by AmericanOption constructor
        OptionType optionType,
        const std::string& discount_curve_name,
        const std::string& vol_curve_name,
        const std::string& /*float_leg_forecast_curve_name*/ // Unused
    ) override {
        return std::make_shared<AmericanOption>(
            optionType,
            strike_or_coupon_or_fixedRate, 
            endDate,                       
            underlying_or_bondName,        
            discount_curve_name,
            vol_curve_name
        );
    }
};

#endif // TRADE_FACTORY_H

//Updated Again
