#ifndef DATE_H
#define DATE_H

#include <iostream>

class Date
{
public:
	int year = 0;
	int month = 0;
	int day = 0;
	Date(int y, int m, int d) : year(y), month(m), day(d) {};
	Date() {
		// std::cout << "default Date constructor called" << std::endl; // Removed
	};
	Date(const std::string& dateStr); // New constructor

	// Comparison operators
	bool operator<(const Date& other) const;
	bool operator<=(const Date& other) const;
	bool operator>(const Date& other) const;
	bool operator==(const Date& other) const;
	bool operator!=(const Date& other) const; // Often useful too
	std::string toString() const; // Added toString method
};

double operator-(const Date& d1, const Date& d2);
std::ostream& operator<<(std::ostream& os, const Date& date);
std::istream& operator>>(std::istream& is, Date& date);

#endif