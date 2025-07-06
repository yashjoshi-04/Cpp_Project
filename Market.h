#ifndef MARKET_H
#define MARKET_H

#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory> // Required for std::shared_ptr
#include <algorithm> // Required for std::sort
#include <fstream>   // Required for file operations in load methods
#include <sstream>   // Required for parsing lines

#include "Date.h"

// Forward declaration for imp::linearInterpolate if it's in a separate utility
// Based on prompt, it was in a namespace `imp`.
namespace imp {
    double linearInterpolate(double x0, double y0, double x1, double y1, double x);
}


class RateCurve {
public:
    RateCurve() = default;
    RateCurve(const std::string& crvName) : name(crvName) {}
    
    void addRate(const Date& tenor, double rate); // tenor const&
    double getRate(const Date& tenor) const;   // tenor const&
    void shock(double shockValue); // Parallel shock all rates
    
    void display() const;
    bool isEmpty() const { return tenorDates.empty(); }
    const std::string& getName() const { return name; }
    const std::vector<Date>& getTenorDates() const { return tenorDates; }
    const std::vector<double>& getRates() const { return rates; }

private:
    std::string name;
    std::vector<Date> tenorDates; // Should be kept sorted by Date for efficient interpolation
    std::vector<double> rates;
};

class VolCurve {
public:
    VolCurve() = default;
    VolCurve(const std::string& crvName) : name(crvName) {}

    void addVol(const Date& tenor, double vol); // tenor const&
    double getVol(const Date& tenor) const;   // tenor const&
    void shock(double shockValue); // Parallel shock all vols

    void display() const;
    bool isEmpty() const { return tenors.empty(); }
    const std::string& getName() const { return name; }
    const std::vector<Date>& getTenors() const { return tenors; }
    const std::vector<double>& getVols() const { return vols; }

private:
    std::string name;
    std::vector<Date> tenors; // Should be kept sorted by Date
    std::vector<double> vols;
};

class Market {
public:
    Date asOf;
    std::string name; // Changed from char*

    Market();
    Market(const Date& now, const std::string& marketName = "defaultMarket");
    
    Market(const Market& other); 
    Market& operator=(const Market& other); 
    ~Market() = default;

    void Print() const;
    
    void addCurve(const std::string& curveName, std::shared_ptr<RateCurve> curve);
    void addVolCurve(const std::string& volCurveName, std::shared_ptr<VolCurve> volCurve);
    
    void addBondPrice(const std::string& bondName, double price);
    void addStockPrice(const std::string& stockName, double price);
    
    double getStockPrice(const std::string& stockName) const;

    std::shared_ptr<const RateCurve> getCurve(const std::string& curveName) const;
    std::shared_ptr<RateCurve> getCurve(const std::string& curveName);
    
    std::shared_ptr<const VolCurve> getVolCurve(const std::string& volCurveName) const;
    std::shared_ptr<VolCurve> getVolCurve(const std::string& volCurveName);

    // Methods to load data from files - these will modify the market object
    bool loadCurveDataFromFile(const std::string& filePath, const std::string& curveNameInFile, const std::string& marketCurveName);
    bool loadVolDataFromFile(const std::string& filePath, const std::string& volCurveNameInFile, const std::string& marketVolCurveName);
    bool loadStockPricesFromFile(const std::string& filePath);
    bool loadBondPricesFromFile(const std::string& filePath);

private:
    std::unordered_map<std::string, std::shared_ptr<RateCurve>> curvesMap;
    std::unordered_map<std::string, std::shared_ptr<VolCurve>> volsMap;
    std::unordered_map<std::string, double> bondPricesMap;
    std::unordered_map<std::string, double> stockPricesMap;

    // Helper for parsing tenor strings if not using Date::dateAddTenor directly for this
    static Date parseTenorStrToDate(const Date& baseDate, const std::string& tenorStr); // Keep if used by loading
    static double parseRateValue(const std::string& rateStr); // Keep if used by loading

};

std::ostream& operator<<(std::ostream& os, const Market& market);

#endif // MARKET_H
//Updated
