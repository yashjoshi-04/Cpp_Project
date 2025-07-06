#ifndef MARKET_H
#define MARKET_H

#include <iostream>
#include <vector>
#include <unordered_map>
#include <cstring> // Added for strlen and strcpy
#include "Date.h"

using namespace std;

class RateCurve {
public:
	RateCurve() {};
	RateCurve(const string& _name) : name(_name) {};
	void addRate(Date tenor, double rate);
	double getRate(Date tenor) const; //implement this function using linear interpolation
	void display() const;
	bool isEmpty() const { return tenorDates.empty(); } // Added isEmpty

private:
	std::string name;
	vector<Date> tenorDates;
	vector<double> rates;
};

class VolCurve { // atm vol curve without smile
public:
	VolCurve() {}
	VolCurve(const string& _name) : name(_name) {};
	void addVol(Date tenor, double rate); //implement this
	double getVol(Date tenor) const; //implement this function using linear interpolation
	void display() const; //implement this
	bool isEmpty() const { return tenors.empty(); } // Added isEmpty

private:
	string name;
	vector<Date> tenors;
	vector<double> vols;
};

class Market
{
public:
	Date asOf;
	char* name;

	Market() : name(nullptr) { // Initialize name to nullptr
		// cout << "default market constructor is called by object@" << this << endl; // Removed
	}

	Market(const Date& now) : asOf(now) {
		// cout << "market constructor is called by object@" << this << endl; // Removed
		const char* testName = "test";
		size_t len = strlen(testName) + 1;
		name = new char[len];
		#if defined(_WIN32) || defined(_WIN64)
			strcpy_s(name, len, testName);
		#else
			strcpy(name, testName);
		#endif
	}

	Market(const Market& other) : asOf(other.asOf), name(nullptr) { // Initialize name to nullptr
		// cout << "copy constructor is called by object@" << this << endl; // Removed
		// deep copy behaviour
		if (other.name != nullptr) {
			size_t len = strlen(other.name) + 1;
			name = new char[len];
			#if defined(_WIN32) || defined(_WIN64)
				strcpy_s(name, len, other.name);
			#else
				strcpy(name, other.name);
			#endif
		}
	}

	Market& operator=(const Market& other) {
		// cout << "assignment constructor is called by object@" << this << endl; // Removed
		if (this != &other) {  // Check for self-assignment
			asOf = other.asOf;
			// Deep copy for name
			if (name != nullptr) {
				delete[] name;
				name = nullptr;
			}
			if (other.name != nullptr) {
				size_t len = strlen(other.name) + 1;
				name = new char[len];
				#if defined(_WIN32) || defined(_WIN64)
					strcpy_s(name, len, other.name);
				#else
					strcpy(name, other.name);
				#endif
			}
			// Copy other members (unordered_maps are handled by their own assignment operators)
			vols = other.vols;
			curves = other.curves;
			bondPrices = other.bondPrices;
			stockPrices = other.stockPrices;
		}
		return *this;
	}

	~Market() {
		// cout << "Market destructor is called" << endl; // Removed
		if (name != nullptr)
			delete[] name; // Use delete[] for arrays
	}

	void Print() const;
	void addCurve(const std::string& curveName, const RateCurve& curve);//implement this
	void addVolCurve(const std::string& curveName, const VolCurve& curve);
	void addBondPrice(const std::string& bondName, double price);
	void addStockPrice(const std::string& stockName, double price);
    double getStockPrice(const std::string& stockName) const {
		auto it = stockPrices.find(stockName);
		if (it != stockPrices.end()) {
			return it->second;
		} else {
			std::cerr << "Error: Stock price for '" << stockName << "' not found. Returning 0.0." << std::endl;
			return 0.0;
		}
	}
    // void PrintStockKeysForDebug() const; // DEBUG REMOVED

	inline RateCurve getCurve(const string& name) const {
		auto it = curves.find(name);
		if (it != curves.end()) {
			return it->second;
		} else {
			std::cerr << "Error: Rate curve '" << name << "' not found. Returning empty curve." << std::endl;
			return RateCurve(); // Return a default-constructed (empty) RateCurve
		}
	}
	inline VolCurve getVolCurve(const string& name) const {
		auto it = vols.find(name);
		if (it != vols.end()) {
			return it->second;
		} else {
			std::cerr << "Error: Vol curve '" << name << "' not found. Returning empty curve." << std::endl;
			return VolCurve(); // Return a default-constructed (empty) VolCurve
		}
	}

private:

	unordered_map<string, VolCurve> vols;
	unordered_map<string, RateCurve> curves;
	unordered_map<string, double> bondPrices;
	unordered_map<string, double> stockPrices;
};

std::ostream& operator<<(std::ostream& os, const Market& obj);
std::istream& operator>>(std::istream& is, Market& obj);

#endif
