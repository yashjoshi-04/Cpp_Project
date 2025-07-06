#include <iostream>
#include <vector>
#include <string>
#include <memory>     // For std::shared_ptr, std::make_unique
#include <fstream>    // For std::ofstream
#include <sstream>    // For std::istringstream (in loadTradesFromFile)
#include <iomanip>    // For std::fixed, std::setprecision
#include <stdexcept>  // For std::exception handling
#include <algorithm>  // For std::transform (in loadTradesFromFile helpers)
#include <chrono>     // For system_clock to get current date
#include <ctime>      // For std::time_t, std::tm, localtime_s/localtime_r

// Project Headers
#include "Date.h"
#include "Market.h"
#include "Trade.h"
#include "Bond.h"
#include "Swap.h"
#include "EuropeanTrade.h"
#include "AmericanTrade.h"
#include "TreeProduct.h"   // Base for options
#include "Pricer.h"
#include "TradeFactory.h"
#include "MarketDecorators.h"
#include "RiskEngine.h"
#include "Types.h"         // For OptionType
#include "MathUtils.h"     // For normalCDF, blackScholesPrice
#include "Utils.h"         // For splitString 

// Helper function to convert string to lower case 
namespace {
    std::string toLowerMain(const std::string& str) {
        std::string data = str;
        std::transform(data.begin(), data.end(), data.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        return data;
    }
} // anonymous namespace

// Function to load trades from file using factories
bool loadTradesFromFile(
    const std::string& filePath,
    std::vector<std::shared_ptr<Trade>>& portfolio,
    const Date& valuationDate, 
    TradeFactory& bondFactory,
    TradeFactory& swapFactory,
    TradeFactory& euroOptFactory,
    TradeFactory& amerOptFactory
) {
    std::ifstream inputFile(filePath);
    if (!inputFile.is_open()) {
        std::cerr << "Error: Could not open trade file: '" << filePath << "'." << std::endl;
        return false;
    }
    std::string line;
    int lineNumber = 0;
    bool headerSkipped = false;

    while (std::getline(inputFile, line)) {
        lineNumber++;
        if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos || line[0] == '#') {
            continue; 
        }
        if (!headerSkipped) {
            headerSkipped = true; 
            continue; 
        }
        std::vector<std::string> tokens;
        splitString(tokens, line, ';'); 
        if (tokens.size() < 12) { 
            std::cerr << "Warning: Skipping line " << lineNumber << " in '" << filePath 
                      << "'. Incorrect number of columns. Expected at least 12, got " << tokens.size() 
                      << ". Line: '" << line << "'" << std::endl;
            continue;
        }
        try {
            std::string type_str = toLowerMain(tokens[1]);
            Date tradeDate_obj(tokens[2]);
            Date startDate_obj(tokens[3]);
            Date endDate_obj(tokens[4]); 
            double notional_val = std::stod(tokens[5]);
            std::string instrument_str = tokens[6];
            double rate_strike_param = std::stod(tokens[7]);
            int frequency_val = std::stoi(tokens[8]); 
            std::string optionType_str = toLowerMain(tokens[9]);
            std::string disc_curve_str = tokens[10];
            std::string vol_curve_str = tokens[11];
            std::string float_fcst_curve_str = (tokens.size() > 12 && !tokens[12].empty()) ? tokens[12] : disc_curve_str; 

            OptionType optType_enum = OptionType::None; 
            if (type_str == "european" || type_str == "american") {
                if (optionType_str == "call") optType_enum = OptionType::Call;
                else if (optionType_str == "put") optType_enum = OptionType::Put;
                else if (optionType_str == "binarycall") optType_enum = OptionType::BinaryCall;
                else if (optionType_str == "binaryput") optType_enum = OptionType::BinaryPut;
                else if (optionType_str != "none" && !optionType_str.empty()) {
                     throw std::invalid_argument("Unknown option type: " + tokens[9]);
                }
            }
            std::shared_ptr<Trade> newTrade = nullptr;
            if (type_str == "bond") {
                newTrade = bondFactory.createTrade(instrument_str, tradeDate_obj, startDate_obj, endDate_obj, 
                                                   notional_val, rate_strike_param, frequency_val, 
                                                   optType_enum, disc_curve_str, vol_curve_str, float_fcst_curve_str);
            } else if (type_str == "swap") {
                newTrade = swapFactory.createTrade(instrument_str, tradeDate_obj, startDate_obj, endDate_obj, 
                                                   notional_val, rate_strike_param, frequency_val, 
                                                   optType_enum, disc_curve_str, vol_curve_str, float_fcst_curve_str);
            } else if (type_str == "european") {
                newTrade = euroOptFactory.createTrade(instrument_str, tradeDate_obj, startDate_obj, endDate_obj, 
                                                      notional_val, rate_strike_param, frequency_val, 
                                                      optType_enum, disc_curve_str, vol_curve_str, float_fcst_curve_str);
            } else if (type_str == "american") {
                newTrade = amerOptFactory.createTrade(instrument_str, tradeDate_obj, startDate_obj, endDate_obj, 
                                                      notional_val, rate_strike_param, frequency_val, 
                                                      optType_enum, disc_curve_str, vol_curve_str, float_fcst_curve_str);
            } else {
                std::cerr << "Warning: Skipping line " << lineNumber << ". Unknown trade type: '" << type_str << "'." << std::endl;
                continue;
            }
            if (newTrade) {
                portfolio.push_back(newTrade);
            }
        } catch (const std::exception& e) {
            std::cerr << "Warning: Skipping line " << lineNumber << " in '" << filePath 
                      << "'. Error parsing trade data: " << e.what() << ". Line: '" << line << "'" << std::endl;
        }
    }
    inputFile.close();
    return !portfolio.empty(); 
}

