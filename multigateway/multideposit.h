//
//  multideposit.h
//  Created by jl777, March 2014
//  MIT License
//


#ifndef gateway_multideposit_h
#define gateway_multideposit_h

uint64_t broadcast_depositconfirmedAM(int32_t coinid,struct crosschain_info *xp,char *NXTaddr)
{
    uint64_t AMtxidbits = 0;
    char AM[4096],*jsontxt,*AMtxid = 0;
    struct json_AM *ap = (struct json_AM *)AM;
    jsontxt = create_depositconfirmed_jsontxt(coinid,xp_coinaddr(xp),xp_txid(xp),xp_vout(xp),xp->value,xp->NXT.asset_txid,xp->assetoshis,NXTaddr);
    if ( jsontxt != 0 )
    {
        set_json_AM(ap,GATEWAY_SIG,DEPOSIT_CONFIRMED,NXTaddr,Global_mp->timestamp,jsontxt,1);
        AMtxid = submit_AM(NXTISSUERACCT,&ap->H,0);
        if ( AMtxid != 0 )
        {
            AMtxidbits = calc_nxt64bits(AMtxid);
            printf("broadcastdeposit confirmed %s\n",AMtxid);
        } else printf("Error submitting asset xfer\n");
        free(jsontxt);
    } else printf("transfer_asset error creating JSON?\n");
    if ( AMtxid != 0 )
        free(AMtxid);
    return(AMtxidbits);
}

void add_confirmed_deposit(struct coin_txid *tp,int32_t coinid,struct crosschain_info *xp)
{
    struct replicated_coininfo *lp = get_replicated_coininfo(coinid);
    if ( lp == 0 )
        return;
    pthread_mutex_lock(&lp->xp_mutex);
    lp->xps = realloc(lp->xps,sizeof(*lp->xps) * (lp->numxps+1));
    lp->xps[lp->numxps] = xp;
    lp->numxps++;
    pthread_mutex_unlock(&lp->xp_mutex);
    if ( tp->confirmedAMbits == 0 )
    {
        if ( xp->NXT.confirmedAMbits == 0 && (xp->nxt64bits % NUM_GATEWAYS) == Global_gp->gatewayid )
            xp->NXT.confirmedAMbits = broadcast_depositconfirmedAM(coinid,xp,xp_NXTaddr(xp));
    } else xp->NXT.confirmedAMbits = tp->confirmedAMbits;

    deposit_NXTacct(coinid,xp_NXTaddr(xp),xp->value);
    printf("Confirmed deposit.%d %s %s %s %.8f -> NXT.%s\n",lp->numxps,coinid_str(coinid),xp_coinaddr(xp),xp_txid(xp),dstr(xp->value),xp_NXTaddr(xp));
}

struct crosschain_info **get_confirmed_deposits(int32_t *nump,int32_t coinid)
{
    struct replicated_coininfo *lp = get_replicated_coininfo(coinid);
    struct crosschain_info **xps = 0;
    int32_t n,i,m = 0;
    if ( lp == 0 )
        return(0);
    pthread_mutex_lock(&lp->xp_mutex);
    n = lp->numxps;
    if ( n != 0 )
    {
        xps = malloc(sizeof(*xps) * n);
        for (i=m=0; i<n; i++)
            if ( lp->xps[i] != 0 )
                xps[m++] = lp->xps[i];
    }
    *nump = m;
    pthread_mutex_unlock(&lp->xp_mutex);
    if ( m == 0 && xps != 0 )
        free(xps), xps = 0;
    printf("get_confirmed_deposits %s returns %d\n",coinid_str(coinid),m);
    return(xps);
}

