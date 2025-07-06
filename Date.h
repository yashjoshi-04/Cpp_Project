#ifndef DATE_H
#define DATE_H

#include <iostream>
#include <string>
#include <stdexcept> // For std::runtime_error, std::out_of_range

class Date {
public:
    int year;
    int month;
    int day;
    long serialNumber; // Excel-compatible serial number

    // Constructors
    Date(int y, int m, int d);
    Date(); // Default constructor (e.g., 1900-01-01 or invalid date)
    explicit Date(const std::string& dateStr); // From \"YYYY-MM-DD\"

    // Serial date functions
    long getSerialDate() const;
    void setFromSerial(long serial); // Sets y,m,d from an Excel-compatible serial

    // Comparison operators
    bool operator<(const Date& other) const;
    bool operator<=(const Date& other) const;
    bool operator>(const Date& other) const;
    bool operator==(const Date& other) const;
    bool operator!=(const Date& other) const;

    // Utility
    std::string toString() const;

private:
    void calculateSerialNumber(); // Calculate Excel serial from y,m,d
    void calculateYMD();      // Calculate y,m,d from Excel serial
    
    static bool isGregorianLeap(int y_val); // Standard Gregorian leap year
};

// Standalone utility functions related to Date
Date dateAddTenor(const Date& startDate, const std::string& tenorStr);
double operator-(const Date& d1, const Date& d2); // Difference in years (Act/365 simple)

std::ostream& operator<<(std::ostream& os, const Date& date);
std::istream& operator>>(std::istream& is, Date& date); // Expects YYYY MM DD

#endif // DATE_H
//Updated
