#include <fstream>
#include <ctime>
#include <chrono> 
#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <iomanip>    // For std::setprecision, std::fixed
#include <cstdlib>    // For exit() and EXIT_FAILURE
#include <cmath>      // For erf() and sqrt()

#include "Market.h"
#include "Pricer.h"
#include "EuropeanTrade.h" 
#include "AmericanTrade.h" 
#include "Bond.h"
#include "Swap.h"
#include "TreeProduct.h" 
#include "Trade.h"       
#include "Utils.h"       
#include "MathUtils.h"   
#include "Date.h"        


#if (defined(_WIN32) || defined(_WIN64)) && !defined(M_SQRT1_2)
#define M_SQRT1_2 0.70710678118654752440 // 1/sqrt(2)
#endif
// +++++++++++++++++++++++++++++++++++++++++++++++

using namespace std;

// Implements the Cumulative Distribution Function (CDF) for the standard normal distribution.
// This is required for the Black-Scholes formula.
double normalCDF(double value) {
	// This uses the complementary error function, erfc.
	// M_SQRT1_2 is 1/sqrt(2), which is now defined globally for this file.
	return 0.5 * std::erfc(-value * M_SQRT1_2);
}


// readFromFile is defined but not used in main.
void readFromFile(const string& fileName, string& outPut) {
	string lineText;
	ifstream MyReadFile(fileName);
	if (MyReadFile.is_open()) {
		while (getline(MyReadFile, lineText)) {
			outPut.append(lineText);
		}
		MyReadFile.close();
	}
	else {
		cerr << "Error: Could not open file " << fileName << " in readFromFile." << endl;
	}
}

// Helper function to parse tenor string like "ON", "1M", "3M", "1Y" etc. into a Date
Date parseTenorString(const Date& baseDate, const string& tenorStr) {
	Date resultDate = baseDate;
	char unit = ' ';
	int val = 0;

	if (tenorStr == "ON") { // Overnight
		resultDate.day += 1; // Simplified: just add 1 day
		// Basic normalization (a proper date library would handle this robustly)
		if (resultDate.day > 30) { // Simplified month days
			resultDate.month++;
			resultDate.day = 1;
			if (resultDate.month > 12) {
				resultDate.year++;
				resultDate.month = 1;
			}
		}
		return resultDate;
	}

	size_t unitPos = tenorStr.find_first_not_of("0123456789");
	if (unitPos != string::npos && unitPos > 0) {
		try {
			val = std::stoi(tenorStr.substr(0, unitPos));
			unit = tenorStr[unitPos];
		}
		catch (const std::exception& e) {
			cerr << "Warning: Could not parse tenor value from string: " << tenorStr << " (" << e.what() << ")" << endl;
			return baseDate;
		}
	}
	else {
		cerr << "Warning: Could not parse tenor string format: " << tenorStr << endl;
		return baseDate; // Return base date if parsing fails
	}

	if (unit == 'M') { // Months
		resultDate.month += val;
		while (resultDate.month > 12) {
			resultDate.year++;
			resultDate.month -= 12;
		}
	}
	else if (unit == 'Y') { // Years
		resultDate.year += val;
	}
	else {
		cerr << "Warning: Unknown unit '" << unit << "' in tenor string: " << tenorStr << endl;
		return baseDate; // Return base date for unknown units
	}
	if (resultDate.day > 28 && (unit == 'M' || unit == 'Y')) {
		resultDate.day = 28;
		cerr << "Warning: Day clamped to 28 for tenor: " << tenorStr << ", new date: " << resultDate << endl;
	}
	return resultDate;
}

// Function to parse rate string like "5.5%" into a double 0.055
double parseRateString(const string& rateStr) {
	string numPart = rateStr;
	size_t percentPos = rateStr.find('%');
	if (percentPos != string::npos) {
		numPart = rateStr.substr(0, percentPos);
	}
	try {
		return std::stod(numPart) / 100.0;
	}
	catch (const std::invalid_argument& e) {
		cerr << "Warning: Could not parse rate string: '" << rateStr << "'. Error: " << e.what() << endl;
		return 0.0;
	}
	catch (const std::out_of_range& e) {
		cerr << "Warning: Rate string value out of range: '" << rateStr << "'. Error: " << e.what() << endl;
		return 0.0;
	}
}