// called from bitcoin.h and other low level to fill out crosschain info, especially to confirm deposits
struct crosschain_info *update_crosschain_info(int32_t coinid,struct coin_txid *tp,struct coin_value *vp,int32_t isconfirmed,struct crosschain_info *xp,uint64_t value,char *coinaddr)
{
    char NXTaddr[64];
    struct multisig_addr *msig;
    struct crosschain_info *confirmed_deposit = 0;
    struct NXThandler_info *mp = Global_mp;
    if ( xp == 0 )
        xp = calloc(1,sizeof(*xp));
    xp->value = value;
    xp->parent = vp;
    xp->assetoshis = value;
    if ( xp->isinternal != 0 )
        confirmed_deposit = xp;
    else
    {
        xp->NXTxfermask = 1;
        msig = search_multisig_addrs(coinid,coinaddr);
        if ( msig != 0 )
        {
            xp->msig = msig;
            xp->nxt64bits = calc_nxt64bits(xp->msig->NXTaddr);
            confirmed_deposit = xp;
        }
        //if ( strcmp("9zz4q4QBmc23tpVpRtZLv7oGbdEo9ukGLq",coinaddr) == 0 )
        //    printf("update_crosschain_info 9zz4q4QBmc23tpVpRtZLv7oGbdEo9ukGLq msig.%p isconfirmed.%d\n",msig,isconfirmed);
    }
    if ( confirmed_deposit != 0 && confirmed_deposit->coin_confirmed == 0 && isconfirmed != 0 )
    {
        confirmed_deposit->NXTRTflag = mp->RTflag;
        confirmed_deposit->coin_confirmed += isconfirmed;
        add_confirmed_deposit(tp,coinid,confirmed_deposit);
        if ( tp->NXTxferbits != 0 )
        {
            printf("already did NXTxfer tp %p pending.%d confirmed.%d\n",xp->parent->parent,xp->NXT.pending,xp->NXT.confirmed);
            xp->NXT.confirmed = 1;  // if asset was transferred, must have been confirmed
            if ( xp->NXT.pending == 0 )  // must do this once and only once
            {
                expand_nxt64bits(NXTaddr,xp->nxt64bits);
                deposit_transfer_NXTacct(coinid,NXTaddr,xp->value);
            }
            xp->NXT.pending |= 1;
        }
    }
    return(xp);
}

int32_t set_pending_assetxfer(int32_t coinid,char *coinaddr,char *cointxid,int32_t vout,int64_t value,uint64_t nxt64bits,uint64_t NXTtxid,int64_t assetoshis)
{
    int32_t NXTpending = 0;
    char NXTaddr[64];
    struct coin_txid *tp = 0;
    struct coin_value *vp = 0;
    struct crosschain_info *xp = 0;
    tp = find_coin_txid(coinid,cointxid);
    if ( tp != 0 && vout >= 0 && vout < tp->numvouts )
    {
        vp = tp->vouts[vout];
        if ( vp != 0 )
            xp = vp->xp;
    }
    expand_nxt64bits(NXTaddr,nxt64bits);
    printf("set pending crosschain xfer NXT.%s %lx %.8f | tp.%p vp.%p xp.%p\n",NXTaddr,(long)NXTtxid,dstr(assetoshis),tp,vp,xp);
    if ( xp == 0 )
    {
        printf("couldn't find matching xp??\n");
        return(-1);
    }
    if ( xp->isinternal != 0 )
        return(0);
    if ( NXTtxid != 0 )
    {
        if ( xp->assetoshis != 0 && xp->assetoshis != assetoshis )
            printf("WARNING: set_pending_assetxfer assetoshis %.8f != %.8f\n",dstr(xp->assetoshis),dstr(assetoshis));
        tp->NXTxferbits = xp->NXT.asset_txid = NXTtxid;
        xp->NXT.confirmed = 1;  // if asset was transferred, must have been confirmed
        xp->assetoshis = assetoshis;
        NXTpending = 1;
        printf("set_pending_assetxfer.(%s NXT.%s %s %.8f)\n",coinid_str(coinid),NXTaddr,nxt64str(xp->NXT.asset_txid),dstr(xp->assetoshis));
    }
    if ( NXTpending != 0 )
    {
        if ( xp->NXT.pending == 0 )  // must do this once and only once
            deposit_transfer_NXTacct(coinid,NXTaddr,xp->value);
        xp->NXT.pending |= NXTpending;
    }
    return(0);
}

