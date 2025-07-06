#include "Market.h"
#include "Date.h" // Ensure Date is fully defined for dateAddTenor if used by helpers
#include <algorithm> // For std::sort, std::transform
#include <fstream>   // For std::ifstream
#include <sstream>   // For std::istringstream
#include <stdexcept> // For std::runtime_error, std::out_of_range
#include <iomanip>   // For std::fixed, std::setprecision in display methods

// --- Namespace for Interpolation Utility (from prompt) ---
namespace imp {
    double linearInterpolate(double x0, double y0, double x1, double y1, double x) {
        if (x1 == x0) { // Avoid division by zero; return one of the y values
            return y0; 
        }
        if (x <= x0) { 
            return y0;
        }
        if (x >= x1) { 
            return y1;
        }
        return y0 + (x - x0) * (y1 - y0) / (x1 - x0);
    }
} // namespace imp

// --- Anonymous namespace for local helper functions ---
namespace {
    std::string toLowerCpp(const std::string& str) {
        std::string data = str;
        std::transform(data.begin(), data.end(), data.begin(),
            [](unsigned char c){ return std::tolower(c); });
        return data;
    }
} // anonymous namespace

// --- RateCurve Method Implementations ---
void RateCurve::addRate(const Date& tenor, double rate) {
    auto it = std::lower_bound(tenorDates.begin(), tenorDates.end(), tenor);
    if (it != tenorDates.end() && *it == tenor) {
        rates[std::distance(tenorDates.begin(), it)] = rate;
    } else {
        size_t insert_pos = std::distance(tenorDates.begin(), it);
        tenorDates.insert(it, tenor);
        rates.insert(rates.begin() + insert_pos, rate);
    }
}

double RateCurve::getRate(const Date& tenor) const {
    if (tenorDates.empty()) {
        std::cerr << "Warning: RateCurve '" << name << "' is empty. Returning 0.0 for getRate." << std::endl;
        return 0.0; 
    }
    if (tenor <= tenorDates.front()) { 
        return rates.front();
    }
    if (tenor >= tenorDates.back()) { 
        return rates.back();
    }

    auto it = std::lower_bound(tenorDates.begin(), tenorDates.end(), tenor);
    if (it != tenorDates.end() && *it == tenor) {
        return rates[std::distance(tenorDates.begin(), it)];
    }
    if (it == tenorDates.begin()) { 
        return rates.front();
    }

    const Date& x1_date = *it;
    const Date& x0_date = *(it - 1);
    double y1 = rates[std::distance(tenorDates.begin(), it)];
    double y0 = rates[std::distance(tenorDates.begin(), it - 1)];

    double x0_serial = x0_date.getSerialDate();
    double x1_serial = x1_date.getSerialDate();
    double x_serial = tenor.getSerialDate();

    return imp::linearInterpolate(x0_serial, y0, x1_serial, y1, x_serial);
}

void RateCurve::shock(double shockValue) {
    for (double& rate : rates) {
        rate += shockValue;
    }
}

void RateCurve::display() const {
    std::cout << "Rate Curve: " << name << std::endl;
    for (size_t i = 0; i < tenorDates.size(); ++i) {
        std::cout << "  " << tenorDates[i].toString() << ": " << std::fixed << std::setprecision(5) << rates[i] << std::endl;
    }
}

// --- VolCurve Method Implementations ---
void VolCurve::addVol(const Date& tenor, double vol) {
    auto it = std::lower_bound(tenors.begin(), tenors.end(), tenor);
    if (it != tenors.end() && *it == tenor) {
        vols[std::distance(tenors.begin(), it)] = vol;
    } else {
        size_t insert_pos = std::distance(tenors.begin(), it);
        tenors.insert(it, tenor);
        vols.insert(vols.begin() + insert_pos, vol);
    }
}

