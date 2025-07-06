#ifndef _TREE_PRODUCT_H
#define _TREE_PRODUCT_H
#include "Date.h"
#include "Trade.h"
#include <string> // For std::string

// this class provide a common member function interface for option type of trade.
class TreeProduct: public Trade
{
public:
    TreeProduct(): Trade(), underlying("") { tradeType = "TreeProduct";};
    TreeProduct(const std::string& underlyingInstrument) : Trade(), underlying(underlyingInstrument) { tradeType = "TreeProduct";};
    virtual ~TreeProduct() {} // Good practice for base classes with virtual functions

    virtual const Date& GetExpiry() const = 0;
    virtual double ValueAtNode(double stockPrice, double t, double continuationValue) const = 0;

    const std::string& getUnderlying() const { return underlying; }

protected: // Changed to protected so derived classes can access if needed, though getter is public
    std::string underlying;
};

#endif