// called from different contexts, very real chance that corresponding xp has not been created yet, especially if NXT blockchain faster than coin's blockchain. then again, it could be a historical AM being reprocessed. Both cases must be handled
int32_t decode_depositconfirmed_json(cJSON *argjson,char *AMtxid)
{
    int32_t createdflag,vout,coinid = -1;
    struct replicated_coininfo *lp;
    uint64_t NXTtxid = 0;
    struct coin_txid *tp = 0;
    int64_t assetoshis,value;
    char cointxid[512],coinaddr[512],NXTaddr[64],name[64],NXTtxidstr[64];
    cJSON *cointxidobj,*addrobj,*NXTobj,*nameobj;
    memset(NXTtxidstr,0,sizeof(NXTtxidstr));
    if ( argjson == 0 || (argjson->type&0xff) == cJSON_NULL )
    {
        printf("decode_assetxfer_json: null argjson?\n");
        return(-1);
    }
    NXTobj = cJSON_GetObjectItem(argjson,"NXTaddr");
    nameobj = cJSON_GetObjectItem(argjson,"coin");
    cointxidobj = cJSON_GetObjectItem(argjson,"cointxid");
    addrobj = cJSON_GetObjectItem(argjson,"coinaddr");
    value = get_satoshi_obj(argjson,"value");
    assetoshis = get_satoshi_obj(argjson,"assetoshis");
    vout = (int)get_cJSON_int(argjson,"vout");
    coinid = (int)get_cJSON_int(argjson,"coinid");
    copy_cJSON(name,nameobj);
    copy_cJSON(cointxid,cointxidobj);
    copy_cJSON(coinaddr,addrobj);
    copy_cJSON(NXTaddr,NXTobj);
    lp = get_replicated_coininfo(coinid);
    if ( lp != 0 )
        tp = MTadd_hashtable(&createdflag,&lp->coin_txids,cointxid);
    if ( tp != 0 )
    {
        printf("found tp.%p for cointxid.%s tp->NXTxferbits %ld\n",tp,cointxid,(unsigned long)tp->NXTxferbits);
        if ( AMtxid != 0 && tp->confirmedAMbits == 0 )
        {
            tp->confirmedAMbits = calc_nxt64bits(AMtxid);
            if ( get_daemon_info(coinid) == 0 )
                deposit_NXTacct(coinid,NXTaddr,value);
            printf("set AMtxid for %s %s NXT.%s <- %.8f | cp.%p\n",cointxid,AMtxid,NXTaddr,dstr(value),get_daemon_info(coinid));
        }
        if ( tp->vouts != 0 && tp->vouts[vout] != 0 && tp->vouts[vout]->xp != 0 && (NXTtxid= tp->vouts[vout]->xp->NXT.asset_txid) != 0 )
        {
            expand_nxt64bits(NXTtxidstr,NXTtxid);
            printf("decode_depositconfirmed_json found NXT asset transfer to match cross chain cointxid.%s NXTtxid.%s\n",cointxid,NXTtxidstr);
        }
    } else printf("cant find cointxid.%s\n",cointxid);
    if ( name[0] != 0 && cointxid[0] != 0 && NXTtxid != 0 && coinaddr[0] != 0 && NXTaddr[0] != 0 && value > 0 && assetoshis > 0 )
    {
        if ( strcmp(name,coinid_str(coinid)) != 0 )
            printf("error getting coinid.%d or name.%s vs %s\n",coinid,name,coinid_str(coinid));
        printf("vout.%d coinid.%d %s %s %s %s %s\n",vout,coinid,name,coinaddr,cointxid,NXTaddr,NXTtxidstr);
        if ( set_pending_assetxfer(coinid,coinaddr,cointxid,vout,value,calc_nxt64bits(NXTaddr),NXTtxid,assetoshis) != 0 )
        {
            printf("decode assetxfer error, queue for later processing\n");
            return(coinid);
        }
    } else printf("decode assetxfer error %d %d %d %d %d %ld %ld\n",name[0],cointxid[0],coinaddr[0],NXTaddr[0],NXTtxidstr[0],(long)value,(long)assetoshis);
    return(-1);
}

