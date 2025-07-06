#include "Bond.h"
#include <cmath>
#include <vector>
#include <numeric>

double Bond::Payoff(double yield) const { // Removed inline

    (void)yield; // Mark yield as unused to potentially suppress compiler warnings.
    return (tradePrice / 100.0 * bondNotional);
}
