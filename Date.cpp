#include "Date.h"
#include <vector>
#include <numeric>   // For std::accumulate (not used in this version but often useful)
#include <algorithm> // For std::tolower
#include <iomanip>   // For std::setw, std::setfill in toString (optional)

// --- Helper function to convert string to lower case (local to this file) ---
namespace { // Anonymous namespace for internal linkage
std::string toLower(const std::string& str) {
    std::string lower_str = str;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return lower_str;
}
} // anonymous namespace

// --- Date Class Member Function Implementations ---

bool Date::isGregorianLeap(int y_val) {
    return (y_val % 4 == 0 && (y_val % 100 != 0 || y_val % 400 == 0));
}

void Date::calculateSerialNumber() {
    // Implements logic from prompt's getSerialDate() to calculate Excel serial
    long excelSerial = 0;
    for (int y_iter = 1900; y_iter < year; ++y_iter) {
        bool leap = (y_iter % 4 == 0 && (y_iter % 100 != 0 || y_iter % 400 == 0));
        if (y_iter == 1900) leap = true; // Excel rule: 1900 is a leap year
        excelSerial += leap ? 366 : 365;
    }
    
    int monthDays[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; // days[0] is dummy
    bool currentYearIsExcelLeap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    if (year == 1900) currentYearIsExcelLeap = true; // Excel rule
    
    if (currentYearIsExcelLeap) monthDays[2] = 29;

    for (int m_iter = 1; m_iter < month; ++m_iter) {
        excelSerial += monthDays[m_iter];
    }
    excelSerial += day;
    this->serialNumber = excelSerial;
}

void Date::calculateYMD() {
    // Implements logic from prompt's serialToDate()
    long sn = this->serialNumber;
    
    if (sn == 60) { // Excel's representation for Feb 29, 1900
        this->year = 1900;
        this->month = 2;
        this->day = 29;
        return;
    }
    
    long dayNumForCalc = sn;
    if (sn > 60) { // If serial > 60, it means it's past Feb 28, 1900.
                   // The \"day number\" in a pure Gregorian sense is one less due to Excel's phantom Feb 29, 1900.
        dayNumForCalc--;
    }

    this->year = 1900;
    while (true) {
        bool leap = isGregorianLeap(this->year); // Use standard Gregorian for calendar progression
        long daysInYear = leap ? 366 : 365;
        if (dayNumForCalc <= daysInYear) {
            break;
        }
        dayNumForCalc -= daysInYear;
        this->year++;
    }

    int monthDays[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (isGregorianLeap(this->year)) { // Use standard Gregorian for days in month
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
     if (this->day == 0 && dayNumForCalc > 0) { // Should not happen if logic is correct
        throw std::logic_error("Day calculation resulted in 0 incorrectly.");
    }
}

Date::Date(int y, int m, int d) : year(y), month(m), day(d) {
    if (m < 1 || m > 12 || d < 1 || d > 31 || y < 1800 || y > 9999) { // Basic validation
        throw std::out_of_range("Invalid year, month, or day in Date constructor.");
    }
    calculateSerialNumber();
}

Date::Date() : year(1900), month(1), day(1) { // Default to Excel epoch start
    calculateSerialNumber(); // serialNumber will be 1
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
             throw std::out_of_range("Date component out of valid range.");
        }
    } catch (const std::exception& e) { // Catches stoi errors or out_of_range from stoi
        throw std::invalid_argument("Error parsing date string '" + dateStr + "': " + e.what());
    }
    calculateSerialNumber();
}

long Date::getSerialDate() const {
    return this->serialNumber;
}

void Date::setFromSerial(long serial) {
    if (serial <= 0) { // Excel serials are positive
        throw std::out_of_range("Serial number must be positive.");
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

// --- Standalone Function Implementations ---

Date dateAddTenor(const Date& startDate, const std::string& tenorStr) {
    std::string lowerTenor = toLower(tenorStr);
    long currentSerial = startDate.getSerialDate();
    long newSerial = currentSerial;

    if (lowerTenor == "on" || lowerTenor == "o/n") {
        newSerial = currentSerial + 1;
    } else {
        if (lowerTenor.empty()) throw std::runtime_error("Empty tenor string.");
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
            // Approximation based on prompt's sample: numUnit * 30 days
            newSerial = currentSerial + numUnits * 30;
        } else if (unit == 'y') { 
            // Approximation based on prompt's sample: numUnit * 360 days
            newSerial = currentSerial + numUnits * 360;
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
    return static_cast<double>(diff_serial) / 365.0; // Simple Act/365
}

std::ostream& operator<<(std::ostream& os, const Date& date) {
    os << date.toString();
    return os;
}

// Expects YYYY MM DD for simplicity, or could be enhanced for YYYY-MM-DD
std::istream& operator>>(std::istream& is, Date& date) {
    int y, m, d;
    char dash1 = 0, dash2 = 0;
    
    is >> y;
    if(is.peek() == '-'){
        is.ignore(); // Consume the '-'
        is >> m;
    } else {
        is >> m;
    }
    if(is.peek() == '-'){
        is.ignore(); // Consume the '-'
        is >> d;
    } else {
        is >> d;
    }

    if (!is.fail()) {
        date = Date(y, m, d); 
    } else {
        // Set failbit if parsing failed or format was unexpected
        is.setstate(std::ios_base::failbit);
    }
    return is;
}

//Updated
