//
//  NXTprotocol.h
//  Created by jl777, March/April 2014
//  MIT License
//

#ifndef gateway_NXTprotocol_h
#define gateway_NXTprotocol_h

struct NXT_guid
{
    struct NXT_str H;
    char guid[128 - sizeof(struct NXT_str)];
};

struct NXT_assettxid
{
    struct NXT_str H;
    uint64_t txidbits,assetbits,senderbits,receiverbits,quantity,price; // price 0 -> not buy/sell
    struct NXT_guid *duplicatehash;
    void *cointx;
    char *guid,*comment;
    int32_t hackattempts;
};

struct NXT_assettxid_list
{
    struct NXT_assettxid **txids;
    int32_t num,max;
};

struct NXT_asset
{
    struct NXT_str H;
    uint64_t assetbits;
    struct NXT_assettxid **txids;   // all transactions for this asset
    int32_t max,num;
};

struct NXT_acct
{
    struct NXT_str H;
    struct coin_acct *coinaccts;
    struct NXT_asset **assets;
    uint64_t *quantities;
    int32_t maxassets,numassets,numcoinaccts;
    struct NXT_assettxid_list **txlists;    // one list for each asset in acct
};


#endif
