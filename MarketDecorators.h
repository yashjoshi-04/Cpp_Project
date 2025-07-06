#ifndef MARKET_DECORATORS_H
#define MARKET_DECORATORS_H

#include "Market.h"
#include "Date.h"
#include <string>
#include <utility> // For std::pair
#include <memory>  // For std::make_shared

// Structure to define a market shock
// For parallel shocks, tenor in 'shock' pair might be ignored or default Date() if not used.
// shock_value is the amount to bump (e.g., 0.0001 for 1bp, 0.01 for 1% vol).
struct MarketShock {
    std::string market_id; // Name of the curve/vol surface to shock (e.g., "USD-SOFR", "VOL_CURVE_AAPL")
    // Date tenor; // Specific tenor to shock - not used for parallel shock as per prompt
    double shock_value; // The actual bump amount (e.g., +0.0001 or -0.0001)
};

// Base class for Market Decorators (optional, but can be good practice if more decorators arise)
// For this project, CurveDecorator and VolDecorator can directly use/modify Market objects.
// The prompt's sample CurveDecorator directly holds two Market objects (up/down).

// Decorator for Interest Rate Curve shocks
class CurveDecorator {
public:
    // Constructor takes the original market and the shock details.
    // It creates two new Market objects: one for bumped up, one for bumped down.
    CurveDecorator(const Market& original_market, const MarketShock& curve_shock_details) 
        : marketUp(original_market), marketDown(original_market) {
        
        // Apply upward shock
        std::shared_ptr<RateCurve> curveToShockUp = marketUp.getCurve(curve_shock_details.market_id);
        if (curveToShockUp) {
            curveToShockUp->shock(curve_shock_details.shock_value); // Parallel shock
        } else {
            std::cerr << "Warning: CurveDecorator could not find curve '" << curve_shock_details.market_id << "' in marketUp to shock." << std::endl;
        }

        // Apply downward shock
        std::shared_ptr<RateCurve> curveToShockDown = marketDown.getCurve(curve_shock_details.market_id);
        if (curveToShockDown) {
            curveToShockDown->shock(-curve_shock_details.shock_value); // Parallel shock (opposite direction)
        } else {
            std::cerr << "Warning: CurveDecorator could not find curve '" << curve_shock_details.market_id << "' in marketDown to shock." << std::endl;
        }
    }

    const Market& getMarketUp() const { return marketUp; }
    const Market& getMarketDown() const { return marketDown; }

private:
    Market marketUp;   // A deep copy of the original market, with one curve bumped up
    Market marketDown; // A deep copy of the original market, with one curve bumped down
};

// Decorator for Volatility Curve shocks
class VolDecorator {
public:
    VolDecorator(const Market& original_market, const MarketShock& vol_shock_details)
        : marketUp(original_market), marketDown(original_market) {

        // Apply upward shock
        std::shared_ptr<VolCurve> volToShockUp = marketUp.getVolCurve(vol_shock_details.market_id);
        if (volToShockUp) {
            volToShockUp->shock(vol_shock_details.shock_value);
        } else {
            std::cerr << "Warning: VolDecorator could not find vol curve '" << vol_shock_details.market_id << "' in marketUp to shock." << std::endl;
        }

        // Apply downward shock
        std::shared_ptr<VolCurve> volToShockDown = marketDown.getVolCurve(vol_shock_details.market_id);
        if (volToShockDown) {
            volToShockDown->shock(-vol_shock_details.shock_value);
        } else {
            std::cerr << "Warning: VolDecorator could not find vol curve '" << vol_shock_details.market_id << "' in marketDown to shock." << std::endl;
        }
    }

    const Market& getMarketUp() const { return marketUp; }
    const Market& getMarketDown() const { return marketDown; }

private:
    Market marketUp;
    Market marketDown;
};

// Note: The prompt's sample VolDecorator only had one market member and shocked it directly.
// For consistency with central difference (PV(up) - PV(down)), having separate up/down markets is cleaner.
// The prompt sample CurveDecorator used two markets, which is better for central differences.

#endif // MARKET_DECORATORS_H
