#include "Market.h"
#include "Date.h" 
#include <algorithm> 
#include <fstream>   
#include <sstream>   
#include <stdexcept> 
#include <iomanip>   

namespace imp {
    double linearInterpolate(double x0, double y0, double x1, double y1, double x) {
        if (x1 == x0) { 
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
} 

namespace {
    std::string toLowerCppLocalMarket(const std::string& str) { // Renamed to be unique
        std::string data = str;
        std::transform(data.begin(), data.end(), data.begin(),
            [](unsigned char c){ return std::tolower(c); });
        return data;
    }
} 

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
    // Corrected comparisons:
    if (tenor <= tenorDates.front()) { 
        return rates.front();
    }
    if (!(tenor < tenorDates.back())) { // tenor >= tenorDates.back()
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
    // Corrected comparisons:
    if (tenor <= tenors.front()) {
        return vols.front();
    }
    if (!(tenor < tenors.back())) { // tenor >= tenors.back()
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
    for (double& vol_val : vols) { 
        vol_val += shockValue;
    }
}

void VolCurve::display() const {
    std::cout << "Volatility Curve: " << name << std::endl;
    for (size_t i = 0; i < tenors.size(); ++i) {
        std::cout << "  " << tenors[i].toString() << ": " << std::fixed << std::setprecision(5) << vols[i] << std::endl;
    }
}

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
    // Removed cerr for const getter, as it might be called to check existence.
    // Let caller handle nullptr.
    return nullptr;
}
std::shared_ptr<RateCurve> Market::getCurve(const std::string& curveName) {
    auto it = curvesMap.find(curveName);
    if (it != curvesMap.end()) {
        return it->second;
    }
    std::cerr << "Warning: Rate curve '" << curveName << "' not found in Market (non-const get). Returning nullptr." << std::endl;
    return nullptr;
}
std::shared_ptr<const VolCurve> Market::getVolCurve(const std::string& volCurveName) const {
    auto it = volsMap.find(volCurveName);
    if (it != volsMap.end()) {
        return it->second;
    }
    return nullptr;
}
std::shared_ptr<VolCurve> Market::getVolCurve(const std::string& volCurveName) {
    auto it = volsMap.find(volCurveName);
    if (it != volsMap.end()) {
        return it->second;
    }
    std::cerr << "Warning: Vol curve '" << volCurveName << "' not found in Market (non-const get). Returning nullptr." << std::endl;
    return nullptr;
}

Date Market::parseTenorStrToDate(const Date& baseDate, const std::string& tenorStr) {
    try {
        return dateAddTenor(baseDate, tenorStr); 
    } catch (const std::exception& e) {
        std::cerr << "Warning: Could not parse tenor string '" << tenorStr << "' in Market helper: " << e.what() << std::endl;
        return baseDate; 
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
        // Trim potential whitespace again before stod
        cleanedStr.erase(0, cleanedStr.find_first_not_of(" \t\r\n"));
        cleanedStr.erase(cleanedStr.find_last_not_of(" \t\r\n") + 1);
        return std::stod(cleanedStr) * multiplier;
    } catch (const std::exception& e) {
        std::cerr << "Warning: Could not parse rate value '" << rateStr << "' (cleaned: '" << cleanedStr << "'): " << e.what() << std::endl;
        return 0.0; 
    }
}

bool Market::loadCurveDataFromFile(const std::string& filePath, 
                                 const std::string& curveNameInFileHint, 
                                 const std::string& marketCurveNameToStore) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open curve data file: " << filePath << std::endl;
        return false;
    }
    
    std::string actualCurveName = marketCurveNameToStore;
    std::string line;
    bool firstLineRead = false;
    bool firstLineIsData = false;

    std::streampos originalPos = file.tellg();
    if (std::getline(file, line)) {
        std::istringstream iss_first(line);
        std::string part1, part2;
        iss_first >> part1; // Read first token
        if (iss_first >> part2 || part1.find(':') != std::string::npos || part1.find('%') != std::string::npos) {
            // If there's a second token, or the first token contains ':' or '%', it's likely data or a header with data format
            firstLineIsData = true;
        } else {
            // Likely a curve name if it's a single token-like string without typical data chars
            actualCurveName = part1;
            // std::cout << "Info: Deduced curve name from file '" << filePath << "' as '" << actualCurveName << "'." << std::endl;
        }
        file.clear(); 
        file.seekg(originalPos); // Rewind to process all lines or the first line again if it was data
    }
    if (actualCurveName.empty()) actualCurveName = curveNameInFileHint; 
    if (actualCurveName.empty()) {
        std::cerr << "Error: No curve name could be determined for data in file: " << filePath << std::endl;
        return false;
    }

    auto curve = std::make_shared<RateCurve>(actualCurveName);
    bool headerSkipped = false; 

    // If the first line was identified as the curve name, skip it in the data reading loop.
    if (!firstLineIsData && firstLineRead) {
        std::getline(file, line); // Consume the name line already processed
    }