double VolCurve::getVol(const Date& tenor) const {
    if (tenors.empty()) {
        std::cerr << "Warning: VolCurve '" << name << "' is empty. Returning 0.0 for getVol." << std::endl;
        return 0.0; 
    }
    if (tenor <= tenors.front()) {
        return vols.front();
    }
    if (tenor >= tenors.back()) {
        return vols.back();
    }

    auto it = std::lower_bound(tenors.begin(), tenors.end(), tenor);
    if (it != tenors.end() && *it == tenor) {
        return vols[std::distance(tenors.begin(), it)];
    }
    if (it == tenors.begin()) {
        return vols.front();
    }

    const Date& x1_date = *it;
    const Date& x0_date = *(it - 1);
    double y1 = vols[std::distance(tenors.begin(), it)];
    double y0 = vols[std::distance(tenors.begin(), it - 1)];

    double x0_serial = x0_date.getSerialDate();
    double x1_serial = x1_date.getSerialDate();
    double x_serial = tenor.getSerialDate();

    return imp::linearInterpolate(x0_serial, y0, x1_serial, y1, x_serial);
}

void VolCurve::shock(double shockValue) {
    for (double& vol_val : vols) { // Renamed to avoid conflict with member name
        vol_val += shockValue;
    }
}

void VolCurve::display() const {
    std::cout << "Volatility Curve: " << name << std::endl;
    for (size_t i = 0; i < tenors.size(); ++i) {
        std::cout << "  " << tenors[i].toString() << ": " << std::fixed << std::setprecision(5) << vols[i] << std::endl;
    }
}

// --- Market Method Implementations ---
Market::Market() : asOf(Date()), name("defaultMarket") {}

Market::Market(const Date& now, const std::string& marketName) : asOf(now), name(marketName) {}

Market::Market(const Market& other) : asOf(other.asOf), name(other.name) {
    curvesMap.clear();
    for (const auto& pair : other.curvesMap) {
        if (pair.second) { 
            curvesMap[pair.first] = std::make_shared<RateCurve>(*pair.second);
        }
    }
    volsMap.clear();
    for (const auto& pair : other.volsMap) {
        if (pair.second) { 
            volsMap[pair.first] = std::make_shared<VolCurve>(*pair.second);
        }
    }
    bondPricesMap = other.bondPricesMap;
    stockPricesMap = other.stockPricesMap;
}

Market& Market::operator=(const Market& other) {
    if (this == &other) {
        return *this;
    }
    asOf = other.asOf;
    name = other.name;

    curvesMap.clear();
    for (const auto& pair : other.curvesMap) {
        if (pair.second) {
            curvesMap[pair.first] = std::make_shared<RateCurve>(*pair.second);
        }
    }
    volsMap.clear();
    for (const auto& pair : other.volsMap) {
        if (pair.second) {
            volsMap[pair.first] = std::make_shared<VolCurve>(*pair.second);
        }
    }
    bondPricesMap = other.bondPricesMap;
    stockPricesMap = other.stockPricesMap;
    return *this;
}

void Market::Print() const {
    std::cout << "Market Name: " << name << std::endl;
    std::cout << "As Of Date: " << asOf.toString() << std::endl;
    for (const auto& pair : curvesMap) {
        if (pair.second) pair.second->display();
    }
    for (const auto& pair : volsMap) {
        if (pair.second) pair.second->display();
    }
    std::cout << "Stock Prices:" << std::endl;
    for (const auto& pair : stockPricesMap) {
        std::cout << "  " << pair.first << ": " << pair.second << std::endl;
    }
    std::cout << "Bond Prices:" << std::endl;
    for (const auto& pair : bondPricesMap) {
        std::cout << "  " << pair.first << ": " << pair.second << std::endl;
    }
}

void Market::addCurve(const std::string& curveName, std::shared_ptr<RateCurve> curve) {
    curvesMap[curveName] = curve;
}

void Market::addVolCurve(const std::string& volCurveName, std::shared_ptr<VolCurve> volCurve) {
    volsMap[volCurveName] = volCurve;
}

void Market::addBondPrice(const std::string& bondName, double price) {
    bondPricesMap[bondName] = price;
}

