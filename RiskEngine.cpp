#include "RiskEngine.h"
#include "Trade.h"            // For Trade interface (getRateCurveName, getVolCurveName)
#include "Market.h"           // For Market object
#include "Pricer.h"           // For Pricer interface
#include "MarketDecorators.h" // For CurveDecorator, VolDecorator, MarketShock

#include <iostream> // For std::cerr (optional warnings)

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
    if (rateCurveName.empty()) {
        // std::cerr << "Warning: Trade type '" << trade->getType() << "' (ID: " << trade->getUnderlyingName() 
        //           << ") does not provide a rate curve name for DV01 calculation. Skipping DV01." << std::endl;
        return dv01_results; // No specific curve to shock for this trade type
    }

    // Check if the curve actually exists in the market before trying to shock
    if (!originalMarket.getCurve(rateCurveName)) {
        std::cerr << "Warning: Rate curve '" << rateCurveName << "' for DV01 not found in the original market for trade " 
                  << trade->getUnderlyingName() << ". Skipping DV01." << std::endl;
        return dv01_results;
    }

    // Create MarketShock details for the identified curve
    // Upward shock is +defaultCurveShockAmount, Downward is -defaultCurveShockAmount
    // The CurveDecorator handles creating two markets, one for up, one for down.
    MarketShock shockDetails;
    shockDetails.market_id = rateCurveName;
    shockDetails.shock_value = defaultCurveShockAmount; // The magnitude of the bump (e.g., 0.0001)

    CurveDecorator shockedMarkets(originalMarket, shockDetails);

    double pv_original = pricer.Price(originalMarket, trade); // Optional: for reference or other calcs
    double pv_up = pricer.Price(shockedMarkets.getMarketUp(), trade);
    double pv_down = pricer.Price(shockedMarkets.getMarketDown(), trade);

    // DV01 = (PV(rate curve bumped up) - PV(rate curve bumped down)) / 2.0
    // This measures the change in PV for a +/- shock_value/2 scenario, effectively the sensitivity to the shock_value.
    // If shock_value is 0.0001 (1bp), this is the PV change for that 1bp parallel move.
    double dv01 = (pv_up - pv_down) / 2.0;
    
    // The result key could be just the curve name, or more descriptive.
    // E.g. "DV01:USD-SOFR"
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
    if (volCurveName.empty()) {
        // std::cerr << "Warning: Trade type '" << trade->getType() << "' (ID: " << trade->getUnderlyingName() 
        //           << ") does not provide a vol curve name for Vega calculation. Skipping Vega." << std::endl;
        return vega_results; // No specific vol curve for this trade
    }

    if (!originalMarket.getVolCurve(volCurveName)) {
        std::cerr << "Warning: Volatility curve '" << volCurveName << "' for Vega not found in the original market for trade " 
                  << trade->getUnderlyingName() << ". Skipping Vega." << std::endl;
        return vega_results;
    }

    MarketShock shockDetails;
    shockDetails.market_id = volCurveName;
    shockDetails.shock_value = defaultVolShockAmount; // e.g., 0.01 for 1% vol bump

    VolDecorator shockedVolMarkets(originalMarket, shockDetails);

    double pv_original = pricer.Price(originalMarket, trade);
    double pv_up = pricer.Price(shockedVolMarkets.getMarketUp(), trade);
    double pv_down = pricer.Price(shockedVolMarkets.getMarketDown(), trade);

    // Vega = (PV(vol curve bumped up) - PV(vol curve bumped down)) / 2.0
    // This is the PV change for a 1% parallel vol move if defaultVolShockAmount is 0.01.
    double vega = (pv_up - pv_down) / 2.0;
    
    vega_results[volCurveName] = vega;

    return vega_results;
}
