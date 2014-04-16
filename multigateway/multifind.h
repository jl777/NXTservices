//
//  multifind.h
//  Created by jl777 March/April, 2014
//  MIT License
//


#ifndef gateway_multifind_h
#define gateway_multifind_h


struct replicated_coininfo *get_replicated_coininfo(int32_t coinid)
{
    struct gateway_info *gp = Global_gp;
    /*if ( gp->replicants[coinid] == 0 )
    {
        printf("replicated_coininfo NULL for coinid.%d??\n",coinid);
        while ( 1 ) sleep(1);
    }*/
    return(gp->replicants[coinid]);
}

struct unspent_info *get_unspent_info(int32_t coinid)
{
    struct replicated_coininfo *lp = get_replicated_coininfo(coinid);
    if ( lp == 0 )
        return(0);
    return(&lp->unspent);
}

struct balance_info *get_balance_info(int32_t coinid)
{
    struct replicated_coininfo *lp = get_replicated_coininfo(coinid);
    if ( lp == 0 )
        return(0);
    return(&lp->funds);
}

struct multisig_addr **get_multisigs(int32_t *nump,int32_t coinid)
{
    struct replicated_coininfo *lp = get_replicated_coininfo(coinid);
    if ( lp == 0 )
        return(0);
    *nump = lp->nummsigs;
    return(lp->msigs);
}

char *coinid_str(int32_t coinid)
{
    switch ( coinid )
    {
        case BTC_COINID: return("BTC");
        case LTC_COINID: return("LTC");
        case CGB_COINID: return("CGB");
        case DOGE_COINID: return("DOGE");
        case DRK_COINID: return("DRK");
        case USD_COINID: return("USD");
        case CNY_COINID: return("CNY");
    }
    return(ILLEGAL_COIN);
}

char *assetid_str(int32_t coinid)
{
    switch ( coinid )
    {
        case BTC_COINID: return(BTC_COINASSET);
        case LTC_COINID: return(LTC_COINASSET);
        case CGB_COINID: return(CGB_COINASSET);
        case DOGE_COINID: return(DOGE_COINASSET);
        case DRK_COINID: return(DRK_COINASSET);
        //case USD_COINID: return(USD_COINASSET);
        //case CNY_COINID: return(CNY_COINASSET);
    }
    return(ILLEGAL_COINASSET);
}

void add_multisig_addr(int32_t coinid,struct coin_acct *acct,struct multisig_addr *msig)
{
    struct replicated_coininfo *lp = get_replicated_coininfo(coinid);
    if ( lp == 0 )
        return;
    printf("acct.%d lp.%d msig.%p %d NXT.%s deposit %s -> %s\n",acct->nummsigs,lp->nummsigs,msig,acct->nummsigs,msig->NXTaddr,coinid_str(coinid),msig->multisigaddr);
    acct->msigs = realloc(acct->msigs,sizeof(*acct->msigs) * (acct->nummsigs+1));
    acct->msigs[acct->nummsigs] = msig;
    acct->nummsigs++;

    lp->msigs = realloc(lp->msigs,sizeof(*lp->msigs) * (lp->nummsigs+1));
    lp->msigs[lp->nummsigs] = msig;
    lp->nummsigs++;
}

struct multisig_addr *search_multisig_addrs(int32_t coinid,char *coinaddr)
{
    int32_t i;
    struct replicated_coininfo *lp = get_replicated_coininfo(coinid);
    struct multisig_addr *msig = 0;
    if ( lp == 0 )
        return(0);
    //pthread_mutex_lock(&cp->msigs_mutex);
    for (i=0; i<lp->nummsigs; i++)
    {
        if ( strcmp(lp->msigs[i]->multisigaddr,coinaddr) == 0 )
        {
            msig = lp->msigs[i];
            break;
        }
    }
    //pthread_mutex_unlock(&cp->msigs_mutex);
    return(msig);
}

int32_t conv_coinstr(char *name)
{
    int32_t i;
    for (i=0; i<64; i++)
        if ( strcmp(coinid_str(i),name) == 0 )
            return(i);
    return(-1);
}

int32_t conv_assetid(char *assetid)
{
    int32_t i;
    for (i=0; i<64; i++)
        if ( strcmp(assetid_str(i),assetid) == 0 )
            return(i);
    return(-1);
}

#define get_coin_info(coinid) ((coinid >= 0 && coinid < 64) ? Global_gp->daemons[coinid] : 0)
#define get_daemon_info get_coin_info
#define find_coin_info(coinid) get_coin_info(get_coin_info(coinid));
#define find_daemon_info find_coin_info

int32_t get_gatewayid(char *ipaddr)
{
    int32_t i;
    for (i=0; i<NUM_GATEWAYS; i++)
    {
        if ( strcmp(ipaddr,Server_names[i]) == 0 )
            return(i);
    }
    printf("(%s) is not gateway server\n",ipaddr);
    return(-1);
}

struct coin_acct *ensure_coin_acct(struct NXT_acct *np,int32_t coinid)
{
    int32_t i;
    struct coin_acct *ap = 0;
    //printf("ensure coin acct %s coinid.%d %s numcoin accounts.%d\n",nxt64str(np->H.nxt64bits),coinid,coinid_str(coinid),np->numcoinaccts);
    for (i=0; i<np->numcoinaccts; i++)
    {
        if ( np->coinaccts[i].coinid == coinid )
            return(&np->coinaccts[i]);
    }
    np->numcoinaccts++;
    np->coinaccts = realloc(np->coinaccts,sizeof(*np->coinaccts) * np->numcoinaccts);
    ap = &np->coinaccts[np->numcoinaccts-1];
    memset(ap,0,sizeof(*ap));
    ap->coinid = coinid;
    return(ap);
}

struct coin_acct *get_coin_acct(int32_t coinid,char *NXTaddr)
{
    char *coinname = coinid_str(coinid);
    int32_t createdflag;
    struct NXT_acct *np;
    if ( strcmp(coinname,ILLEGAL_COIN) == 0 )
        return(0);
    np = get_NXTacct(&createdflag,Global_mp,NXTaddr);
    if ( np != 0 )
        return(ensure_coin_acct(np,coinid));
    return(0);
}

char *get_deposit_addr(int32_t coinid,char *NXTaddr)
{
    struct coin_acct *acct;
    if ( (acct= get_coin_acct(coinid,NXTaddr)) != 0 && acct->msigs != 0 )
        return(acct->msigs[0]->multisigaddr);
    else return(0);
}

char *find_user_withdrawaddr(int32_t coinid,uint64_t nxt64bits)
{
    struct coin_acct *ap;
    char NXTaddr[64];
    expand_nxt64bits(NXTaddr,nxt64bits);
    ap = get_coin_acct(coinid,NXTaddr);
    if ( ap != 0 && ap->withdrawaddr[0] != 0 )
        return(ap->withdrawaddr);
    return(0);
}

struct coin_txid *find_coin_txid(int32_t coinid,char *txid)
{
    struct replicated_coininfo *lp = get_replicated_coininfo(coinid);
    uint64_t hashval;
    if ( lp == 0 )
        return(0);
    hashval = MTsearch_hashtable(&lp->coin_txids,txid);
    if ( hashval == HASHSEARCH_ERROR )
        return(0);
    else return(lp->coin_txids->hashtable[hashval]);
}

#endif
