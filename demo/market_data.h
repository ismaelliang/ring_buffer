#ifndef MARKET_DATA_H
#define MARKET_DATA_H

#pragma pack(push, 1)
struct MarketData
{
    char symbol[16];
    double price;
    int volume;
    long timestamp;
};
#pragma pack(pop)

#endif // MARKET_DATA_H
