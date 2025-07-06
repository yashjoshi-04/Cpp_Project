#include "Utils.h"
// Required headers are already included via Utils.h or are standard enough (like for size_t)
// No other specific includes like <iostream> or <cctype> are needed for this version.

void splitString(std::vector<std::string>& output, const std::string& inputLine, char separator) {
    output.clear();
    // if (inputLine.empty()) { // Optional: handle empty input line explicitly if desired, though stringstream handles it.
    //     return;
    // }
    std::stringstream ss(inputLine);
    std::string segment;

    while (std::getline(ss, segment, separator)) {
        size_t first = segment.find_first_not_of(" \t\r\n"); // Added space to trim
        size_t last = segment.find_last_not_of(" \t\r\n");  // Added space to trim
        if (first == std::string::npos) { // Segment is all whitespace or empty
            output.push_back("");
        }
        else {
            output.push_back(segment.substr(first, (last - first + 1)));
        }
    }
    // Handle case where the line ends with the separator, adding an empty string at the end
    if (!inputLine.empty() && inputLine.back() == separator) {
        output.push_back("");
    }
}
