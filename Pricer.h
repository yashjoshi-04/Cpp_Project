#ifndef _PRICER
#define _PRICER

#include <vector>
#include <cmath>
#include <iostream>
#include <string>

#include "Trade.h"
#include "TreeProduct.h"
#include "Market.h"
#include "Bond.h"
#include "Swap.h"

class Pricer {
public:
	virtual ~Pricer() {}
	virtual double Price(const Market& mkt, Trade* trade);

protected:
	virtual double PriceTree(const Market& mkt, const TreeProduct& trade) { return 0; };
	virtual double PriceBond(const Market& mkt, Bond* bond); // Added for bond DCF pricing
};

class BinomialTreePricer : public Pricer
{
public:
	BinomialTreePricer(int N) :
		nTimeSteps(N),
		states(N + 1),
		u(0.0), d(0.0), p(0.0), currentSpot(0.0),
		fixedRiskFreeRate_(-1.0), Df_(0.0), dT_(0.0) // Initialize all members
	{
	}

	double PriceTree(const Market& mkt, const TreeProduct& trade) override;
	virtual void setFixedRiskFreeRate(double rate) { fixedRiskFreeRate_ = rate; }

protected:
	virtual void ModelSetup(double S0, double sigma, double rate, double dt);

	virtual double GetSpot(int ti, int si) const { return currentSpot * std::pow(u, ti - si) * std::pow(d, si); };
	virtual inline double GetProbUp() const { return p; };
	virtual double GetProbDown() const { return 1 - p; };

	int nTimeSteps;
	std::vector<double> states;
	double u;
	double d;
	double p;
	double currentSpot;
	double fixedRiskFreeRate_;
	double Df_;
	double dT_;
};

class CRRBinomialTreePricer : public BinomialTreePricer
{
public:
	CRRBinomialTreePricer(int N) : BinomialTreePricer(N) {}

	double PriceTree(const Market& mkt, const TreeProduct& trade) override;

protected:
	void ModelSetup(double S0, double sigma, double rate, double dt) override;
	void ModelSetup(const Market& mkt, const TreeProduct& product);

	double GetSpot(int ti, int si) const override {
		return currentSpot * std::pow(u, ti - 2 * si);
	}
};

class JRRNBinomialTreePricer : public BinomialTreePricer
{
public:
	JRRNBinomialTreePricer(int N) : BinomialTreePricer(N) {}

protected:
	void ModelSetup(double S0, double sigma, double rate, double dt) override;
};

#endif