void Market::addStockPrice(const std::string& stockName, double price) {
    stockPricesMap[stockName] = price;
}

double Market::getStockPrice(const std::string& stockName) const {
    auto it = stockPricesMap.find(stockName);
    if (it != stockPricesMap.end()) {
        return it->second;
    }
    std::cerr << "Warning: Stock price for '" << stockName << "' not found in Market. Returning 0.0." << std::endl;
    return 0.0; 
}

std::shared_ptr<const RateCurve> Market::getCurve(const std::string& curveName) const {
    auto it = curvesMap.find(curveName);
    if (it != curvesMap.end()) {
        return it->second;
    }
    std::cerr << "Warning: Rate curve '" << curveName << "' not found in Market. Returning nullptr." << std::endl;
    return nullptr;
}

std::shared_ptr<RateCurve> Market::getCurve(const std::string& curveName) {
    auto it = curvesMap.find(curveName);
    if (it != curvesMap.end()) {
        return it->second;
    }
    std::cerr << "Warning: Rate curve '" << curveName << "' not found in Market. Returning nullptr." << std::endl;
    return nullptr;
}

std::shared_ptr<const VolCurve> Market::getVolCurve(const std::string& volCurveName) const {
    auto it = volsMap.find(volCurveName);
    if (it != volsMap.end()) {
        return it->second;
    }
    std::cerr << "Warning: Vol curve '" << volCurveName << "' not found in Market. Returning nullptr." << std::endl;
    return nullptr;
}

std::shared_ptr<VolCurve> Market::getVolCurve(const std::string& volCurveName) {
    auto it = volsMap.find(volCurveName);
    if (it != volsMap.end()) {
        return it->second;
    }
    std::cerr << "Warning: Vol curve '" << volCurveName << "' not found in Market. Returning nullptr." << std::endl;
    return nullptr;
}

// --- Market Data Loading Method Implementations ---

Date Market::parseTenorStrToDate(const Date& baseDate, const std::string& tenorStr) {
    try {
        // Assuming dateAddTenor is robust and available (defined via Date.h/cpp)
        return dateAddTenor(baseDate, tenorStr); 
    } catch (const std::exception& e) {
        std::cerr << "Warning: Could not parse tenor string '" << tenorStr << "' in Market loader: " << e.what() << std::endl;
        return baseDate; // Or throw an exception further up, or return a specific 'invalid' Date object
    }
}

double Market::parseRateValue(const std::string& rateStr) {
    std::string cleanedStr = rateStr;
    size_t percentPos = cleanedStr.find('%');
    double multiplier = 1.0;
    if (percentPos != std::string::npos) {
        cleanedStr.erase(percentPos, 1);
        multiplier = 0.01;
    }
    try {
        return std::stod(cleanedStr) * multiplier;
    } catch (const std::exception& e) {
        std::cerr << "Warning: Could not parse rate value '" << rateStr << "': " << e.what() << std::endl;
        return 0.0; 
    }
}

