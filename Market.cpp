#include "Market.h"

using namespace std;

void RateCurve::display() const {
    cout << "rate curve:" << name << endl;
    for (size_t i=0; i<tenorDates.size(); i++) {
      cout << tenorDates[i] << ":" << rates[i] << endl;
  }
  cout << endl;
}

void RateCurve::addRate(Date tenor, double rate) {
    tenorDates.push_back(tenor);
    rates.push_back(rate);
    // Sort to maintain order for interpolation (optional, but good practice if data isn't guaranteed to be added in order)
    // For simplicity, assuming data is added in chronological order.
}

double RateCurve::getRate(Date tenor) const {
    if (tenorDates.empty()) {
        return 0.0; // Or throw an exception
    }
    // If tenor is before the first date, use the first rate
    if (tenor - tenorDates.front() <= 0) {
        return rates.front();
    }
    // If tenor is after the last date, use the last rate
    if (tenor - tenorDates.back() >= 0) {
        return rates.back();
    }

    // Find the interval for linear interpolation
    for (size_t i = 0; i < tenorDates.size() - 1; ++i) {
        if ((tenor - tenorDates[i]) >= 0 && (tenor - tenorDates[i+1]) <= 0) {
            double t1 = tenor - tenorDates[i];
            double t2 = tenorDates[i+1] - tenor;
            double totalDiff = tenorDates[i+1] - tenorDates[i];
            if (totalDiff == 0) return rates[i]; // Avoid division by zero
            return (rates[i] * t2 + rates[i+1] * t1) / totalDiff;
        }
    }
    return 0.0; // Should not reach here if logic is correct
}

void VolCurve::display() const {
    cout << "vol curve:" << name << endl;
    for (size_t i = 0; i < tenors.size(); i++) {
        cout << tenors[i] << ":" << vols[i] << endl;
    }
    cout << endl;
}

void VolCurve::addVol(Date tenor, double vol) {
    tenors.push_back(tenor);
    vols.push_back(vol);
}

double VolCurve::getVol(Date tenor) const {
    if (tenors.empty()) {
        return 0.0; // Or throw an exception
    }
    // If tenor is before the first date, use the first vol
    if (tenor - tenors.front() <= 0) {
        return vols.front();
    }
    // If tenor is after the last date, use the last vol
    if (tenor - tenors.back() >= 0) {
        return vols.back();
    }

    // Find the interval for linear interpolation
    for (size_t i = 0; i < tenors.size() - 1; ++i) {
        if ((tenor - tenors[i]) >= 0 && (tenor - tenors[i+1]) <= 0) {
            double t1 = tenor - tenors[i];
            double t2 = tenors[i+1] - tenor;
            double totalDiff = tenors[i+1] - tenors[i];
            if (totalDiff == 0) return vols[i]; // Avoid division by zero
            return (vols[i] * t2 + vols[i+1] * t1) / totalDiff;
        }
    }
    return 0.0; // Should not reach here if logic is correct
}

void Market::Print() const
{
  cout << "market asof: " << asOf << endl;
  
  for (auto const& [name, curve] : curves) {
    curve.display();
  }
  for (auto const& [name, vol] : vols) {
    vol.display();
  }
  cout << "Bond Prices:" << endl;
  for (auto const& [name, price] : bondPrices) {
      cout << name << ": " << price << endl;
  }
  cout << endl;
  cout << "Stock Prices:" << endl;
  for (auto const& [name, price] : stockPrices) {
      cout << name << ": " << price << endl;
  }
  cout << endl;
}

void Market::addCurve(const std::string& curveName, const RateCurve& curve){
  curves.emplace(curveName, curve);
}

void Market::addVolCurve(const std::string& curveName, const VolCurve& curve) {
    vols.emplace(curveName, curve);
}

void Market::addBondPrice(const std::string& bondName, double price) {
    bondPrices.emplace(bondName, price);
}

void Market::addStockPrice(const std::string& stockName, double price) {
    stockPrices.emplace(stockName, price);
}

// double Market::getStockPrice(const std::string& stockName) const { // Definition moved to Market.h
//     return stockPrices.at(stockName);
// }

// void Market::PrintStockKeysForDebug() const { // DEBUG REMOVED
//    for (auto const& [key, val] : stockPrices) {
//        std::cout << "  - Key: '" << key << "'" << std::endl;
//    }
// }

std::ostream& operator<<(std::ostream& os, const Market& mkt)
{
  os << mkt.asOf << std::endl;
  return os;
}

std::istream& operator>>(std::istream& is, Market& mkt)
{
  is >> mkt.asOf;
  return is;
}
