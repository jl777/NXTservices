//
//  multistructs.h
//  Created by jl777, March/April 2014
//  MIT License
//


#ifndef gateway_multistructs_h
#define gateway_multistructs_h

struct req_deposit
{
    uint64_t nxt64bits,modified,AMtxidbits;
    char NXTaddr[MAX_NXTADDR_LEN],NXTtxid[MAX_NXTADDR_LEN],cointxid[MAX_COINTXID_LEN];;
};

struct pubkey_info { char pubkey[128],coinaddr[64],server[64]; };

struct multisig_addr
{
    char NXTaddr[MAX_NXTADDR_LEN],multisigaddr[MAX_COINADDR_LEN],redeemScript[256];
    struct pubkey_info pubkeys[NUM_GATEWAYS];
    int32_t m,n,coinid,rippletag;
    int64_t maxunspent,unspent;
};

struct crosschain_NXTinfo
{
    uint64_t asset_txid,confirmedAMbits;
    int32_t confirmed,pending;
};

struct crosschain_info
{
    uint64_t nxt64bits;
    int64_t value,assetoshis;
    struct multisig_addr *msig;
    struct coin_value *parent;
    int32_t NXTRTflag,NXTxfermask,coin_confirmed,isinternal;
    struct crosschain_NXTinfo NXT;
};

struct coin_value
{
    int64_t modified,value;
    struct crosschain_info *xp;
    struct coin_txid *parent,*spent,*pendingspend;
    union { char *script; char *coinbase; };
    int32_t parent_vout,spent_vin,pending_spendvin,isconfirmed,iscoinbase,isinternal;
    char coinaddr[MAX_COINADDR_LEN];
};

struct coin_txid
{
    int64_t modified;
    uint64_t confirmedAMbits,NXTxferbits;
    char txid[MAX_COINTXID_LEN];
    int32_t numvins,numvouts,hasinternal,height;
    struct coin_value **vins,**vouts;
};

struct rawtransaction
{
    char *destaddrs[MAX_MULTISIG_OUTPUTS];
    struct coin_value *inputs[MAX_MULTISIG_INPUTS];
    int64_t amount,change,inputsum,destamounts[MAX_MULTISIG_OUTPUTS];
    int32_t numoutputs,numinputs;
    char rawtransaction[4096],signedtransaction[4096],txid[64];
};

struct withdraw_info
{
    struct server_request_header H;
    uint64_t modified,AMtxidbits;
    int64_t amount,moneysent;
    int32_t coinid,srcgateway,destgateway,twofactor,authenticated;
    char cointxid[MAX_COINTXID_LEN],withdrawaddr[64],NXTaddr[MAX_NXTADDR_LEN],redeemtxid[MAX_NXTADDR_LEN],comment[1024];
    struct rawtransaction rawtx;
};

struct unspent_info
{
    int64_t maxunspent,unspent,maxavail,minavail,smallest_msigunspent;
    struct coin_value **vps,*maxvp,*minvp;
    int32_t maxvps,num;
    char smallest_msig[128];
};

struct balance_info { int64_t deposits,transfers,withdraws,moneysends,remaining_transfers,remaining_moneysends,balance; };

struct coin_acct
{
    struct balance_info funds;
    struct multisig_addr **msigs;
    int32_t enable_2FA,nummsigs,coinid;
    char withdrawaddr[MAX_COINADDR_LEN];
};

struct replicated_coininfo // runs on all nodes and only needs NXT blockchain
{
    char name[64],assetid[64];
    int32_t coinid,timestamp,nummsigs,numxps,numtransfers,numredemptions,nummoneysends,pollseconds;
    int64_t txfee,markeramount,NXTheight;
    struct balance_info funds;
    struct unspent_info unspent;
    pthread_mutex_t msigs_mutex,xp_mutex;
    struct crosschain_info **xps;
    struct hashtable *coin_txids,*redeemtxids;
    struct pingpong_queue retrydepositconfirmed,withdrawreqs,withdraws;
    queue_t errorqueue;
    struct multisig_addr **msigs;
    struct withdraw_info **wps;
    
    // only valid for actual multigateways
    struct hashtable *genaddr_txids;
    struct pingpong_queue genreqs,genaddr,authenticate,sendcoin;
};

struct daemon_info
{
    int32_t numretries,mismatch,coinid;
    int32_t (*validate_coinaddr)(struct daemon_info *cp,int32_t coinid,char *coinaddr);
    int32_t (*add_unique_txids)(struct daemon_info *cp,int32_t coinid,int64_t blockheight,int32_t RTmode);
    int64_t (*get_blockheight)(struct daemon_info *cp,int32_t coinid);
    char *(*calc_rawtransaction)(struct daemon_info *cp,int32_t coinid,struct rawtransaction *rp,char *destaddr,int64_t amout);
    // only for actual multigateways
    struct multisig_addr *(*gen_depositaddr)(struct daemon_info *cp,int32_t coinid,char *NXTaddr);
    char *(*submit_withdraw)(struct daemon_info *cp,int32_t coinid,struct withdraw_info *wp,struct withdraw_info *otherwp);
};

struct gateway_info
{
    queue_t errorqueue[64];
    char *name[64],*assetid[64],*internalmarker[64],*serverport[64],*userpass[64],*walletbackup[64];
    int32_t min_confirms[64],blockheight[64],numinitlist[64],backupcount[64];
    int32_t initdone[64],*initlist[64];

    pthread_mutex_t consensus_mutex[64];
    struct withdraw_info withdrawinfos[64][NUM_GATEWAYS];
    int32_t gensocks[NUM_GATEWAYS],wdsocks[NUM_GATEWAYS];
    char gateways[64][NUM_GATEWAYS][64];
    struct replicated_coininfo *replicants[64];
    struct daemon_info *daemons[64];
    int32_t gatewayid,timestamp;
};

#endif