bool loadTradesFromFile(const std::string& filePath, std::vector<Trade*>& portfolio, const Market& mkt) {
	std::ifstream inputFile(filePath);
	if (!inputFile.is_open()) {
		std::cerr << "Error: Could not open trade file: '" << filePath << "'. Program will terminate." << std::endl;
		return false; // Signal failure
	}

	std::string line;
	int lineNumber = 0;
	bool headerSkipped = false;

	while (std::getline(inputFile, line)) {
		lineNumber++;
		if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos) {
			continue;
		}
		if (!headerSkipped) {
			headerSkipped = true;
			continue; // Skip header row
		}

		std::vector<std::string> tokens;
		splitString(tokens, line, ';');

		if (tokens.size() < 11) {
			std::cerr << "Warning: Skipping line " << lineNumber << ". Incorrect number of columns. Expected at least 11, got " << tokens.size() << ". Line: '" << line << "'" << std::endl;
			continue;
		}

		try {
			std::string type = tokens[1];
			Date trade_dt(tokens[2]);
			Date start_dt(tokens[3]);
			Date end_dt(tokens[4]);
			double notional = std::stod(tokens[5]);
			std::string instrument = tokens[6];
			double rate_param = std::stod(tokens[7]);
			double strike_param = std::stod(tokens[8]);
			double freq_param = std::stod(tokens[9]);
			std::string option_str = tokens[10];

			int paymentsPerYear = 0;
			if (freq_param > 1e-9) {
				paymentsPerYear = static_cast<int>(std::round(1.0 / freq_param));
				if (paymentsPerYear <= 0 && notional > 1e-9) {
					throw std::invalid_argument("Calculated paymentsPerYear is zero or negative from freq_param for a non-zero notional trade.");
				}
			}
			else if (notional > 1e-9 && (type == "swap" || type == "bond")) {
				throw std::invalid_argument("Frequency parameter is zero for a non-zero notional swap/bond.");
			}


			if (type == "swap") {
				portfolio.push_back(new Swap(trade_dt, start_dt, end_dt, notional, rate_param, paymentsPerYear));
			}
			else if (type == "bond") {
				portfolio.push_back(new Bond(instrument, trade_dt, start_dt, end_dt, notional, rate_param, paymentsPerYear, strike_param));
			}
			else if (type == "european" || type == "american") {
				OptionType optTypeVal;
				if (option_str == "call") {
					optTypeVal = Call;
				}
				else if (option_str == "put") {
					optTypeVal = Put;
				}
				else {
					throw std::invalid_argument("Invalid option type: " + option_str);
				}
				Date expiryDate = end_dt;

				if (instrument == "APPL") {
					instrument = "APPL:";
				}

				if (type == "european") {
					portfolio.push_back(new EuropeanOption(optTypeVal, strike_param, expiryDate, instrument));
				}
				else { // american
					portfolio.push_back(new AmericanOption(optTypeVal, strike_param, expiryDate, instrument));
				}
			}
			else {
				std::cerr << "Warning: Skipping line " << lineNumber << ". Unknown trade type: '" << type << "'. Line: '" << line << "'" << std::endl;
			}
		}
		catch (const std::invalid_argument& e) {
			std::cerr << "Warning: Skipping line " << lineNumber << ". Invalid data format (" << e.what() << "). Line: '" << line << "'" << std::endl;
		}
		catch (const std::out_of_range& e) {
			std::cerr << "Warning: Skipping line " << lineNumber << ". Numerical value out of range (" << e.what() << "). Line: '" << line << "'" << std::endl;
		}
		catch (const std::exception& e) {
			std::cerr << "Warning: Skipping line " << lineNumber << ". Unexpected error (" << e.what() << "). Line: '" << line << "'" << std::endl;
		}
	}
	inputFile.close();
	if (portfolio.empty() && lineNumber > 1) {
		std::cerr << "Warning: No valid trades were loaded from file '" << filePath << "'." << std::endl;
	}
	return true;
}