bool Market::loadCurveDataFromFile(const std::string& filePath, const std::string& curveNameInFileIgnored, const std::string& marketCurveName) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open curve data file: " << filePath << std::endl;
        return false;
    }
    auto curve = std::make_shared<RateCurve>(marketCurveName);
    std::string line;
    bool headerSkipped = false; 

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#' || line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
        if (!headerSkipped && (toLowerCpp(line).find("tenor") != std::string::npos || toLowerCpp(line).find("date") != std::string::npos) ){
             headerSkipped = true; 
             continue;
        }

        std::istringstream iss(line);
        std::string tenorPart, ratePart;
        // Example parsing for lines like "1M : 0.05" or "2024-12-31 : 0.025"
        if (std::getline(iss, tenorPart, ':') && std::getline(iss, ratePart)) {
            // Trim whitespace
            tenorPart.erase(0, tenorPart.find_first_not_of(" \t"));
            tenorPart.erase(tenorPart.find_last_not_of(" \t") + 1);
            ratePart.erase(0, ratePart.find_first_not_of(" \t"));
            ratePart.erase(ratePart.find_last_not_of(" \t") + 1);
        } else {
            std::cerr << "Warning: Malformed line in curve file '" << filePath << "': " << line << std::endl;
            continue;
        }
        
        Date tenorDate;
        if (tenorPart.find('-') != std::string::npos && tenorPart.length() >= 8) { 
            try {
                tenorDate = Date(tenorPart);
            } catch (const std::exception& e) {
                std::cerr << "Warning: Invalid date format '" << tenorPart << "' in curve file. " << e.what() << std::endl;
                continue;
            }
        } else { 
             tenorDate = parseTenorStrToDate(this->asOf, tenorPart); 
        }
        double rate = parseRateValue(ratePart);
        curve->addRate(tenorDate, rate);
    }
    if (!curve->isEmpty()) {
        addCurve(marketCurveName, curve);
        return true;
    }
    return false;
}

bool Market::loadVolDataFromFile(const std::string& filePath, const std::string& volCurveNameInFileIgnored, const std::string& marketVolCurveName) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open vol data file: " << filePath << std::endl;
        return false;
    }
    auto volCurve = std::make_shared<VolCurve>(marketVolCurveName);
    std::string line;
    bool headerSkipped = false;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#' || line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
        if (!headerSkipped && (toLowerCpp(line).find("tenor") != std::string::npos || toLowerCpp(line).find("expiry") != std::string::npos) ){
             headerSkipped = true;
             continue;
        }
        std::istringstream iss(line);
        std::string tenorPart, volPart;
        if (std::getline(iss, tenorPart, ':') && std::getline(iss, volPart)) {
            tenorPart.erase(0, tenorPart.find_first_not_of(" \t"));
            tenorPart.erase(tenorPart.find_last_not_of(" \t") + 1);
            volPart.erase(0, volPart.find_first_not_of(" \t"));
            volPart.erase(volPart.find_last_not_of(" \t") + 1);
        } else {
            std::cerr << "Warning: Malformed line in vol file '" << filePath << "': " << line << std::endl;
            continue;
        }

        Date tenorDate;
        if (tenorPart.find('-') != std::string::npos && tenorPart.length() >= 8) {
            try {
                tenorDate = Date(tenorPart);
            } catch (const std::exception& e) {
                std::cerr << "Warning: Invalid date format '" << tenorPart << "' in vol file. " << e.what() << std::endl;
                continue;
            }
        } else {
             tenorDate = parseTenorStrToDate(this->asOf, tenorPart);
        }
        double vol = parseRateValue(volPart); 
        volCurve->addVol(tenorDate, vol);
    }
    if (!volCurve->isEmpty()) {
        addVolCurve(marketVolCurveName, volCurve);
        return true;
    }
    return false;
}

bool Market::loadStockPricesFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open stock prices file: " << filePath << std::endl;
        return false;
    }
    std::string line;
    std::string stockName;
    double stockPrice;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#' || line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
        std::istringstream iss(line);
        if (iss >> stockName >> stockPrice) {
            addStockPrice(stockName, stockPrice);
        } else {
            std::cerr << "Warning: Malformed line in stock prices file '" << filePath << "': " << line << std::endl;
        }
    }
    return true; 
}

bool Market::loadBondPricesFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open bond prices file: " << filePath << std::endl;
        return false;
    }
    std::string line;
    std::string bondName;
    double bondPrice;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#' || line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
        std::istringstream iss(line);
        if (iss >> bondName >> bondPrice) {
            addBondPrice(bondName, bondPrice);
        } else {
            std::cerr << "Warning: Malformed line in bond prices file '" << filePath << "': " << line << std::endl;
        }
    }
    return true;
}

std::ostream& operator<<(std::ostream& os, const Market& market) {
    os << "Market: " << market.name << " AsOf: " << market.asOf.toString();
    return os;
}
//Updated
