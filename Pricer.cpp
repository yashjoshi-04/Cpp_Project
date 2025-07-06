#include <cmath>
#include <iostream> // For std::cerr
#include <vector>   // For std::vector in PriceBond
#include "Pricer.h"
#include "Market.h"
#include "Date.h"   // For Date operations in PriceBond

// Pricer base class
double Pricer::Price(const Market& mkt, Trade* trade) {
    double pv = 0.0;
    if (TreeProduct* treePtr = dynamic_cast<TreeProduct*>(trade)) {
        pv = PriceTree(mkt, *treePtr);
    }
    else if (Bond* bondPtr = dynamic_cast<Bond*>(trade)) {
        pv = PriceBond(mkt, bondPtr); // Call dedicated bond pricing method
    }
    else if (Swap* swapPtr = dynamic_cast<Swap*>(trade)) {
        double marketRateForFloatLeg = 0.0;
        RateCurve yieldCurve = mkt.getCurve("USD_Yield_Curve");
        if (!yieldCurve.isEmpty()) {
            marketRateForFloatLeg = yieldCurve.getRate(swapPtr->getEndDate());
        }
        else {
            std::cerr << "Warning: Swap pricing using 'USD_Yield_Curve' but it is missing or empty for swap ending "
                << swapPtr->getEndDate() << ". Using 0.0 as market rate for floating leg." << std::endl;
        }
        pv = swapPtr->Payoff(marketRateForFloatLeg);
    }
    else {
        std::cerr << "Error: Unsupported trade type in Pricer::Price: " << trade->getType() << std::endl;
    }
    return pv;
}

// Default implementation for PriceBond in base Pricer class (e.g., can be overridden)
// Or could be pure virtual if all concrete pricers must implement it.
// For now, providing a base DCF implementation here.
double Pricer::PriceBond(const Market& mkt, Bond* bond) {
    Date valuationDate = mkt.asOf;
    Date bondStartDate = bond->getStartDate();
    Date maturityDate = bond->getEndDate();
    double bondNotional = bond->getNotional();
    double couponRate = bond->getCouponRate();
    int couponFreq = bond->getCouponFreq();

    if (couponFreq <= 0) {
        std::cerr << "Error: Bond " << bond->getInstrumentName() << " has invalid coupon frequency: " << couponFreq << std::endl;
        return 0.0;
    }

    double totalNpv = 0.0;
    double couponAmount = (couponRate / couponFreq) * bondNotional;

    RateCurve yieldCurve = mkt.getCurve("USD_Yield_Curve");
    if (yieldCurve.isEmpty()) {
        std::cerr << "Error: USD_Yield_Curve not found or empty. Cannot price bond " << bond->getInstrumentName() << " theoretically." << std::endl;
        return 0.0;
    }

    Date currentPaymentDate = bondStartDate;

    // Find the first coupon payment date on or after the valuationDate,
    // but also respecting the bond's coupon schedule starting from bondStartDate.
    // This loop advances currentPaymentDate to the first period end.
    // Then it keeps advancing until it's >= valuationDate.

    // First, align currentPaymentDate to the first coupon date *after* bondStartDate
    // or on bondStartDate if it's a coupon date (e.g. for ex-coupon valuation, though not typical here)
    // This simplified loop assumes coupons are paid at end of periods starting from bondStartDate.

    std::vector<Date> payment_dates;
    Date temp_coupon_date = bondStartDate;

    // Generate all potential coupon dates from bondStartDate up to maturityDate
    while (temp_coupon_date < maturityDate) {
        int months_to_add = 12 / couponFreq;
        temp_coupon_date.month += months_to_add;
        while (temp_coupon_date.month > 12) {
            temp_coupon_date.month -= 12;
            temp_coupon_date.year++;
        }
        if (temp_coupon_date <= maturityDate) {
            payment_dates.push_back(temp_coupon_date);
        }
        else {
            break;
        }
    }

    // Discount future coupon payments
    for (const auto& pDate : payment_dates) {
        if (pDate > valuationDate) {
            double timeToCashFlow = pDate - valuationDate;
            double discountRate = yieldCurve.getRate(pDate);
            double df = std::exp(-discountRate * timeToCashFlow);
            totalNpv += couponAmount * df;
        }
    }

    // Discount principal + final coupon at maturityDate
    if (maturityDate > valuationDate) {
        double timeToMaturity = maturityDate - valuationDate;

        // Check if the last generated coupon date was maturity. If not, this is the final coupon.
        // Or, more simply, assume a coupon is always paid with principal at maturity.
        double finalCouponAmount = couponAmount;
        double principalAmount = bondNotional;
        double finalCashFlowAmount = principalAmount + finalCouponAmount;

        double discountRateAtMaturity = yieldCurve.getRate(maturityDate);
        double dfMaturity = std::exp(-discountRateAtMaturity * timeToMaturity);
        totalNpv += finalCashFlowAmount * dfMaturity;
    }
    else if (maturityDate == valuationDate) {
        // If valuing on maturity, principal + coupon are due (undiscounted)
        // This assumes the coupon on maturity date wasn't paid by the loop if pDate > valuationDate was strictly used
        // However, the problem might imply that if valuationDate == maturityDate, the bond has just paid/settled.
        // For simplicity, if value on maturity, it's par + coupon.
        totalNpv += bondNotional + couponAmount;
    }

    return totalNpv;
}