int main()
{
	//task 1, create an market data object, and update the market data from from txt file
	Date valueDate;
	time_t t = time(nullptr);
#if defined(_WIN32) || defined(_WIN64)
	struct tm timeInfo;
	if (localtime_s(&timeInfo, &t) == 0) {
		valueDate.year = timeInfo.tm_year + 1900;
		valueDate.month = timeInfo.tm_mon + 1;
		valueDate.day = timeInfo.tm_mday;
	}
	else {
		std::cerr << "Error: localtime_s failed. Using default date." << std::endl;
		valueDate.year = 1970; valueDate.month = 1; valueDate.day = 1;
	}
#else
	struct tm timeInfo;
	if (localtime_r(&t, &timeInfo) != nullptr) {
		valueDate.year = timeInfo.tm_year + 1900;
		valueDate.month = timeInfo.tm_mon + 1;
		valueDate.day = timeInfo.tm_mday;
	}
	else {
		std::cerr << "Error: localtime_r failed. Using default date." << std::endl;
		valueDate.year = 1970; valueDate.month = 1; valueDate.day = 1;
	}
#endif
	cout << "Value Date: " << valueDate << endl;

	Market mkt0;
	Market mkt1 = Market(valueDate);
	Market mkt2(mkt1);
	Market mkt3;
	mkt3 = mkt2;

	// Load market data from files
	RateCurve usdYieldCurve("USD_Yield_Curve");
	ifstream curveFile("curve.txt");
	if (curveFile.is_open()) {
		string line;
		if (getline(curveFile, line)) { /* Skip header */ }
		while (getline(curveFile, line)) {
			if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
			size_t colonPos = line.find(':');
			if (colonPos != string::npos) {
				string tenorStr = line.substr(0, colonPos);
				string rateStr = line.substr(colonPos + 1);
				rateStr.erase(0, rateStr.find_first_not_of(" \t\n\r\f\v"));
				rateStr.erase(rateStr.find_last_not_of(" \t\n\r\f\v") + 1);

				Date tenorDate = parseTenorString(valueDate, tenorStr);
				double rateVal = parseRateString(rateStr);
				usdYieldCurve.addRate(tenorDate, rateVal);
			}
			else {
				std::cerr << "Warning: Malformed line in curve.txt: " << line << std::endl;
			}
		}
		curveFile.close();
		mkt1.addCurve("USD_Yield_Curve", usdYieldCurve);
		cout << "Loaded USD_Yield_Curve." << endl;
	}
	else {
		std::cerr << "Error: Unable to open curve.txt. Program will terminate." << std::endl;
		exit(EXIT_FAILURE);
	}

	VolCurve spxVolCurve("Vol_Curve");
	ifstream volFile("vol.txt");
	if (volFile.is_open()) {
		string line;
		while (getline(volFile, line)) {
			if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
			size_t colonPos = line.find(':');
			if (colonPos != string::npos) {
				string tenorStr = line.substr(0, colonPos);
				string volStr = line.substr(colonPos + 1);
				volStr.erase(0, volStr.find_first_not_of(" \t\n\r\f\v"));
				volStr.erase(volStr.find_last_not_of(" \t\n\r\f\v") + 1);

				Date tenorDate = parseTenorString(valueDate, tenorStr);
				double volValue = parseRateString(volStr);
				spxVolCurve.addVol(tenorDate, volValue);
			}
			else {
				std::cerr << "Warning: Malformed line in vol.txt: " << line << std::endl;
			}
		}
		volFile.close();
		mkt1.addVolCurve("Vol_Curve", spxVolCurve);
		cout << "Loaded Vol_Curve." << endl;
	}
	else {
		std::cerr << "Error: Unable to open vol.txt. Program will terminate." << std::endl;
		exit(EXIT_FAILURE);
	}

	ifstream stockFile("stockPrice.txt");
	if (stockFile.is_open()) {
		string stockName;
		double stockPriceVal;
		while (stockFile >> stockName >> stockPriceVal) {
			mkt1.addStockPrice(stockName, stockPriceVal);
		}
		stockFile.close();
		cout << "Loaded stock prices." << endl;
	}
	else {
		std::cerr << "Error: Unable to open stockPrice.txt. Program will terminate." << std::endl;
		exit(EXIT_FAILURE);
	}

	mkt1.Print();

	// Create the original hardcoded portfolio
	std::vector<Trade*> originalPortfolio;
	std::cout << "\n--- Creating Original Hardcoded Portfolio ---" << std::endl;
	Date bondMaturity = valueDate;
	bondMaturity.year += 2;
	originalPortfolio.push_back(new Bond("SGD-MAS-BILL", valueDate, valueDate, bondMaturity, 100000.0, 0.025, 2, 101.5));
	std::cout << "Added OriginalBond to originalPortfolio." << std::endl;

	Date swapMaturity = valueDate;
	swapMaturity.year += 5;
	originalPortfolio.push_back(new Swap(valueDate, valueDate, swapMaturity, 1000000.0, 0.045, 2));
	std::cout << "Added OriginalSwap to originalPortfolio." << std::endl;

	double stockPriceForOrigOption = mkt1.getStockPrice("APPL:");
	if (stockPriceForOrigOption < 1e-9 && mkt1.getStockPrice("AAPL") > 1e-9) {
		stockPriceForOrigOption = mkt1.getStockPrice("AAPL");
	}
	else if (stockPriceForOrigOption < 1e-9) {
		std::cerr << "Warning: Could not get stock price for APPL: or AAPL for original option creation, defaulting strike logic to use 100.0." << std::endl;
		stockPriceForOrigOption = 100.0;
	}
	Date europeanOptionExpiryDate = valueDate;
	europeanOptionExpiryDate.month += 6;
	if (europeanOptionExpiryDate.month > 12) {
		europeanOptionExpiryDate.year += (europeanOptionExpiryDate.month - 1) / 12;
		europeanOptionExpiryDate.month = (europeanOptionExpiryDate.month - 1) % 12 + 1;
	}
	double europeanCallStrikeVal = stockPriceForOrigOption * 0.95;
	originalPortfolio.push_back(new EuropeanOption(Call, europeanCallStrikeVal, europeanOptionExpiryDate, "APPL:"));
	std::cout << "Added OriginalEuropeanCall to originalPortfolio." << std::endl;

	originalPortfolio.push_back(new AmericanOption(Call, europeanCallStrikeVal, europeanOptionExpiryDate, "APPL:"));
	std::cout << "Added OriginalAmericanCall to originalPortfolio." << std::endl;

	CRRBinomialTreePricer crrPricerOriginal(20);

	std::cout << "\n--- Pricing Results for Original Hardcoded Portfolio (20 steps) ---" << std::endl;
	for (size_t i = 0; i < originalPortfolio.size(); ++i) {
		Trade* trade = originalPortfolio[i];
		if (trade == nullptr) continue;
		double pv = crrPricerOriginal.Price(mkt1, trade);
		std::string specific_info = "";
		if (auto* bond = dynamic_cast<Bond*>(trade)) {
			std::stringstream ss; ss << bond->getEndDate();
			specific_info = " (Bond: " + bond->getInstrumentName() +
				", K=" + std::to_string(bond->getPrice()) +
				", Maturity=" + ss.str() + ")";
		}
		else if (dynamic_cast<Swap*>(trade)) {
			specific_info = " (Swap)";
		}
		else if (auto* option = dynamic_cast<TreeProduct*>(trade)) {
			std::string opt_type_str = "Unknown Option";
			double strike_val = 0.0;
			std::stringstream ss; ss << option->GetExpiry();
			if (auto* eu_opt = dynamic_cast<EuropeanOption*>(option)) {
				opt_type_str = (eu_opt->getOptionType() == Call ? "Euro Call" : "Euro Put");
				strike_val = eu_opt->getStrike();
			}
			else if (auto* am_opt = dynamic_cast<AmericanOption*>(option)) {
				opt_type_str = (am_opt->getOptionType() == Call ? "Amer Call" : "Amer Put");
				strike_val = am_opt->getStrike();
			}
			specific_info = " (Option: " + option->getUnderlying() +
				", Type: " + opt_type_str +
				", K=" + std::to_string(strike_val) +
				", Expiry=" + ss.str() + ")";
		}
		std::cout << "Original Trade [" << i << "] Type: " << trade->getType() << specific_info
			<< ", PV: " << std::fixed << std::setprecision(2) << pv << std::endl;
	}

	//task 2, load portfolio from trade.txt
	vector<Trade*> myPortfolio;
	if (!loadTradesFromFile("trade.txt", myPortfolio, mkt1)) {
		for (Trade* t : myPortfolio) { if (t) delete t; }
		return 1;
	}
	if (myPortfolio.empty() && ifstream("trade.txt").peek() != ifstream::traits_type::eof()) {
		std::cout << "No trades loaded from file, but trade file was not empty. Check warnings. Exiting." << std::endl;
	}
	else if (myPortfolio.empty()) {
		std::cout << "Trade file is empty or no valid trades found. Proceeding..." << std::endl;
	}


	//task 3, create a pricer and price the portfolio from trade.txt
	Pricer* treePricer = new CRRBinomialTreePricer(50);
	if (auto* crrPricer = dynamic_cast<CRRBinomialTreePricer*>(treePricer)) {
		crrPricer->setFixedRiskFreeRate(0.04);
		std::cout << "INFO: Options in 'myPortfolio' (from file) will be priced with fixed 4% RFR by treePricer." << std::endl;
	}

	cout << "\n--- Pricing Results for L5 Portfolio (from trade.txt) ---" << endl;
	for (size_t i = 0; i < myPortfolio.size(); ++i) {
		Trade* trade = myPortfolio[i];
		if (trade == nullptr) continue;

		double pv = treePricer->Price(mkt1, trade);
		std::string l5_pv_label = "PV";
		std::string l5_trade_details = "";
		if (auto* bond = dynamic_cast<Bond*>(trade)) {
			std::stringstream maturity_ss; maturity_ss << bond->getEndDate();
			l5_trade_details = " (Bond: " + bond->getInstrumentName() +
				", Coupon: " + std::to_string(bond->getCouponRate() * 100.0) + "%" +
				", Freq: " + std::to_string(bond->getCouponFreq()) +
				", Maturity: " + maturity_ss.str() +
				", FilePrice: " + std::to_string(bond->getPrice()) +
				")";
			l5_pv_label = "Theoretical NPV";
		}
		else if (dynamic_cast<Swap*>(trade)) {
			l5_trade_details = " (Swap)";
		}
		else if (auto* option = dynamic_cast<TreeProduct*>(trade)) {
			std::string optionTypeStr = "Unknown Option";
			OptionType optTypeEnum = Call;
			double strike_val = 0.0;
			std::stringstream expiry_ss; expiry_ss << option->GetExpiry();
			if (auto* euroOpt = dynamic_cast<EuropeanOption*>(option)) {
				optTypeEnum = euroOpt->getOptionType();
				optionTypeStr = "Euro " + std::string(optTypeEnum == Call ? "Call" : "Put");
				strike_val = euroOpt->getStrike();
			}
			else if (auto* amerOpt = dynamic_cast<AmericanOption*>(option)) {
				optTypeEnum = amerOpt->getOptionType();
				optionTypeStr = "Amer " + std::string(optTypeEnum == Call ? "Call" : "Put");
				strike_val = amerOpt->getStrike();
			}
			l5_trade_details = " (Option: " + option->getUnderlying() +
				", Type: " + optionTypeStr +
				", K=" + std::to_string(strike_val) +
				", Expiry: " + expiry_ss.str() +
				")";
		}
		std::cout << "L5 Portfolio Trade [" << i << "] Type: " << trade->getType() << l5_trade_details
			<< ", " << l5_pv_label << ": " << std::fixed << std::setprecision(2) << pv << std::endl;
	}

	// task 4, analyzing pricing result
	EuropeanOption* loadedEuropeanCall = nullptr;
	for (Trade* trade : myPortfolio) {
		loadedEuropeanCall = dynamic_cast<EuropeanOption*>(trade);
		if (loadedEuropeanCall && loadedEuropeanCall->getOptionType() == Call) {
			break;
		}
		loadedEuropeanCall = nullptr;
	}

	if (loadedEuropeanCall) {
		std::cout << "\n--- European Call vs Black-Scholes (for first loaded European Call) ---" << std::endl;
		std::string underlying = loadedEuropeanCall->getUnderlying();
		double S = mkt1.getStockPrice(underlying);
		double K = loadedEuropeanCall->getStrike();
		Date expiry = loadedEuropeanCall->GetExpiry();
		double T_bs = expiry - valueDate;
		double r_bs = 0.04;

		double sigma_bs = mkt1.getVolCurve("Vol_Curve").getVol(expiry);
		if (S < 1e-9) {
			std::cerr << "Warning: Stock price for " << underlying << " is zero. BS calculation might be invalid." << std::endl;
		}
		if (sigma_bs < 1e-9) {
			std::cerr << "Warning: Volatility for " << underlying << " at expiry " << expiry << " is zero. BS calculation might be invalid." << std::endl;
			sigma_bs = 0.20;
		}
		if (T_bs < 1e-9) {
			std::cout << "Option " << underlying << " K=" << K << " has expired or T_bs is zero. Skipping Black-Scholes." << std::endl;
		}
		else {
			double treePriceForThisOption = treePricer->Price(mkt1, loadedEuropeanCall);

			double d1 = (std::log(S / K) + (r_bs + sigma_bs * sigma_bs / 2.0) * T_bs) / (sigma_bs * std::sqrt(T_bs));
			double d2 = d1 - sigma_bs * std::sqrt(T_bs);
			double blackScholesCallPrice = S * normalCDF(d1) - K * std::exp(-r_bs * T_bs) * normalCDF(d2);

			std::cout << "Comparing to first European Call Option found in portfolio:" << std::endl;
			std::cout << "Underlying: " << underlying << ", Strike: " << K << ", Expiry: " << expiry << std::endl;
			std::cout << "Parameters: S=" << S << ", K=" << K << ", T=" << T_bs << ", r=" << r_bs << ", sigma=" << sigma_bs << std::endl;
			std::cout << "Tree Model PV (50 steps, 0.04 RFR): " << std::fixed << std::setprecision(2) << treePriceForThisOption << std::endl;
			std::cout << "Black-Scholes PV: " << std::fixed << std::setprecision(2) << blackScholesCallPrice << std::endl;
		}
	}
	else {
		std::cout << "\n--- European Call vs Black-Scholes ---" << std::endl;
		std::cout << "No European Call option found in loaded portfolio to compare with Black-Scholes." << std::endl;
	}

	std::cout << "\n--- Black-Scholes Comparison for Original European Call ---" << std::endl;
	EuropeanOption* originalEuCall = nullptr;
	for (Trade* trade : originalPortfolio) {
		EuropeanOption* tempOpt = dynamic_cast<EuropeanOption*>(trade);
		if (tempOpt && tempOpt->getOptionType() == Call && tempOpt->getUnderlying() == "APPL:") {
			originalEuCall = tempOpt;
			break;
		}
	}

	if (originalEuCall) {
		std::string underlying = originalEuCall->getUnderlying();
		double S_orig = mkt1.getStockPrice(underlying);
		if (S_orig < 1e-9 && underlying == "APPL:") {
			S_orig = mkt1.getStockPrice("AAPL");
			if (S_orig < 1e-9) {
				std::cerr << "Warning: Could not get stock price for " << underlying << " or AAPL for original BS calc. Defaulting S to 100." << std::endl;
				S_orig = 100.0;
			}
		}

		double K_orig = originalEuCall->getStrike();
		Date expiry_orig = originalEuCall->GetExpiry();
		double T_bs_orig = expiry_orig - valueDate;

		RateCurve yieldCurve_orig = mkt1.getCurve("USD_Yield_Curve");
		double r_bs_orig = 0.0;
		if (yieldCurve_orig.isEmpty()) {
			std::cerr << "Warning: 'USD_Yield_Curve' is empty. Using 0.0 risk-free rate for original BS calculation." << std::endl;
		}
		else {
			r_bs_orig = yieldCurve_orig.getRate(expiry_orig);
		}

		VolCurve volCurve_orig = mkt1.getVolCurve("Vol_Curve");
		double sigma_bs_orig = 0.2;
		if (volCurve_orig.isEmpty()) {
			std::cerr << "Warning: 'Vol_Curve' is empty. Using 0.2 default volatility for original BS calculation." << std::endl;
		}
		else {
			sigma_bs_orig = volCurve_orig.getVol(expiry_orig);
		}
		if (sigma_bs_orig < 1e-9) {
			std::cerr << "Warning: Volatility for original BS calc is zero. Defaulting to 0.2." << std::endl;
			sigma_bs_orig = 0.2;
		}


		if (T_bs_orig <= 1e-6) {
			std::cout << "Original European Call option has expired or T_bs is zero/negative. Skipping Black-Scholes calculation." << std::endl;
			std::cout << "Expiry: " << expiry_orig << ", ValueDate: " << valueDate << ", T_bs: " << T_bs_orig << std::endl;
		}
		else if (S_orig <= 1e-6 || K_orig <= 1e-6) {
			std::cout << "Invalid parameters for Black-Scholes for Original European Call (S=" << S_orig << ", K=" << K_orig << "). Skipping calculation." << std::endl;
		}
		else {
			double d1_orig = (std::log(S_orig / K_orig) + (r_bs_orig + sigma_bs_orig * sigma_bs_orig / 2.0) * T_bs_orig) / (sigma_bs_orig * std::sqrt(T_bs_orig));
			double d2_orig = d1_orig - sigma_bs_orig * std::sqrt(T_bs_orig);
			double blackScholesCallPrice_orig = S_orig * normalCDF(d1_orig) - K_orig * std::exp(-r_bs_orig * T_bs_orig) * normalCDF(d2_orig);

			double treePrice_orig_for_bs = crrPricerOriginal.Price(mkt1, originalEuCall);

			std::cout << "Original European Call (" << underlying << " K=" << K_orig << ", Expiry=" << expiry_orig << "):" << std::endl;
			std::cout << "  Parameters: S=" << S_orig << ", K=" << K_orig << ", T=" << T_bs_orig << ", r=" << r_bs_orig << ", sigma=" << sigma_bs_orig << std::endl;
			std::cout << "  Tree Model PV (20 steps, market rates): " << std::fixed << std::setprecision(2) << treePrice_orig_for_bs << std::endl;
			std::cout << "  Black-Scholes PV (market rate r=" << r_bs_orig << ", sigma=" << sigma_bs_orig << "): " << std::fixed << std::setprecision(2) << blackScholesCallPrice_orig << std::endl;
		}
	}
	else {
		std::cout << "Original European Call option not found in originalPortfolio for Black-Scholes comparison." << std::endl;
	}

	std::cout << "\n--- American vs European Options (Original Hardcoded Portfolio) ---" << std::endl;
	EuropeanOption* origEuCall_forComp = nullptr;
	AmericanOption* origAmCall_forComp = nullptr;
	for (Trade* t : originalPortfolio) {
		if (!origEuCall_forComp) {
			EuropeanOption* tempEu = dynamic_cast<EuropeanOption*>(t);
			if (tempEu && tempEu->getOptionType() == Call && tempEu->getUnderlying() == "APPL:") origEuCall_forComp = tempEu;
		}
		if (!origAmCall_forComp) {
			AmericanOption* tempAm = dynamic_cast<AmericanOption*>(t);
			if (tempAm && tempAm->getOptionType() == Call && tempAm->getUnderlying() == "APPL:") origAmCall_forComp = tempAm;
		}
	}

	for (auto trade : originalPortfolio) {
		if (trade) {
			delete trade;
		}
	}
	originalPortfolio.clear();

	for (auto trade : myPortfolio) {
		if (trade) {
			delete trade;
		}
	}
	myPortfolio.clear();

	delete treePricer;

	return 0;
}