int main() {
    try {
        Date valueDate;
        auto now_chrono = std::chrono::system_clock::now();
        std::time_t now_time_t = std::chrono::system_clock::to_time_t(now_chrono);
        std::tm localTime;
        #if defined(_WIN32) || defined(_WIN64)
            localtime_s(&localTime, &now_time_t);
        #else
            localtime_r(&now_time_t, &localTime);
        #endif
        valueDate = Date(localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday);
        std::cout << "Valuation Date: " << valueDate.toString() << std::endl;

        Market market(valueDate, "GlobalMarket");
        std::cout << "\nLoading Market Data..." << std::endl;
        if (!market.loadCurveDataFromFile("usd_curve.txt", "USD-SOFR", "USD-SOFR")) { 
             std::cerr << "Failed to load USD-SOFR curve data from usd_curve.txt. Proceeding with caution." << std::endl;
        }
        if (!market.loadCurveDataFromFile("sgd_curve.txt", "SGD-SORA", "SGD-SORA")) {
             std::cerr << "Failed to load SGD-SORA curve data from sgd_curve.txt. Proceeding with caution." << std::endl;
        }
        if (!market.loadVolDataFromFile("vol.txt", "VOL_CURVE_DEFAULT", "VOL_CURVE_DEFAULT")) { 
             std::cerr << "Failed to load default volatility curve data from vol.txt. Proceeding with caution." << std::endl;
        }
        if (!market.loadStockPricesFromFile("stockPrice.txt")) {
             std::cerr << "Failed to load stock price data from stockPrice.txt. Proceeding with caution." << std::endl;
        }
        market.Print();

        BondFactory bondFactory;
        SwapFactory swapFactory;
        EuropeanOptionFactory euroOptFactory;
        AmericanOptionFactory amerOptFactory;

        std::vector<std::shared_ptr<Trade>> portfolio;
        std::cout << "\nLoading Portfolio from trade.txt..." << std::endl;
        if (!loadTradesFromFile("trade.txt", portfolio, valueDate, bondFactory, swapFactory, euroOptFactory, amerOptFactory)) {
            std::cerr << "Could not load any trades from trade.txt or file not found." << std::endl;
        } else {
            std::cout << "Loaded " << portfolio.size() << " trades into the portfolio." << std::endl;
        }

        auto treePricer = std::make_unique<CRRBinomialTreePricer>(50);
        std::ofstream outputFile("results.txt");
        if (!outputFile.is_open()) {
            std::cerr << "Error: Could not open results.txt for writing." << std::endl;
            return 1;
        }
        outputFile << std::fixed << std::setprecision(4);
        std::cout << std::fixed << std::setprecision(4);

        outputFile << "Trade Pricing and Risk Results as of " << valueDate.toString() << std::endl;
        outputFile << "======================================================================" << std::endl;
        outputFile << "TradeID/Underlying;Type;PV;DV01_Curve;DV01_Value;Vega_Curve;Vega_Value" << std::endl;

        std::cout << "\nCalculating PV and Greeks for Portfolio..." << std::endl;
        RiskEngine riskEngine(0.0001, 0.01);

        for (const auto& trade : portfolio) {
            if (!trade) continue;
            std::cout << "Processing Trade: " << trade->getUnderlyingName() << " (" << trade->getType() << ")" << std::endl;
            outputFile << trade->getUnderlyingName() << ";" << trade->getType() << ";";
            double pv = 0.0;
            try {
                pv = treePricer->Price(market, trade);
                std::cout << "  PV: " << pv << std::endl;
                outputFile << pv << ";";
            } catch (const std::exception& e) {
                std::cerr << "  Error pricing trade " << trade->getUnderlyingName() << ": " << e.what() << std::endl;
                outputFile << "Error;" ;
            }
            try {
                auto dv01_map = riskEngine.computeDv01(trade, market, *treePricer);
                if (!dv01_map.empty()) {
                    for (const auto& dv_pair : dv01_map) {
                        std::cout << "  DV01 (" << dv_pair.first << "): " << dv_pair.second << std::endl;
                        outputFile << dv_pair.first << ";" << dv_pair.second << ";";
                    }
                } else {
                    outputFile << "N/A;0.0;"; 
                }
            } catch (const std::exception& e) {
                std::cerr << "  Error calculating DV01 for " << trade->getUnderlyingName() << ": " << e.what() << std::endl;
                outputFile << "Error;Error;";
            }
            try {
                auto vega_map = riskEngine.computeVega(trade, market, *treePricer);
                if (!vega_map.empty()) {
                    for (const auto& v_pair : vega_map) {
                        std::cout << "  Vega (" << v_pair.first << "): " << v_pair.second << std::endl;
                        outputFile << v_pair.first << ";" << v_pair.second;
                    }
                } else {
                    outputFile << "N/A;0.0"; 
                }
            } catch (const std::exception& e) {
                std::cerr << "  Error calculating Vega for " << trade->getUnderlyingName() << ": " << e.what() << std::endl;
                outputFile << "Error;Error";
            }
            outputFile << std::endl;
        }
        std::cout << "\nResults written to results.txt" << std::endl;

        std::cout << "\n--- Comparison Data for Write-up ---" << std::endl;
        outputFile << "\n--- Comparison Data for Write-up ---" << std::endl;
        std::shared_ptr<EuropeanOption> euroCall = nullptr;
        std::shared_ptr<AmericanOption> amerCall = nullptr;
        std::string commonUnderlying = "";
        double commonStrike = 0.0;
        Date commonExpiry;
        for (const auto& trade : portfolio) {
            if (auto ec = std::dynamic_pointer_cast<EuropeanOption>(trade)) {
                if (ec->getOptionType() == OptionType::Call) {
                    euroCall = ec;
                    commonUnderlying = ec->getUnderlyingName();
                    commonStrike = ec->getStrike();
                    commonExpiry = ec->GetExpiry();
                    for (const auto& other_trade : portfolio) {
                        if (auto ac = std::dynamic_pointer_cast<AmericanOption>(other_trade)) {
                            if (ac->getOptionType() == OptionType::Call && 
                                ac->getUnderlyingName() == commonUnderlying && 
                                std::abs(ac->getStrike() - commonStrike) < 1e-6 && 
                                ac->GetExpiry() == commonExpiry) {
                                amerCall = ac;
                                break;
                            }
                        }
                    }
                    if (euroCall) break; 
                }
            }
        }
        if (euroCall) {
            std::cout << "\nEuropean Option vs. Black-Scholes:" << std::endl;
            outputFile << "\nEuropean Option vs. Black-Scholes:" << std::endl;
            double treePriceEuro = treePricer->Price(market, euroCall);
            double S = market.getStockPrice(euroCall->getUnderlyingName());
            double K = euroCall->getStrike();
            Date expiry = euroCall->GetExpiry();
            double T = (expiry - valueDate);
            std::shared_ptr<const RateCurve> rCurve = market.getCurve(euroCall->getRateCurveName());
            std::shared_ptr<const VolCurve> vCurve = market.getVolCurve(euroCall->getVolCurveName());
            if (rCurve && vCurve && T >= 0) {
                double r = rCurve->getRate(expiry);
                double sigma = vCurve->getVol(expiry);
                double bsPrice = blackScholesPrice(euroCall->getOptionType(), S, K, T, r, sigma);
                std::cout << "  Underlying: " << euroCall->getUnderlyingName() << ", K: " << K << ", Expiry: " << expiry.toString() << std::endl;
                std::cout << "  Binomial Tree Price (50 steps): " << treePriceEuro << std::endl;
                std::cout << "  Black-Scholes Price: " << bsPrice << std::endl;
                std::cout << "  Difference (Tree - BS): " << (treePriceEuro - bsPrice) << std::endl;
                outputFile << "  Underlying: " << euroCall->getUnderlyingName() << ", K: " << K << ", Expiry: " << expiry.toString() << std::endl;
                outputFile << "  Binomial Tree Price (50 steps): " << treePriceEuro << std::endl;
                outputFile << "  Black-Scholes Price: " << bsPrice << std::endl;
                outputFile << "  Difference (Tree - BS): " << (treePriceEuro - bsPrice) << std::endl;
            } else {
                std::cout << "  Could not calculate Black-Scholes price for " << euroCall->getUnderlyingName() << " (missing market data or T<=0)." << std::endl;
                outputFile << "  Could not calculate Black-Scholes price for " << euroCall->getUnderlyingName() << " (missing market data or T<=0)." << std::endl;
            }
            if (amerCall) {
                std::cout << "\nAmerican Call vs. European Call (Same Parameters):" << std::endl;
                outputFile << "\nAmerican Call vs. European Call (Same Parameters):" << std::endl;
                double treePriceAmer = treePricer->Price(market, amerCall);
                std::cout << "  Underlying: " << commonUnderlying << ", K: " << commonStrike << ", Expiry: " << commonExpiry.toString() << std::endl;
                std::cout << "  American Call (Tree Price): " << treePriceAmer << std::endl;
                std::cout << "  European Call (Tree Price): " << treePriceEuro << std::endl;
                std::cout << "  Early Exercise Premium (American - European): " << (treePriceAmer - treePriceEuro) << std::endl;
                outputFile << "  Underlying: " << commonUnderlying << ", K: " << commonStrike << ", Expiry: " << commonExpiry.toString() << std::endl;
                outputFile << "  American Call (Tree Price): " << treePriceAmer << std::endl;
                outputFile << "  European Call (Tree Price): " << treePriceEuro << std::endl;
                outputFile << "  Early Exercise Premium (American - European): " << (treePriceAmer - treePriceEuro) << std::endl;
            } else {
                std::cout << "\nMatching American Call for comparison not found in portfolio." << std::endl;
                outputFile << "\nMatching American Call for comparison not found in portfolio." << std::endl;
            }
        } else {
            std::cout << "\nNo European Call option found in portfolio for Black-Scholes comparison." << std::endl;
            outputFile << "\nNo European Call option found in portfolio for Black-Scholes comparison." << std::endl;
        }
        outputFile.close();
        std::cout << "\nProject execution finished successfully." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\nUnhandled exception in main: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\nUnknown unhandled exception in main." << std::endl;
        return 1;
    }
    return 0;
}

//Updated