int32_t authorize_asset_transfer(int32_t coinid,char *assetid,struct crosschain_info *xp,char *NXTaddr)
{
    cJSON *json;
    char *comment;
    int32_t flag = 0;
    char txid[64];
    if ( xp->isinternal == 0 && xp->coin_confirmed != 0 && xp->NXTxfermask != xp->NXT.confirmed
#ifndef DEBUG_MODE
        && mp->RTflag >= xp->NXTRTflag+MIN_NXTCONFIRMS
#endif
        )
    {
        printf("asset_txid.%lx modval.%d %s.%ld %.8f xfermask.%d pending.%d veri.%d\n",(long)xp->NXT.asset_txid ,(int)(xp->nxt64bits % NUM_GATEWAYS),NXTaddr,(long)(xp->nxt64bits % NUM_GATEWAYS),dstr(xp->assetoshis),xp->NXTxfermask,xp->NXT.pending,balance_verification(0,coinid,NXTaddr,xp->value));
        if ( Global_gp->gatewayid >= 0 && (xp->NXTxfermask & 1) != 0 && (xp->NXT.pending & 1) == 0 )
        {
            printf("right before asset xfer: %d %d %d\n",(xp->nxt64bits % NUM_GATEWAYS) == Global_gp->gatewayid,assetid[0] != 0 && xp->assetoshis != 0,balance_verification(0,coinid,NXTaddr,xp->value) > 0 );
            if ( (xp->nxt64bits % NUM_GATEWAYS) == Global_gp->gatewayid &&
                 assetid[0] != 0 && xp->assetoshis != 0 && balance_verification(0,coinid,NXTaddr,xp->value) > 0 )
            {
                if ( xp->parent != 0 && xp->parent->parent != 0 && xp->parent->parent->NXTxferbits == 0 )
                {
                    json = gen_crosschain_json(coinid,xp,NXTaddr);
                    if ( json != 0 )
                    {
                        comment = cJSON_Print(json);
                        printf("gen crosschain got %s %p tp->xfer %ld\n",comment,xp->parent->parent,(unsigned long)xp->parent->parent->NXTxferbits);
                        stripwhite(comment,strlen(comment));
                        //comment = replacequotes(comment);
                    }
                    else comment = 0;
                    xp->NXT.asset_txid = issue_transferAsset(Global_mp->NXTACCTSECRET,NXTaddr,assetid,xp->assetoshis,MIN_NQTFEE,10,comment);
                    xp->parent->parent->NXTxferbits = xp->NXT.asset_txid;
                    printf("%s xfer txid.%ld\n",comment!=0?comment:"",(unsigned long)xp->NXT.asset_txid);
                    if ( xp->NXT.asset_txid != 0 )
                        xp->NXT.pending |= 1, flag |= 1;
                    if ( comment != 0 )
                        free(comment);
                    if ( json != 0 )
                        free_json(json);
               }
            }
        }
        else if ( (xp->NXT.pending & 1) != 0 && (xp->NXT.confirmed & 1) == 0 && xp->NXT.asset_txid != 0 )
        {
            expand_nxt64bits(txid,xp->NXT.asset_txid);
            printf("pending %s confirms.%d\n",txid,get_NXTtxid_confirmations(txid));
            if ( get_NXTtxid_confirmations(txid) >= MIN_NXTCONFIRMS )
                xp->NXT.confirmed |= 1;
        }
    }
    return(flag);
}

void transfer_assets(int32_t coinid)
{
    struct crosschain_info *xp,**confirmed_deposits;
    int32_t i,n;
    char NXTaddr[64];
    confirmed_deposits = get_confirmed_deposits(&n,coinid);
    if ( confirmed_deposits == 0 )
        return;
    for (i=0; i<n; i++)
    {
        xp = confirmed_deposits[i];
        expand_nxt64bits(NXTaddr,xp->nxt64bits);
        printf("isinternal.%d coinconfirmed.%d NXTxfermask.%d pending.%d NXTconfirmed.%d\n",xp->isinternal,xp->coin_confirmed,xp->NXTxfermask,xp->NXT.pending,xp->NXT.confirmed);
        if ( xp->isinternal == 0 )
            authorize_asset_transfer(coinid,assetid_str(coinid),xp,NXTaddr);
    }
    update_unspent_funds(coinid,confirmed_deposits,n);
    free(confirmed_deposits);
}

#endif
