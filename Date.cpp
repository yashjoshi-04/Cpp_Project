#include "Date.h"
#include <string>    // For std::string, std::stoi
#include <stdexcept> // For std::invalid_argument, std::out_of_range
// #include <vector> // Not strictly needed for this impl, but good if other split methods were used

// New constructor implementation
Date::Date(const std::string& dateStr) : year(0), month(0), day(0) { // Initialize to default/invalid
    if (dateStr.length() != 10) {
        throw std::invalid_argument("Date string length is not 10 (YYYY-MM-DD): " + dateStr);
    }
    if (dateStr[4] != '-' || dateStr[7] != '-') {
        throw std::invalid_argument("Date string format is not YYYY-MM-DD (hyphens missing or misplaced): " + dateStr);
    }

    try {
        year = std::stoi(dateStr.substr(0, 4));
        month = std::stoi(dateStr.substr(5, 2));
        day = std::stoi(dateStr.substr(8, 2));

        // Basic validation (can be more sophisticated)
        if (month < 1 || month > 12 || day < 1 || day > 31) { // Simplified day check
            throw std::invalid_argument("Invalid month or day in date string: " + dateStr);
        }
        // Could add year > 0 check etc.
    }
    catch (const std::invalid_argument& e) { // Catch stoi specific conversion errors
        throw std::invalid_argument("Invalid number format in date string segments (YYYY-MM-DD): " + dateStr + ". Original error: " + e.what());
    }
    catch (const std::out_of_range& e) { // Catch stoi out of range errors
        throw std::out_of_range("Numeric value out of range in date string segments (YYYY-MM-DD): " + dateStr + ". Original error: " + e.what());
    }
}

// Comparison operator implementations
bool Date::operator<(const Date& other) const {
    if (year < other.year) return true;
    if (year > other.year) return false;
    if (month < other.month) return true;
    if (month > other.month) return false;
    return day < other.day;
}

bool Date::operator<=(const Date& other) const {
    return (*this < other) || (*this == other);
}

bool Date::operator>(const Date& other) const {
    return !(*this <= other);
}

bool Date::operator==(const Date& other) const {
    return year == other.year && month == other.month && day == other.day;
}

bool Date::operator!=(const Date& other) const {
    return !(*this == other);
}

//return date difference in fraction of year
double operator-(const Date& d1, const Date& d2)
{
    int yearDiff = d1.year - d2.year;
    int monthDiff = (d1.month - d2.month);
    int dayDiff = d1.day - d2.day;
    return yearDiff + monthDiff / 12.0 + dayDiff / 365.0;
}

std::ostream& operator<<(std::ostream& os, const Date& d)
{
    os << d.year << "-" << d.month << "-" << d.day; // Removed std::endl
    return os;
}

std::istream& operator>>(std::istream& is, Date& d)
{
    is >> d.year >> d.month >> d.day;
    return is;
}

std::string Date::toString() const {
    std::string monthStr = (month < 10 ? "0" : "") + std::to_string(month);
    std::string dayStr = (day < 10 ? "0" : "") + std::to_string(day);
    return std::to_string(year) + "-" + monthStr + "-" + dayStr;
}