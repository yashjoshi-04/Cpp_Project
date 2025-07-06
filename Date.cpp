#include "Date.h"
#include <vector>
#include <numeric>   
#include <algorithm> 
#include <iomanip>   
#include <sstream>   // For std::istringstream in operator>>

namespace { 
std::string toLowerDateCpp(const std::string& str) { // Renamed for clarity
    std::string lower_str = str;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return lower_str;
}
} 

bool Date::isGregorianLeap(int y_val) {
    return (y_val % 4 == 0 && (y_val % 100 != 0 || y_val % 400 == 0));
}

void Date::calculateSerialNumber() {
    long excelSerial = 0;
    for (int y_iter = 1900; y_iter < year; ++y_iter) {
        bool leap = (y_iter % 4 == 0 && (y_iter % 100 != 0 || y_iter % 400 == 0));
        if (y_iter == 1900) leap = true; 
        excelSerial += leap ? 366 : 365;
    }
    
    int monthDays[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; 
    bool currentYearIsExcelLeap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    if (year == 1900) currentYearIsExcelLeap = true; 
    
    if (currentYearIsExcelLeap) monthDays[2] = 29;

    for (int m_iter = 1; m_iter < month; ++m_iter) {
        excelSerial += monthDays[m_iter];
    }
    excelSerial += day;
    this->serialNumber = excelSerial;
}

void Date::calculateYMD() {
    long sn = this->serialNumber;
    
    if (sn == 60) { 
        this->year = 1900;
        this->month = 2;
        this->day = 29;
        return;
    }
    
    long dayNumForCalc = sn;
    if (sn > 60) { 
        dayNumForCalc--;
    }

    this->year = 1900;
    while (true) {
        bool leap = isGregorianLeap(this->year); 
        long daysInYear = leap ? 366 : 365;
        if (dayNumForCalc <= daysInYear) {
            break;
        }
        dayNumForCalc -= daysInYear;
        this->year++;
    }

    int monthDays[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (isGregorianLeap(this->year)) { 
        monthDays[2] = 29;
    }

    this->month = 1;
    while (true) {
        if (this->month > 12) throw std::logic_error("Month calculation failed in Date::calculateYMD - month exceeded 12");
        if (dayNumForCalc <= monthDays[this->month]) {
            break;
        }
        dayNumForCalc -= monthDays[this->month];
        this->month++;
    }
    this->day = static_cast<int>(dayNumForCalc);
     if (this->day == 0 && dayNumForCalc > 0 && sn > 0) { 
        // This case should ideally not be hit if serial number is valid and logic is perfect.
        // If day becomes 0, it might indicate an issue with the last day of a month or serial 0.
        // For serial 1 (1900-01-01), dayNumForCalc = 1, month = 1, day = 1. Correct.
        throw std::logic_error("Day calculation resulted in 0 incorrectly in Date::calculateYMD.");
    }
}

Date::Date(int y, int m, int d) : year(y), month(m), day(d) {
    if (m < 1 || m > 12 || d < 1 || d > 31 || y < 1800 || y > 9999) { 
        std::string errorMsg = "Invalid year(" + std::to_string(y) + 
                               "), month(" + std::to_string(m) + 
                               "), or day(" + std::to_string(d) + ") in Date constructor.";
        throw std::out_of_range(errorMsg);
    }
    calculateSerialNumber();
}

Date::Date() : year(1900), month(1), day(1) { 
    calculateSerialNumber(); 
}

Date::Date(const std::string& dateStr) {
    if (dateStr.length() != 10 || dateStr[4] != '-' || dateStr[7] != '-') {
        throw std::invalid_argument("Date string format must be YYYY-MM-DD: " + dateStr);
    }
    try {
        this->year = std::stoi(dateStr.substr(0, 4));
        this->month = std::stoi(dateStr.substr(5, 2));
        this->day = std::stoi(dateStr.substr(8, 2));
        if (this->month < 1 || this->month > 12 || this->day < 1 || this->day > 31 || this->year < 1800 || this->year > 9999) {
             throw std::out_of_range("Date component out of valid range in string constructor.");
        }
    } catch (const std::exception& e) { 
        throw std::invalid_argument("Error parsing date string '" + dateStr + "': " + e.what());
    }
    calculateSerialNumber();
}

long Date::getSerialDate() const {
    return this->serialNumber;
}

void Date::setFromSerial(long serial) {
    if (serial <= 0) { 
        throw std::out_of_range("Serial number must be positive for Date::setFromSerial.");
    }
    this->serialNumber = serial;
    calculateYMD();
}

bool Date::operator<(const Date& other) const { return this->serialNumber < other.serialNumber; }
bool Date::operator<=(const Date& other) const { return this->serialNumber <= other.serialNumber; }
bool Date::operator>(const Date& other) const { return this->serialNumber > other.serialNumber; }
bool Date::operator==(const Date& other) const { return this->serialNumber == other.serialNumber; }
bool Date::operator!=(const Date& other) const { return this->serialNumber != other.serialNumber; }

std::string Date::toString() const {
    char buffer[11];
    #if defined(_WIN32) || defined(_WIN64)
        sprintf_s(buffer, sizeof(buffer), "%04d-%02d-%02d", year, month, day);
    #else
        snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", year, month, day);
    #endif
    return std::string(buffer);
}

Date dateAddTenor(const Date& startDate, const std::string& tenorStr) {
    std::string lowerTenor = toLowerDateCpp(tenorStr);
    long currentSerial = startDate.getSerialDate();
    long newSerial = currentSerial;

    if (lowerTenor == "on" || lowerTenor == "o/n") {
        newSerial = currentSerial + 1;
    } else {
        if (lowerTenor.empty()) throw std::runtime_error("Empty tenor string in dateAddTenor.");
        char unit = lowerTenor.back();
        std::string numPartStr = lowerTenor.substr(0, lowerTenor.length() - 1);
        if (numPartStr.empty()) throw std::runtime_error("Tenor string missing number: " + tenorStr);
        
        int numUnits = 0;
        try {
            numUnits = std::stoi(numPartStr);
        } catch (const std::exception& e) {
            throw std::runtime_error("Invalid number in tenor string '" + tenorStr + "': " + e.what());
        }

        if (unit == 'd') {
            newSerial = currentSerial + numUnits;
        } else if (unit == 'w') {
            newSerial = currentSerial + numUnits * 7;
        } else if (unit == 'm') { 
            newSerial = currentSerial + numUnits * 30; // Approximation
        } else if (unit == 'y') { 
            newSerial = currentSerial + numUnits * 360; // Approximation
        } else {
            throw std::runtime_error("Unsupported tenor unit '" + std::string(1, unit) + "' in tenor: " + tenorStr);
        }
    }
    if (newSerial <= 0) throw std::runtime_error("Calculated new serial is non-positive in dateAddTenor");
    Date newDate; 
    newDate.setFromSerial(newSerial);
    return newDate;
}

double operator-(const Date& d1, const Date& d2) {
    long diff_serial = d1.getSerialDate() - d2.getSerialDate();
    return static_cast<double>(diff_serial) / 365.0; 
}

std::ostream& operator<<(std::ostream& os, const Date& date) {
    os << date.toString();
    return os;
}

std::istream& operator>>(std::istream& is, Date& date) {
    int y, m, d;
    // char dash1_unused = 0, dash2_unused = 0; // Marked unused
    char separator_char = 0;

    is >> y;
    if (is.peek() == '-') { is >> separator_char; }
    is >> m;
    if (is.peek() == '-') { is >> separator_char; }
    is >> d;

    if (!is.fail()) {
        date = Date(y, m, d); 
    } else {
        is.setstate(std::ios_base::failbit);
    }
    return is;
}
