#include "RiskEngine.h"
#include "Trade.h"            
#include "Market.h"           
#include "Pricer.h"           
#include "MarketDecorators.h" 

#include <iostream> 

RiskEngine::RiskEngine(double default_curve_shock_abs,
                       double default_vol_shock_abs)
    : defaultCurveShockAmount(default_curve_shock_abs),
      defaultVolShockAmount(default_vol_shock_abs) {}

std::map<std::string, double> RiskEngine::computeDv01(
    const std::shared_ptr<Trade>& trade,
    const Market& originalMarket,
    const Pricer& pricer) const {
    
    std::map<std::string, double> dv01_results;
    if (!trade) {
        std::cerr << "Error: Null trade provided to computeDv01." << std::endl;
        return dv01_results;
    }

    std::string rateCurveName = trade->getRateCurveName();
    if (rateCurveName.empty() || rateCurveName == "NONE" || rateCurveName == "na") {
        return dv01_results; 
    }

    if (!originalMarket.getCurve(rateCurveName)) {
        std::cerr << "Warning: Rate curve '" << rateCurveName << "' for DV01 not found in the original market for trade " 
                  << trade->getUnderlyingName() << " (" << trade->getType() << "). Skipping DV01." << std::endl;
        return dv01_results;
    }

    MarketShock shockDetails;
    shockDetails.market_id = rateCurveName;
    shockDetails.shock_value = defaultCurveShockAmount; 

    CurveDecorator shockedMarkets(originalMarket, shockDetails);

    // [[maybe_unused]] double pv_original = pricer.Price(originalMarket, trade); 
    double pv_up = pricer.Price(shockedMarkets.getMarketUp(), trade);
    double pv_down = pricer.Price(shockedMarkets.getMarketDown(), trade);

    double dv01 = (pv_up - pv_down) / 2.0;
    
    dv01_results[rateCurveName] = dv01;

    return dv01_results;
}

std::map<std::string, double> RiskEngine::computeVega(
    const std::shared_ptr<Trade>& trade,
    const Market& originalMarket,
    const Pricer& pricer) const {

    std::map<std::string, double> vega_results;
    if (!trade) {
        std::cerr << "Error: Null trade provided to computeVega." << std::endl;
        return vega_results;
    }

    std::string volCurveName = trade->getVolCurveName();
    if (volCurveName.empty() || volCurveName == "NONE" || volCurveName == "na") {
        return vega_results; 
    }

    if (!originalMarket.getVolCurve(volCurveName)) {
        std::cerr << "Warning: Volatility curve '" << volCurveName << "' for Vega not found in the original market for trade " 
                  << trade->getUnderlyingName() << " (" << trade->getType() << "). Skipping Vega." << std::endl;
        return vega_results;
    }

    MarketShock shockDetails;
    shockDetails.market_id = volCurveName;
    shockDetails.shock_value = defaultVolShockAmount; 

    VolDecorator shockedVolMarkets(originalMarket, shockDetails);

    // [[maybe_unused]] double pv_original = pricer.Price(originalMarket, trade);
    double pv_up = pricer.Price(shockedVolMarkets.getMarketUp(), trade);
    double pv_down = pricer.Price(shockedVolMarkets.getMarketDown(), trade);

    double vega = (pv_up - pv_down) / 2.0;
    
    vega_results[volCurveName] = vega;

    return vega_results;
}
