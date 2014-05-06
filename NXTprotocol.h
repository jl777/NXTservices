//
//  NXTprotocol.h
//  Created by jl777, March/April 2014
//  MIT License
//

#ifndef gateway_NXTprotocol_h
#define gateway_NXTprotocol_h

#define SYNC_MAXUNREPORTED 32
#define SYNC_FRAGSIZE 1024

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

struct udp_info { uint64_t nxt64bits; uint32_t ipbits; uint16_t port; };
#define SET_UDPINFO(up,bits,addr) ((up)->nxt64bits = bits, (up)->ipbits = calc_ipbits(inet_ntoa((addr)->sin_addr)), (up)->port = ntohs((addr)->sin_port))

struct NXT_acct
{
    struct NXT_str H;
    struct coin_acct *coinaccts;
    struct NXT_asset **assets;
    uint64_t *quantities;
    struct NXT_assettxid_list **txlists;    // one list for each asset in acct
    int32_t maxassets,numassets,numcoinaccts;
    // fields for NXTorrent
    double hisfeedbacks[6],myfb_tohim[6];    // stats on feedbacks given
    // fields for RT comms
    int32_t Usock,recvid,sentid;
    struct sockaddr Uaddr;
    struct udp_info U;
    char dispname[128];
    unsigned char pubkey[crypto_box_PUBLICKEYBYTES];
    uint32_t memcrcs[SYNC_MAXUNREPORTED],localcrcs[SYNC_MAXUNREPORTED];
};


#endif
