#include <iostream>
#include <vector>
#include <string>
#include <memory>     // For std::shared_ptr, std::make_unique
#include <fstream>    // For std::ofstream
#include <sstream>    // For std::istringstream (in loadTradesFromFile)
#include <iomanip>    // For std::fixed, std::setprecision
#include <stdexcept>  // For std::exception handling
#include <algorithm>  // For std::transform 
#include <chrono>     // For system_clock to get current date
#include <cmath>      // For std::round
#include <ctime>      // For std::time_t, std::tm, localtime_s/localtime_r

// Project Headers
#include "Date.h"
#include "Market.h"
#include "Trade.h"
#include "Bond.h"
#include "Swap.h"
#include "EuropeanTrade.h"
#include "AmericanTrade.h"
#include "TreeProduct.h"   
#include "Pricer.h"
#include "TradeFactory.h"
#include "MarketDecorators.h"
#include "RiskEngine.h"
#include "Types.h"         
#include "MathUtils.h"     
#include "Utils.h"         

namespace {
    std::string toLowerMainCpp(const std::string& str) { 
        std::string data = str;
        std::transform(data.begin(), data.end(), data.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        return data;
    }
} 

bool loadTradesFromFile(
    const std::string& filePath,
    std::vector<std::shared_ptr<Trade>>& portfolio,
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

    // Expects 13 or 14 fields from your confirmed trade.txt format
    // 0:id; 1:type; 2:trade_dt; 3:start_dt; 4:end_dt; 5:notional; 6:instrument; 
    // 7:rate(bond/swap); 8:strike(option); 9:freq(bond/swap, decimal); 10:option_type; 
    // 11:DiscountCurve; 12:VolCurve; 13:FloatForecastCurve(optional)

    while (std::getline(inputFile, line)) {
        lineNumber++;
        if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos || line[0] == '#') {
            continue; 
        }
        if (!headerSkipped && 
            (toLowerMainCpp(line).find("id;type;trade_dt") != std::string::npos || 
             toLowerMainCpp(line).find("discount_curve;vol_curve") != std::string::npos )) {
            headerSkipped = true; 
            std::cout << "Info: Header line detected and skipped in '" << filePath << "': '" << line << "'" << std::endl;
            continue; 
        }
        if (!headerSkipped && lineNumber == 1) {
            std::cout << "Info: First line of '" << filePath << "' ('" << line << "') is not a recognized header. Assuming data from line 1." << std::endl;
        }

        std::vector<std::string> tokens;
        splitString(tokens, line, ';'); 

        if (tokens.size() < 13) { // Need at least 13 fields (up to VolCurve)
            std::cerr << "Warning: Skipping line " << lineNumber << " in '" << filePath 
                      << "'. Incorrect number of columns. Expected at least 13, got " << tokens.size() 
                      << ". Line: '" << line << "'" << std::endl;
            continue;
        }

        try {
            std::string type_str = toLowerMainCpp(tokens[1]);
            Date tradeDate_obj(tokens[2]);
            Date startDate_obj(tokens[3]);
            Date endDate_obj(tokens[4]); 
            double notional_val = std::stod(tokens[5]);
            std::string instrument_str = tokens[6];
            
            double rate_param = 0.0;      
            double strike_param = 0.0;    
            double freq_decimal_param = 0.0; 
            int frequency_val = 0;        

            if (!tokens[7].empty()) rate_param = std::stod(tokens[7]);
            if (!tokens[8].empty()) strike_param = std::stod(tokens[8]);
            if (!tokens[9].empty()) freq_decimal_param = std::stod(tokens[9]);

            if (type_str == "bond" || type_str == "swap") {
                if (std::abs(freq_decimal_param) > 1e-9) { 
                    if (std::abs(freq_decimal_param - static_cast<int>(freq_decimal_param)) < 1e-6 ) { 
                        frequency_val = static_cast<int>(freq_decimal_param);
                    } else { 
                        frequency_val = static_cast<int>(std::round(1.0 / freq_decimal_param));
                    }
                    if (frequency_val <= 0 && std::abs(rate_param)>1e-9) frequency_val = 1; 
                } else {
                    frequency_val = (std::abs(rate_param) > 1e-9) ? 1 : 0; // If coupon/rate exists, assume freq 1 if not specified
                }
            } else { 
                frequency_val = 0;
            }

            std::string optionType_str = toLowerMainCpp(tokens[10]);
            std::string disc_curve_str = tokens[11];
            std::string vol_curve_str = tokens[12]; 
            std::string float_fcst_curve_str = (tokens.size() > 13 && !tokens[13].empty()) ? tokens[13] : disc_curve_str; 

            OptionType optType_enum = OptionType::None; 
            if (type_str == "european" || type_str == "american") {
                if (optionType_str == "call") optType_enum = OptionType::Call;
                else if (optionType_str == "put") optType_enum = OptionType::Put;
                else if (optionType_str == "binarycall") optType_enum = OptionType::BinaryCall;
                else if (optionType_str == "binaryput") optType_enum = OptionType::BinaryPut;
                else if (optionType_str != "none" && !optionType_str.empty() && optionType_str != "na") {
                     throw std::invalid_argument("Unknown option type in trade.txt: " + tokens[10]);
                }
            }

            std::shared_ptr<Trade> newTrade = nullptr;
            double primary_rate_strike_for_factory = 0.0;
            if (type_str == "bond") primary_rate_strike_for_factory = rate_param; 
            else if (type_str == "swap") primary_rate_strike_for_factory = rate_param; 
            else if (type_str == "european" || type_str == "american") primary_rate_strike_for_factory = strike_param; 

            if (type_str == "bond") {
                newTrade = bondFactory.createTrade(instrument_str, tradeDate_obj, startDate_obj, endDate_obj, 
                                                   notional_val, primary_rate_strike_for_factory, frequency_val, 
                                                   optType_enum, disc_curve_str, vol_curve_str, float_fcst_curve_str);
            } else if (type_str == "swap") {
                newTrade = swapFactory.createTrade(instrument_str, tradeDate_obj, startDate_obj, endDate_obj, 
                                                   notional_val, primary_rate_strike_for_factory, frequency_val, 
                                                   optType_enum, disc_curve_str, vol_curve_str, float_fcst_curve_str);
            } else if (type_str == "european") {
                newTrade = euroOptFactory.createTrade(instrument_str, tradeDate_obj, startDate_obj, endDate_obj, 
                                                      notional_val, primary_rate_strike_for_factory, frequency_val, 
                                                      optType_enum, disc_curve_str, vol_curve_str, float_fcst_curve_str);
            } else if (type_str == "american") {
                newTrade = amerOptFactory.createTrade(instrument_str, tradeDate_obj, startDate_obj, endDate_obj, 
                                                      notional_val, primary_rate_strike_for_factory, frequency_val, 
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
        
        // Load USD-SOFR from curve.txt (assuming curve.txt contains it and loader can find/use the name)
        if (!market.loadCurveDataFromFile("curve.txt", "USD-SOFR" /* Hint for loader if file has no internal name */, "USD-SOFR" /* Key to store in market */)) { 
             std::cerr << "Warning: Failed to load USD-SOFR curve data from curve.txt." << std::endl;
        }
        // Attempt to load SGD-SORA if sgd_curve.txt exists
        // User needs to provide this file or modify curve.txt and the loader for multiple curves in one file.
        if (!market.loadCurveDataFromFile("sgd_curve.txt", "SGD-SORA", "SGD-SORA")) { 
             std::cout << "Info: SGD-SORA curve data not loaded (e.g., sgd_curve.txt missing or empty)." << std::endl;
        } else {
             std::cout << "Info: Successfully loaded SGD-SORA curve data from sgd_curve.txt." << std::endl;
        }

        // Load a default/generic Volatility Curve
        if (!market.loadVolDataFromFile("vol.txt", "VOL_CURVE_DEFAULT", "VOL_CURVE_DEFAULT")) { 
             std::cerr << "Warning: Failed to load default volatility curve data from vol.txt (VOL_CURVE_DEFAULT)." << std::endl;
        }
        // Load a specific vol curve for APPL if trade.txt uses VOL_APPL and you have a file for it
        // For example, if you create vol_appl.txt with APPL specific vols.
        if (!market.loadVolDataFromFile("vol_appl.txt", "VOL_APPL", "VOL_APPL")) { 
             std::cout << "Info: VOL_APPL curve data not loaded (e.g., vol_appl.txt missing or empty). Trades referencing it will try VOL_CURVE_DEFAULT if not found." << std::endl;
        } else {
             std::cout << "Info: Successfully loaded VOL_APPL curve data from vol_appl.txt." << std::endl;
        }

        if (!market.loadStockPricesFromFile("stockPrice.txt")) {
             std::cerr << "Warning: Failed to load stock price data from stockPrice.txt." << std::endl;
        }
        if (!market.loadBondPricesFromFile("bondPrice.txt")) {
            std::cerr << "Warning: Failed to load bond price data from bondPrice.txt." << std::endl;
        }
        market.Print();

        BondFactory bondFactory;
        SwapFactory swapFactory;
        EuropeanOptionFactory euroOptFactory;
        AmericanOptionFactory amerOptFactory;

        std::vector<std::shared_ptr<Trade>> portfolio;
        std::cout << "\nLoading Portfolio from trade.txt..." << std::endl;
        if (!loadTradesFromFile("trade.txt", portfolio, bondFactory, swapFactory, euroOptFactory, amerOptFactory)) {
            std::cerr << "Warning: Could not load any trades from trade.txt or file not found. Portfolio may be empty." << std::endl;
        } else {
            std::cout << "Loaded " << portfolio.size() << " trades into the portfolio." << std::endl;
        }

        auto treePricer = std::make_unique<CRRBinomialTreePricer>(50); // 50 steps as per requirement
        std::ofstream outputFile("results.txt");
        if (!outputFile.is_open()) {
            std::cerr << "CRITICAL Error: Could not open results.txt for writing. Terminating." << std::endl;
            return 1;
        }
        outputFile << std::fixed << std::setprecision(6); 
        std::cout << std::fixed << std::setprecision(6);  

        outputFile << "Instrument;Type;PV;DV01_Curve;DV01_Value;Vega_Curve;Vega_Value" << std::endl;

        std::cout << "\nCalculating PV and Greeks for Portfolio..." << std::endl;
        RiskEngine riskEngine(0.0001, 0.01); // 1bp IR shock (0.0001), 1% Vol shock (0.01)

        for (const auto& trade : portfolio) {
            if (!trade) continue;
            std::string tradeIdForOutput = trade->getUnderlyingName(); // Default to underlying
            // If your trades had a specific ID field you parsed, you could use that here.
            std::cout << "Processing Trade: " << tradeIdForOutput << " (" << trade->getType() << ")" << std::endl;
            outputFile << tradeIdForOutput << ";" << trade->getType() << ";";
            double pv = 0.0;
            try {
                pv = treePricer->Price(market, trade);
                std::cout << "  PV: " << pv << std::endl;
                outputFile << pv << ";";
            } catch (const std::exception& e) {
                std::cerr << "  Error pricing trade " << tradeIdForOutput << " (" << trade->getType() << "): " << e.what() << std::endl;
                outputFile << "ErrorPricing;" ;
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
                std::cerr << "  Error calculating DV01 for " << tradeIdForOutput << " (" << trade->getType() << "): " << e.what() << std::endl;
                outputFile << "ErrorDV01;0.0;";
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
                std::cerr << "  Error calculating Vega for " << tradeIdForOutput << " (" << trade->getType() << "): " << e.what() << std::endl;
                outputFile << "ErrorVega;0.0";
            }
            outputFile << std::endl;
        }
        std::cout << "\nResults written to results.txt" << std::endl;

        std::cout << "\n--- Comparison Data for Write-up ---" << std::endl;
        outputFile << "\n--- Comparison Data for Write-up ---" << std::endl;
        std::shared_ptr<EuropeanOption> euroCallForComparison = nullptr;
        std::shared_ptr<AmericanOption> amerCallForComparison = nullptr;
        std::string commonUnderlyingComp = "";
        double commonStrikeComp = 0.0;
        Date commonExpiryComp;
        // Find first European Call to use as a base for comparisons
        for (const auto& trade : portfolio) {
            if (auto ec = std::dynamic_pointer_cast<EuropeanOption>(trade)) {
                if (ec->getOptionType() == OptionType::Call) {
                    euroCallForComparison = ec;
                    commonUnderlyingComp = ec->getUnderlyingName();
                    commonStrikeComp = ec->getStrike();
                    commonExpiryComp = ec->GetExpiry();
                    break; // Found one, use it
                }
            }
        }
        // Try to find a matching American Call
        if (euroCallForComparison) {
            for (const auto& trade : portfolio) {
                if (auto ac = std::dynamic_pointer_cast<AmericanOption>(trade)) {
                    if (ac->getOptionType() == OptionType::Call && 
                        ac->getUnderlyingName() == commonUnderlyingComp && 
                        std::abs(ac->getStrike() - commonStrikeComp) < 1e-6 && 
                        ac->GetExpiry() == commonExpiryComp) {
                        amerCallForComparison = ac;
                        break;
                    }
                }
            }
        }

        if (euroCallForComparison) {
            std::string ulyName = euroCallForComparison->getUnderlyingName();
            double K = euroCallForComparison->getStrike();
            Date expiry = euroCallForComparison->GetExpiry();
            std::string title = "European Option vs. Black-Scholes: (" + ulyName + " K" + std::to_string(K) + " Exp:" + expiry.toString() + ")";
            std::cout << "\n" << title << std::endl;
            outputFile << "\n" << title << std::endl;
            double treePriceEuro = 0.0; 
            try { treePriceEuro = treePricer->Price(market, euroCallForComparison); } catch (const std::exception& e) { std::cerr << "Error pricing Euro Call for BS comparison: " << e.what() << std::endl; }
            
            double S = market.getStockPrice(ulyName);
            double T = (expiry - valueDate);
            std::shared_ptr<const RateCurve> rCurve = market.getCurve(euroCallForComparison->getRateCurveName());
            std::shared_ptr<const VolCurve> vCurve = market.getVolCurve(euroCallForComparison->getVolCurveName());
            if (rCurve && vCurve && T >= -1e-9) { 
                T = std::max(0.0, T);
                double r = rCurve->getRate(expiry);
                double sigma = vCurve->getVol(expiry);
                if (sigma <= 1e-9) {
                    std::cout << "  Warning: Volatility for Black-Scholes is zero or very low (" << sigma << "). BS price may be intrinsic only." << std::endl;
                    outputFile << "  Warning: Volatility for Black-Scholes is zero or very low (" << sigma << "). BS price may be intrinsic only." << std::endl;
                }
                try {
                    double bsPrice = blackScholesPrice(euroCallForComparison->getOptionType(), S, K, T, r, sigma);
                    std::cout << "  Parameters for BS: S=" << S << ", K=" << K << ", T=" << T << ", r=" << r << ", sigma=" << sigma << std::endl;
                    outputFile << "  Parameters for BS: S=" << S << ", K=" << K << ", T=" << T << ", r=" << r << ", sigma=" << sigma << std::endl;
                    std::cout << "  Binomial Tree Price (50 steps): " << treePriceEuro << std::endl;
                    std::cout << "  Black-Scholes Price: " << bsPrice << std::endl;
                    std::cout << "  Difference (Tree - BS): " << (treePriceEuro - bsPrice) << std::endl;
                    outputFile << "  Binomial Tree Price (50 steps): " << treePriceEuro << std::endl;
                    outputFile << "  Black-Scholes Price: " << bsPrice << std::endl;
                    outputFile << "  Difference (Tree - BS): " << (treePriceEuro - bsPrice) << std::endl;
                } catch (const std::exception& e) {
                    std::cout << "  Error calculating Black-Scholes price: " << e.what() << std::endl;
                    outputFile << "  Error calculating Black-Scholes price: " << e.what() << std::endl;
                }
            } else {
                std::cout << "  Could not calculate Black-Scholes price for " << ulyName << " (missing market data or T<0). RateCurve valid: " << (rCurve!=nullptr) << ", VolCurve valid: " << (vCurve!=nullptr) << ", T: " << T << std::endl;
                outputFile << "  Could not calculate Black-Scholes price for " << ulyName << " (missing market data or T<0)." << std::endl;
            }
            if (amerCallForComparison) {
                std::string titleAmer = "American Call vs. European Call (Same Parameters - " + commonUnderlyingComp + " K" + std::to_string(commonStrikeComp) + " Exp:" + commonExpiryComp.toString() + ")";
                std::cout << "\n" << titleAmer << std::endl;
                outputFile << "\n" << titleAmer << std::endl;
                double treePriceAmer = 0.0;
                try { treePriceAmer = treePricer->Price(market, amerCallForComparison); } catch (const std::exception& e) { std::cerr << "Error pricing Amer Call for comparison: " << e.what() << std::endl; }
                std::cout << "  American Call (Tree Price): " << treePriceAmer << std::endl;
                std::cout << "  European Call (Tree Price): " << treePriceEuro << std::endl;
                std::cout << "  Early Exercise Premium (American - European): " << (treePriceAmer - treePriceEuro) << std::endl;
                outputFile << "  American Call (Tree Price): " << treePriceAmer << std::endl;
                outputFile << "  European Call (Tree Price): " << treePriceEuro << std::endl;
                outputFile << "  Early Exercise Premium (American - European): " << (treePriceAmer - treePriceEuro) << std::endl;
            } else {
                std::string missingAmerInfo = "\nMatching American Call for comparison not found in portfolio (Looked for Underlying: " + commonUnderlyingComp + ", K: " + std::to_string(commonStrikeComp) + ", Exp: " + commonExpiryComp.toString() + ").";
                if (commonUnderlyingComp.empty() && euroCallForComparison) missingAmerInfo = "\nNo European call was found to base American option comparison on.";
                else if (commonUnderlyingComp.empty()) missingAmerInfo = "\nNo European call was found, so no American option comparison attempted.";
                std::cout << missingAmerInfo << std::endl;
                outputFile << missingAmerInfo << std::endl;
            }
        } else {
            std::cout << "\nNo European Call option found in portfolio for Black-Scholes/American comparison." << std::endl;
            outputFile << "\nNo European Call option found in portfolio for Black-Scholes/American comparison." << std::endl;
        }
        outputFile.close();
        std::cout << "\nProject execution finished." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "\nUnhandled exception in main: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\nUnknown unhandled exception in main." << std::endl;
        return 1;
    }
    return 0;
}
//Updated again
