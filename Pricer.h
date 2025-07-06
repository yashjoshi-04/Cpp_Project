#ifndef PRICER_H
#define PRICER_H

#include <vector>
#include <cmath>
#include <memory>
#include <string>

// Forward declarations
class Market;
class Trade;
class TreeProduct;
class Bond;
class Swap;

class Pricer {
public:
    virtual ~Pricer() = default;
    virtual double Price(const Market& mkt, const std::shared_ptr<Trade>& trade) const;

protected:
    // PriceTree is the core method for options, to be implemented by derived tree pricers.
    // It's marked const because the pricer itself (its configuration like N_steps) doesn't change per call,
    // but it will need to calculate tree parameters (u,d,p,etc.) based on mkt and product.
    // These calculations will happen inside PriceTree or a helper it calls.
    virtual double PriceTree(const Market& mkt, const TreeProduct& product) const = 0;
};

class BinomialTreePricer : public Pricer {
public:
    BinomialTreePricer(int nSteps) : N(nSteps) {}

protected:
    // These members will be populated by a specific model's setup logic (e.g., CRR's ModelSetup)
    // They are effectively 'cached' or 'configured' per product pricing call within PriceTree.
    // Making them mutable allows PriceTree (which is const) to modify them via a helper method.
    mutable int N; // Number of time steps
    mutable double deltaT; // Time step duration
    mutable double u;      // Up factor
    mutable double d;      // Down factor
    mutable double p_up;   // Risk-neutral probability of up move
    mutable double p_down; // Risk-neutral probability of down move
    mutable double df_step; // Discount factor per step

    // Pure virtual method for specific model (CRR, JRR) to set up these parameters
    virtual void SetupTreeParams(const Market& mkt, const TreeProduct& product) const = 0;
};

class CRRBinomialTreePricer : public BinomialTreePricer {
public:
    CRRBinomialTreePricer(int nSteps) : BinomialTreePricer(nSteps) {}

    // Main pricing method for tree products using CRR model
    double PriceTree(const Market& mkt, const TreeProduct& product) const override;

protected:
    // CRR-specific implementation to set u, d, p, etc.
    void SetupTreeParams(const Market& mkt, const TreeProduct& product) const override;
};

#endif // PRICER_H
//Updated