// BinomialTreePricer implementation
void BinomialTreePricer::ModelSetup(double S0, double sigma, double r, double dt)
{
    this->dT_ = dt;
    double sqrt_dt = std::sqrt(dt);
    u = std::exp(sigma * sqrt_dt);
    d = std::exp(-sigma * sqrt_dt);

    if (std::abs(u - d) < 1e-14) {
        p = (r >= 0) ? 1.0 : 0.0;
        if (std::abs(r) < 1e-14 && std::abs(sigma) < 1e-14) p = 0.5; // If r=0 and sigma=0, p=0.5
    }
    else {
        p = (std::exp(r * dt) - d) / (u - d);
    }
    if (p < 0.0) p = 0.0;
    if (p > 1.0) p = 1.0;
    currentSpot = S0;
    Df_ = std::exp(-r * dt);
}

double BinomialTreePricer::PriceTree(const Market& mkt, const TreeProduct& trade) {
    double T = trade.GetExpiry() - mkt.asOf;
    if (T < 1e-6 && T > -1e-6) { // If effectively zero time to expiry, or already past
        // If T is very small positive, dt might become 0 if nTimeSteps is large, leading to issues.
        // Handle T=0 as intrinsic value. If T<0, also intrinsic (though negative T is odd).
       // std::cout << "DEBUG: Option " << trade.getUnderlying() << " T=" << T << ". Returning intrinsic." << std::endl;
        return trade.Payoff(mkt.getStockPrice(trade.getUnderlying()));
    }
    if (T < 0) { // Strictly past expiry
        // std::cerr << "Warning: Option " << trade.getUnderlying() << " has expired (T=" << T << "). Returning intrinsic value (likely 0)." << std::endl;
        return trade.Payoff(mkt.getStockPrice(trade.getUnderlying()));
    }
    if (nTimeSteps <= 0) {
        std::cerr << "Error: Number of time steps in BinomialTreePricer must be positive." << std::endl;
        return 0.0;
    }
    double dt = T / nTimeSteps;

    double stockPrice = mkt.getStockPrice(trade.getUnderlying());
    double vol = mkt.getVolCurve("SPX_Vol_Curve").getVol(trade.GetExpiry());

    double effectiveRate;
    if (fixedRiskFreeRate_ >= 0.0) {
        effectiveRate = fixedRiskFreeRate_;
    }
    else {
        RateCurve yieldCurve = mkt.getCurve("USD_Yield_Curve");
        if (yieldCurve.isEmpty()) {
            std::cerr << "Warning: 'USD_Yield_Curve' not found or empty in BinomialTreePricer::PriceTree for " << trade.getUnderlying()
                << ". Defaulting option risk-free rate to 0.0." << std::endl;
            effectiveRate = 0.0;
        }
        else {
            effectiveRate = yieldCurve.getRate(trade.GetExpiry());
        }
    }

    ModelSetup(stockPrice, vol, effectiveRate, dt);

    for (int i = 0; i <= nTimeSteps; i++) {
        states[i] = trade.Payoff(GetSpot(nTimeSteps, i));
    }

    for (int k = nTimeSteps - 1; k >= 0; k--) {
        for (int i = 0; i <= k; i++) {
            double continuation = Df_ * (states[i] * GetProbUp() + states[i + 1] * GetProbDown());
            states[i] = trade.ValueAtNode(GetSpot(k, i), dt * k, continuation);
        }
    }
    return states[0];
}

// CRRBinomialTreePricer implementation
void CRRBinomialTreePricer::ModelSetup(double S0, double sigma, double r, double dt)
{
    this->dT_ = dt;
    currentSpot = S0;
    double sqrt_dt = std::sqrt(dt);
    u = std::exp(sigma * sqrt_dt);
    d = std::exp(-sigma * sqrt_dt);

    if (std::abs(u - d) < 1e-14) {
        p = (r >= 0) ? 1.0 : 0.0;
        if (std::abs(r) < 1e-14 && std::abs(sigma) < 1e-14) p = 0.5;
    }
    else {
        p = (std::exp(r * dt) - d) / (u - d);
    }
    if (p < 0.0) p = 0.0;
    if (p > 1.0) p = 1.0;
    Df_ = std::exp(-r * dt);
}

