#ifndef RISK_ENGINE_H
#define RISK_ENGINE_H

#include "Market.h"
#include "Trade.h"
#include "Pricer.h"
#include "MarketDecorators.h" // For MarketShock, CurveDecorator, VolDecorator

#include <string>
#include <vector>
#include <map>
#include <memory> // For std::shared_ptr
// For std::future, std::async (if implementing async computation later)
// #include <future> 

class RiskEngine {
public:
    // Constructor: takes default shock sizes for different risk types.
    // curve_shock_abs: absolute shock for IR curves (e.g., 0.0001 for 1bp)
    // vol_shock_abs: absolute shock for Vol curves (e.g., 0.01 for 1%)
    RiskEngine(double default_curve_shock_abs = 0.0001, 
               double default_vol_shock_abs = 0.01);

    // Computes DV01 for a given trade.
    // DV01 is sensitivity to a 1bp parallel shift in the relevant interest rate curve.
    // Returns a map: {curve_name: dv01_value}
    std::map<std::string, double> computeDv01(const std::shared_ptr<Trade>& trade,
                                              const Market& originalMarket,
                                              const Pricer& pricer) const;

    // Computes Vega for a given trade.
    // Vega is sensitivity to a 1% parallel shift in the relevant volatility curve.
    // Returns a map: {vol_curve_name: vega_value}
    std::map<std::string, double> computeVega(const std::shared_ptr<Trade>& trade,
                                              const Market& originalMarket,
                                              const Pricer& pricer) const;

    // The prompt's sample RiskEngine had a more generic computeRisk function.
    // void computeRisk(std::string riskType, std::shared_ptr<Trade> trade, bool singleThread);
    // For now, specific functions like computeDv01 and computeVega are clearer.
    // Multi-threading can be added as an enhancement to these functions later.

private:
    double defaultCurveShockAmount; // e.g., 0.0001 for 1bp
    double defaultVolShockAmount;   // e.g., 0.01 for 1%

    // The prompt's sample RiskEngine stored decorator objects.
    // An alternative is to create decorators on-the-fly within compute methods.
    // This avoids storing potentially many shocked market states if not needed persistently.
    // std::unordered_map<std::string, CurveDecorator> curveShocks; 
    // std::unordered_map<std::string, VolDecorator> volShocks;
}; 

#endif // RISK_ENGINE_H
