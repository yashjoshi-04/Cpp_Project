#include "Pricer.h"
#include "Market.h"
#include "Trade.h"
#include "TreeProduct.h" 
#include "Bond.h"        
#include "Swap.h"        
#include "EuropeanTrade.h" 
#include "AmericanTrade.h" 

#include <vector>
#include <cmath>     
#include <algorithm> 
#include <stdexcept> 
#include <iostream>  

double Pricer::Price(const Market& mkt, const std::shared_ptr<Trade>& trade) const {
    if (!trade) {
        throw std::invalid_argument("Trade pointer is null in Pricer::Price");
    }

    if (auto bond = std::dynamic_pointer_cast<const Bond>(trade)) {
        return bond->Pv(mkt); 
    }
    if (auto swap = std::dynamic_pointer_cast<const Swap>(trade)) {
        return swap->Pv(mkt); 
    }
    if (auto treeProduct = std::dynamic_pointer_cast<const TreeProduct>(trade)) {
        return this->PriceTree(mkt, *treeProduct);
    }
    
    std::cerr << "Warning: Pricer::Price could not determine specific pricing method for trade type: " 
              << trade->getType() << ". Attempting generic Trade::Pv." << std::endl;
    return trade->Pv(mkt); 
}

void CRRBinomialTreePricer::SetupTreeParams(const Market& mkt, const TreeProduct& product) const {
    Date valuationDate = mkt.asOf;
    Date expiryDate = product.GetExpiry();
    double T = (expiryDate - valuationDate); 

    if (T < -1e-9) { 
        throw std::runtime_error("Option already expired in CRRBinomialTreePricer::SetupTreeParams.");
    }
    T = std::max(0.0, T); // Clamp T to be non-negative

    this->deltaT = (this->N == 0) ? 0 : T / this->N; 

    std::shared_ptr<const RateCurve> rCurve = mkt.getCurve(product.getRateCurveName());
    std::shared_ptr<const VolCurve> vCurve = mkt.getVolCurve(product.getVolCurveName());

    if (!rCurve || rCurve->isEmpty()) {
        throw std::runtime_error("Rate curve '" + product.getRateCurveName() + "' not found or empty for tree setup.");
    }
    if (!vCurve || vCurve->isEmpty()) {
        throw std::runtime_error("Volatility curve '" + product.getVolCurveName() + "' not found or empty for tree setup.");
    }

    double r = rCurve->getRate(expiryDate);    
    double sigma = vCurve->getVol(expiryDate); 

    if (this->deltaT > 1e-9) { // Avoid division by zero or issues if T=0 (N might be >0)
        this->u = std::exp(sigma * std::sqrt(deltaT));
        this->d = 1.0 / u;
        double a = std::exp(r * deltaT); 
        this->p_up = (a - d) / (u - d);
        if (this->u == this->d) { // Avoid division by zero if u=d (e.g. sigma=0)
             this->p_up = (a >=d) ? 1.0 : 0.0; // if a=d, then p_up can be anything, conventionally 0.5 or based on limit.
                                            // if sigma is 0, u=d=1. a = exp(r*dT). p_up = (exp(r*dT)-1)/0. This needs care.
                                            // If sigma=0, tree is deterministic S0*exp(rT). Price is discounted payoff.
             if (std::abs(sigma) < 1e-9) { // If sigma is effectively zero
                 this->p_up = (a >= 1.0) ? 1.0 : 0.0; // Simplified: if rate positive, goes up; else down/neutral. Better: use deterministic pricing.
             } else {
                  throw std::runtime_error("CRR Tree: u and d are equal but sigma is not zero. Check parameters.");
             }
        }
        this->p_down = 1.0 - p_up;
        this->df_step = std::exp(-r * deltaT);

        if (p_up < -1e-6 || p_up > 1.0 + 1e-6) { // Allow small tolerance for floating point errors
            std::cerr << "Warning: CRR risk-neutral probability p_up (" << p_up << ") is out of [0,1] range significantly. Clamping. Check parameters (r=" << r << ", sigma=" << sigma << ", dT=" << deltaT << ", u=" << u << ", d=" << d << ", a=" << a << ")." << std::endl;
            this->p_up = std::max(0.0, std::min(1.0, this->p_up));
            this->p_down = 1.0 - this->p_up;
        }
    } else { // T=0 or N=0
        this->u = 1.0;
        this->d = 1.0;
        this->p_up = 0.5; // Arbitrary, won't be used if N=0 or T=0
        this->p_down = 0.5;
        this->df_step = 1.0;
    }
}

double CRRBinomialTreePricer::PriceTree(const Market& mkt, const TreeProduct& product) const {
    double S0 = mkt.getStockPrice(product.getUnderlyingName());
    if (S0 < 0) {
        throw std::runtime_error("Initial stock price cannot be negative.");
    }

    SetupTreeParams(mkt, product); // Sets mutable members u, d, p_up, p_down, deltaT, df_step

    if (this->N == 0 || this->deltaT <= 1e-9) { // If N is 0 or time step is zero (option on expiry)
        return product.Payoff(S0);
    }

    std::vector<double> V(N + 1); // Option values at current time step, going backwards

    // Initialize values at expiry (time N)
    for (int j = 0; j <= N; ++j) { // j is number of up steps
        double S_expiry_node = S0 * std::pow(this->u, j) * std::pow(this->d, N - j);
        V[j] = product.Payoff(S_expiry_node);
    }

    // Backward induction
    for (int i = N - 1; i >= 0; --i) { // Time step i
        for (int j = 0; j <= i; ++j) { // Node j at time step i (j up steps)
            double S_node = S0 * std::pow(this->u, j) * std::pow(this->d, i - j);
            double continuation_value = (this->p_up * V[j + 1] + this->p_down * V[j]) * this->df_step;
            V[j] = product.ValueAtNode(S_node, i * this->deltaT, continuation_value);
        }
    }
    return V[0]; // Value at t=0, node 0 (0 up steps)
}

//Updated