    while (std::getline(file, line)) {
        if (!headerSkipped && (toLowerCppLocalMarket(line).find("tenor") != std::string::npos || toLowerCppLocalMarket(line).find("rate") != std::string::npos)){
             headerSkipped = true; 
             continue;
        }
        if (line.empty() || line[0] == '#' || line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
        
        std::istringstream iss(line);
        std::string tenorPart, ratePart_str;
        if (std::getline(iss, tenorPart, ':') && std::getline(iss, ratePart_str)) {
            tenorPart.erase(0, tenorPart.find_first_not_of(" \t"));
            tenorPart.erase(tenorPart.find_last_not_of(" \t") + 1);
            ratePart_str.erase(0, ratePart_str.find_first_not_of(" \t"));
            ratePart_str.erase(ratePart_str.find_last_not_of(" \t") + 1);
        } else {
            std::cerr << "Warning: Malformed line in curve file '" << filePath << "' for curve '" << actualCurveName << "': " << line << std::endl;
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
        double rate = parseRateValue(ratePart_str);
        curve->addRate(tenorDate, rate);
    }
    if (!curve->isEmpty()) {
        addCurve(actualCurveName, curve); 
        return true;
    }
    return false;
}

bool Market::loadVolDataFromFile(const std::string& filePath, [[maybe_unused]] const std::string& volCurveNameInFileHint, const std::string& marketVolCurveNameToStore) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open vol data file: " << filePath << std::endl;
        return false;
    }
    std::string actualVolCurveName = marketVolCurveNameToStore;
    std::string line;
    bool firstLineRead = false;
    bool firstLineIsData = false;

    std::streampos originalPos = file.tellg();
    if (std::getline(file, line)) {
        std::istringstream iss_first(line);
        std::string part1, part2;
        iss_first >> part1; 
        if (iss_first >> part2 || part1.find(':') != std::string::npos || part1.find('%') != std::string::npos) {
            firstLineIsData = true;
        } else {
            actualVolCurveName = part1;
        }
        file.clear(); 
        file.seekg(originalPos); 
    }
    if (actualVolCurveName.empty() && volCurveNameInFileHint.empty()) {
         std::cerr << "Error: No vol curve name for data in: " << filePath << std::endl; return false;
    } else if (actualVolCurveName.empty()) {
        actualVolCurveName = volCurveNameInFileHint;
    }
    
    auto volCurve = std::make_shared<VolCurve>(actualVolCurveName);
    bool headerSkipped = false;
    if (!firstLineIsData && firstLineRead) {
        std::getline(file, line); // Consume the name line
    }

    while (std::getline(file, line)) {
        if (!headerSkipped && (toLowerCppLocalMarket(line).find("tenor") != std::string::npos || toLowerCppLocalMarket(line).find("expiry") != std::string::npos || toLowerCppLocalMarket(line).find("volatility") != std::string::npos) ){
             headerSkipped = true;
             continue;
        }
        if (line.empty() || line[0] == '#' || line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
        std::istringstream iss(line);
        std::string tenorPart, volPart_str;
        if (std::getline(iss, tenorPart, ':') && std::getline(iss, volPart_str)) {
            tenorPart.erase(0, tenorPart.find_first_not_of(" \t"));
            tenorPart.erase(tenorPart.find_last_not_of(" \t") + 1);
            volPart_str.erase(0, volPart_str.find_first_not_of(" \t"));
            volPart_str.erase(volPart_str.find_last_not_of(" \t") + 1);
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
        double vol = parseRateValue(volPart_str); 
        volCurve->addVol(tenorDate, vol);
    }
    if (!volCurve->isEmpty()) {
        addVolCurve(actualVolCurveName, volCurve);
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
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#' || line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
        std::istringstream iss(line);
        std::string namePart, pricePart;
        if (std::getline(iss, namePart, ':') && std::getline(iss, pricePart)) {
            namePart.erase(0, namePart.find_first_not_of(" \t"));
            namePart.erase(namePart.find_last_not_of(" \t") + 1);
            pricePart.erase(0, pricePart.find_first_not_of(" \t"));
            pricePart.erase(pricePart.find_last_not_of(" \t") + 1);
            try {
                double stockPrice = std::stod(pricePart);
                addStockPrice(namePart, stockPrice);
            } catch (const std::exception& e) {
                std::cerr << "Warning: Could not parse stock price for '" << namePart << "' from value '" << pricePart << "'. " << e.what() << std::endl;
            }
        } else { // Fallback for space separated if colon not found
            std::istringstream iss_space(line); // re-init with original line
            std::string stockName_space;
            double stockPrice_space;
            if (iss_space >> stockName_space >> stockPrice_space) {
                 addStockPrice(stockName_space, stockPrice_space);
            } else {
                 std::cerr << "Warning: Malformed line in stock prices file '" << filePath << "': " << line << std::endl;
            }
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
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#' || line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
        std::istringstream iss(line);
        std::string namePart, pricePart;
        if (std::getline(iss, namePart, ':') && std::getline(iss, pricePart)) {
            namePart.erase(0, namePart.find_first_not_of(" \t"));
            namePart.erase(namePart.find_last_not_of(" \t") + 1);
            pricePart.erase(0, pricePart.find_first_not_of(" \t"));
            pricePart.erase(pricePart.find_last_not_of(" \t") + 1);
            try {
                double bondPrice = std::stod(pricePart);
                addBondPrice(namePart, bondPrice);
            } catch (const std::exception& e) {
                std::cerr << "Warning: Could not parse bond price for '" << namePart << "' from value '" << pricePart << "'. " << e.what() << std::endl;
            }
        } else { // Fallback for space separated
            std::istringstream iss_space(line);
            std::string bondName_space;
            double bondPrice_space;
            if (iss_space >> bondName_space >> bondPrice_space) {
                addBondPrice(bondName_space, bondPrice_space);
            } else {
                std::cerr << "Warning: Malformed line in bond prices file '" << filePath << "': " << line << std::endl;
            }
        }
    }
    return true;
}

std::ostream& operator<<(std::ostream& os, const Market& market) {
    os << "Market: " << market.name << " AsOf: " << market.asOf.toString();
    return os;
}
