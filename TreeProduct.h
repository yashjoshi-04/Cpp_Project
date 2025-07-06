#ifndef _TREE_PRODUCT_H
#define _TREE_PRODUCT_H

#include "Date.h"
#include "Trade.h" // Trade::getUnderlyingName() is virtual here
#include <string> 

class TreeProduct: public Trade
{
public:
    TreeProduct(): Trade(), underlying("") { 
        tradeType = "TreeProduct"; // Set tradeType from base Trade class
    }
    TreeProduct(const std::string& underlyingInstrument) 
        : Trade(), underlying(underlyingInstrument) { 
        tradeType = "TreeProduct";
        // Ensure the underlyingName in the base Trade part is also set if it's used anywhere,
        // though direct access to TreeProduct::underlying is better for options.
        // For consistency with the virtual function, we override it.
    }
    virtual ~TreeProduct() = default;

    virtual const Date& GetExpiry() const = 0;
    virtual double ValueAtNode(double stockPrice, double t, double continuationValue) const = 0;

    // This is the direct accessor for TreeProduct's own underlying string.
    const std::string& getUnderlying() const { return underlying; }

    // Override Trade's virtual function to return the correct underlying name.
    std::string getUnderlyingName() const override { return underlying; }

protected: 
    std::string underlying;
};

#endif // _TREE_PRODUCT_H