void CRRBinomialTreePricer::ModelSetup(const Market& mkt, const TreeProduct& product) {
    double S0 = mkt.getStockPrice(product.getUnderlying());
    double sigma = mkt.getVolCurve("SPX_Vol_Curve").getVol(product.GetExpiry());
    double T = product.GetExpiry() - mkt.asOf;

    this->dT_ = (nTimeSteps > 0 && T >= 0) ? T / nTimeSteps : 0; // Ensure dT is not negative

    this->currentSpot = S0;
    this->u = std::exp(sigma * std::sqrt(this->dT_));
    this->d = std::exp(-sigma * std::sqrt(this->dT_));

    double effectiveRate;
    if (this->fixedRiskFreeRate_ >= 0.0) {
        effectiveRate = this->fixedRiskFreeRate_;
    }
    else {
        RateCurve yieldCurve = mkt.getCurve("USD_Yield_Curve");
        if (yieldCurve.isEmpty()) {
            std::cerr << "Warning: 'USD_Yield_Curve' not found or empty in CRR::ModelSetup for " << product.getUnderlying()
                << ". Defaulting rfr to 0.0." << std::endl;
            effectiveRate = 0.0;
        }
        else {
            effectiveRate = yieldCurve.getRate(product.GetExpiry());
        }
    }

    if (std::abs(this->u - this->d) < 1e-14) {
        this->p = (effectiveRate >= 0) ? 1.0 : 0.0;
        if (std::abs(effectiveRate) < 1e-14 && std::abs(sigma) < 1e-14) this->p = 0.5;
    }
    else {
        this->p = (std::exp(effectiveRate * this->dT_) - this->d) / (this->u - this->d);
    }

    if (this->p < 0.0) this->p = 0.0;
    if (this->p > 1.0) this->p = 1.0;
    this->Df_ = std::exp(-effectiveRate * this->dT_);
}

double CRRBinomialTreePricer::PriceTree(const Market& mkt, const TreeProduct& trade) {
    double T = trade.GetExpiry() - mkt.asOf;
    if (T < 1e-6 && T > -1e-6) { // Effectively zero or past expiry
        // std::cerr << "Warning: Option " << trade.getUnderlying() << " T~0. Returning intrinsic." << std::endl;
        return trade.Payoff(mkt.getStockPrice(trade.getUnderlying()));
    }
    if (T < 0) {
        // std::cerr << "Warning: Option " << trade.getUnderlying() << " has expired (T=" << T << "). Returning intrinsic value." << std::endl;
        return trade.Payoff(mkt.getStockPrice(trade.getUnderlying()));
    }
    if (nTimeSteps <= 0) {
        std::cerr << "Error: Number of time steps in CRRBinomialTreePricer must be positive." << std::endl;
        return 0.0;
    }
    double dt = T / nTimeSteps;

    ModelSetup(mkt, trade);

    for (int i = 0; i <= nTimeSteps; i++) {
        states[i] = trade.Payoff(GetSpot(nTimeSteps, i));
    }

    for (int k = nTimeSteps - 1; k >= 0; k--) {
        for (int i = 0; i <= k; i++) {
            double continuation = Df_ * (states[i] * GetProbUp() + states[i + 1] * GetProbDown());
            states[i] = trade.ValueAtNode(GetSpot(k, i), dt * k, continuation);
        }
    }
    return states[0];
}

// JRRNBinomialTreePricer implementation
void JRRNBinomialTreePricer::ModelSetup(double S0, double sigma, double rate, double dt)
{
    this->dT_ = dt;
    u = std::exp((rate - sigma * sigma / 2) * dt + sigma * std::sqrt(dt));
    d = std::exp((rate - sigma * sigma / 2) * dt - sigma * std::sqrt(dt));
    if (std::abs(u - d) < 1e-14) {
        p = (rate >= 0) ? 1.0 : 0.0;
        if (std::abs(rate) < 1e-14 && std::abs(sigma) < 1e-14) p = 0.5;
    }
    else {
        p = (std::exp(rate * dt) - d) / (u - d);
    }
    if (p < 0.0) p = 0.0;
    if (p > 1.0) p = 1.0;
    currentSpot = S0;
    Df_ = std::exp(-rate * dt);
}