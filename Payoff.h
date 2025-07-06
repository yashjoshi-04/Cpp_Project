#ifndef PAYOFF_H
#define PAYOFF_H

#include "Types.h" // For OptionType
#include <stdexcept> // For std::runtime_error or similar if needed for default case
#include <algorithm> // For std::max if used inside, though not in current VanillaOption

namespace PAYOFF
{
    inline double VanillaOption(OptionType optType, double strike, double S)
    {
        switch (optType)
        {
        case OptionType::Call:
            return S > strike ? S - strike : 0.0;
        case OptionType::Put:
            return S < strike ? strike - S : 0.0;
        case OptionType::BinaryCall:
            return S >= strike ? 1.0 : 0.0;
        case OptionType::BinaryPut:
            return S <= strike ? 1.0 : 0.0;
        // case OptionType::None: // Explicitly handle if payoff for 'None' should be defined
        //     return 0.0; 
        default:
            // Or return 0.0, or handle as an error, depending on desired behavior for unlisted types.
            // throw std::runtime_error("Unsupported optionType in PAYOFF::VanillaOption"); 
            return 0.0; // Defaulting to 0 for any other type for now
        }
    }

    inline double CallSpread(double strike1, double strike2, double S)
    {
        // Assuming strike1 < strike2 for a standard call spread payoff
        if (S <= strike1) // Corrected condition to <= for consistency
            return 0.0;
        else if (S >= strike2) // Corrected condition to >= 
            return (strike2 - strike1); // Payoff is the difference between strikes if S is above K2
                                       // The original was returning 1, which is more like a binary spread.
                                       // A standard cash call spread payoff is max(0, S-K1) - max(0, S-K2)
                                       // If S < K1, payoff = 0
                                       // If K1 <= S < K2, payoff = S - K1
                                       // If S >= K2, payoff = (S - K1) - (S - K2) = K2 - K1
                                       // The prompt's original CallSpread was: (S-K1)/(K2-K1) for K1<S<K2 and 1 for S>K2.
                                       // This looks like a normalized/digital spread payoff, not a cash payoff.
                                       // Let's stick to the prompt's provided logic for CallSpread for now.
            if (strike1 >= strike2) return 0.0; // Avoid division by zero or negative spread width
            if (S < strike1) return 0.0;
            else if (S > strike2) return 1.0; // As per original prompt's sample code
            else return (S - strike1) / (strike2 - strike1); // As per original prompt's sample code
    }
}
#endif // PAYOFF_H
