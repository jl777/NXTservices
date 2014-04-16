//
//  multisig.h
//  Created by jl777 March 2014
//  MIT License
//

#ifndef gateway_multisig_h
#define gateway_multisig_h

// begin network functions, each gateway direct links to the other two
struct directnet_getpubkey
{
	struct server_request_header H __attribute__ ((packed));
    int32_t coinid,srcgateway,destgateway,flag __attribute__ ((packed));
    char NXTaddr[MAX_NXTADDR_LEN];
    char pubkey[128],coinaddr[64];
};

int32_t issue_directnet_getpubkey(int32_t destgateway,int32_t funcid,char *argstr,struct directnet_getpubkey *rp,struct daemon_info *cp,int32_t coinid)
{
    struct gateway_info *gp = Global_gp;
    memset(rp,0,sizeof(*rp));
    rp->srcgateway = gp->gatewayid;
    rp->destgateway = destgateway;
    rp->coinid = coinid;
    if ( funcid == MULTIGATEWAY_GETCOINADDR )
        safecopy(rp->NXTaddr,argstr,sizeof(rp->NXTaddr));
    else safecopy(rp->coinaddr,argstr,sizeof(rp->coinaddr));
    if ( server_request(&gp->gensocks[destgateway],gp->gateways[coinid][destgateway],&rp->H,MULTIGATEWAY_VARIANT,funcid) == sizeof(*rp) )
        return(0);
    else return(-1);
}

char *get_coinaddr_pubkey(int32_t gatewayid,char *pubkey,struct daemon_info *cp,int32_t coinid,char *coinaddr)
{
    struct gateway_info *gp = Global_gp;
    char addr[256];
    struct directnet_getpubkey R;
    char *retstr;
    cJSON *json,*pubobj;
    if ( gatewayid == Global_gp->gatewayid )
    {
        sprintf(addr,"\"%s\"",coinaddr);
        retstr = bitcoind_RPC(coinid_str(coinid),cp->numretries,gp->serverport[coinid],gp->userpass[coinid],"validateaddress",addr);
        if ( retstr != 0 )
        {
            json = cJSON_Parse(retstr);
            if ( json != 0 )
            {
                pubobj = cJSON_GetObjectItem(json,"pubkey");
                copy_cJSON(pubkey,pubobj);
                //printf("got.%s get_coinaddr_pubkey (%s)\n",coinid_str(coinid),pubkey);
                free_json(json);
            } else printf("get_coinaddr_pubkey.%s: parse error.(%s)\n",coinid_str(coinid),retstr);
            free(retstr);
            return(pubkey);
        } else printf("%s error issuing validateaddress\n",coinid_str(coinid));
    }
    else
    {
        if ( issue_directnet_getpubkey(gatewayid,MULTIGATEWAY_GETPUBKEY,coinaddr,&R,cp,coinid) == 0 )
        {
            if ( R.flag > 0 && strlen(R.pubkey) > 0 )
            {
                safecopy(pubkey,R.pubkey,sizeof(R.pubkey));
                return(pubkey);
            } else printf("get_coinaddr_pubkey.%s target.%d got R.flag %d %s\n",coinid_str(coinid),gatewayid,R.flag,R.pubkey);
        } else printf("get_coinaddr_pubkey.%s target.%d network error\n",coinid_str(coinid),gatewayid);
    }
    return(0);
}

char *get_coinaddr(int32_t gatewayid,char *coinaddr,struct daemon_info *cp,int32_t coinid,char *NXTaddr)
{
    struct gateway_info *gp = Global_gp;
    char addr[64];
    struct directnet_getpubkey R;
    char *retstr;
    //printf("get_coinaddr.%s for gateway.%d from %d NXT.(%s)\n",coinid_str(coinid),gatewayid,mp->gatewayid,NXTaddr);
    if ( gatewayid == Global_gp->gatewayid )
    {
        sprintf(addr,"\"%s\"",NXTaddr);
        retstr = bitcoind_RPC(coinid_str(coinid),cp->numretries,gp->serverport[coinid],gp->userpass[coinid],"getaccountaddress",addr);
        if ( retstr != 0 )
        {
            safecopy(coinaddr,retstr,sizeof(R.coinaddr));
            //printf("%s got.(%s) for NXT.%s\n",coinid_str(coinid),coinaddr,NXTaddr);
            free(retstr);
            return(coinaddr);
        }
    }
    else
    {
        if ( issue_directnet_getpubkey(gatewayid,MULTIGATEWAY_GETCOINADDR,NXTaddr,&R,cp,coinid) == 0 )
        {
            if ( R.flag > 0 && strlen(R.coinaddr) > 0 )
            {
                safecopy(coinaddr,R.coinaddr,sizeof(R.coinaddr));
                return(coinaddr);
            } else printf("get_coinaddr.%s target.%d got R.flag %d %s\n",coinid_str(coinid),gatewayid,R.flag,R.pubkey);
        } else printf("get_coinaddr.%s target.%d network error\n",coinid_str(coinid),gatewayid);
    }
    return(0);
}

int32_t process_directnet_getpubkey(struct directnet_getpubkey *req,char *clientip)
{
    struct daemon_info *cp;
    struct gateway_info *gp = Global_gp;
    char *retstr = 0;
    req->flag = 0;
    if ( (cp= get_daemon_info(req->coinid)) != 0 )
    {
        printf("process_directnet_getpubkey.%s NXT.%s size.%ld for ip.(%s) funcid.%d %s\n",coinid_str(req->coinid),req->NXTaddr,sizeof(*req),clientip,req->H.funcid,req->coinaddr);
        if ( req->H.funcid == MULTIGATEWAY_GETCOINADDR )
            retstr = get_coinaddr(gp->gatewayid,req->coinaddr,cp,req->coinid,req->NXTaddr);
        else if ( req->H.funcid == MULTIGATEWAY_GETPUBKEY )
            retstr = get_coinaddr_pubkey(gp->gatewayid,req->pubkey,cp,req->coinid,req->coinaddr);
        if ( retstr != 0 )
            req->flag = 1;
        else req->flag = -1;
    }
    return(sizeof(*req));
}
// end of network functions


struct multisig_addr *alloc_multisig_addr(int32_t coinid,int32_t n,char *NXTaddr)
{
    struct multisig_addr *msig;
    msig = calloc(1,sizeof(*msig));
    msig->n = n;
    msig->coinid = coinid;
    safecopy(msig->NXTaddr,NXTaddr,sizeof(msig->NXTaddr));
    if ( n > 0 )
        msig->m = msig->n - 1;
    return(msig);
}

int32_t pubkeycmp(struct pubkey_info *ref,struct pubkey_info *cmp)
{
    if ( strcmp(ref->pubkey,cmp->pubkey) != 0 )
        return(1);
    if ( strcmp(ref->coinaddr,cmp->coinaddr) != 0 )
        return(2);
    if ( strcmp(ref->server,cmp->server) != 0 )
        return(3);
    return(0);
}

int32_t msigcmp(struct multisig_addr *ref,struct multisig_addr *msig)
{
    int32_t i,x;
    if ( ref == 0 )
        return(-1);
    if ( strcmp(ref->multisigaddr,msig->multisigaddr) != 0 )
    {
        printf("A ref.(%s) vs msig.(%s)\n",ref->multisigaddr,msig->multisigaddr);
        return(1);
    }
    if ( strcmp(ref->NXTaddr,msig->NXTaddr) != 0 )
    {
        printf("B ref.(%s) vs msig.(%s)\n",ref->NXTaddr,msig->NXTaddr);
        return(2);
    }
    if ( strcmp(ref->redeemScript,msig->redeemScript) != 0 )
    {
        printf("C ref.(%s) vs msig.(%s)\n",ref->redeemScript,msig->redeemScript);
        return(3);
    }
    for (i=0; i<NUM_GATEWAYS; i++)
        if ( (x= pubkeycmp(&ref->pubkeys[i],&msig->pubkeys[i])) != 0 )
        {
            switch ( x )
            {
            case 1: printf("P.%d pubkey ref.(%s) vs msig.(%s)\n",x,ref->pubkeys[i].pubkey,msig->pubkeys[i].pubkey); break;
            case 2: printf("P.%d pubkey ref.(%s) vs msig.(%s)\n",x,ref->pubkeys[i].coinaddr,msig->pubkeys[i].coinaddr); break;
            case 3: printf("P.%d pubkey ref.(%s) vs msig.(%s)\n",x,ref->pubkeys[i].server,msig->pubkeys[i].server); break;
            default: printf("unexpected retval.%d\n",x);
            }
            return(4+i);
        }
    return(0);
}

void copymsig(struct multisig_addr *dest,struct multisig_addr *src)
{
    int32_t i;
    safecopy(dest->multisigaddr,src->multisigaddr,sizeof(dest->multisigaddr));
    safecopy(dest->NXTaddr,src->NXTaddr,sizeof(dest->NXTaddr));
    safecopy(dest->redeemScript,src->redeemScript,sizeof(dest->redeemScript));
    for (i=0; i<NUM_GATEWAYS; i++)
        dest->pubkeys[i] = src->pubkeys[i];
}

struct multisig_addr *decode_msigjson(char *NXTaddr,cJSON *obj)
{
    int32_t j,m,coinid;
    char nxtstr[512],namestr[64];
    struct multisig_addr *msig = 0;
    cJSON *pobj,*redeemobj,*pubkeysobj,*addrobj,*tmp,*nxtobj,*nameobj;
    coinid = (int)get_cJSON_int(obj,"coinid");
    nameobj = cJSON_GetObjectItem(obj,"coin");
    //printf("coinid.%d cp.%p\n",coinid,cp);
    if ( nameobj != 0 )
    {
        copy_cJSON(namestr,nameobj);
        if ( strcmp(coinid_str(coinid),namestr) != 0 )
        {
            printf("name miscompare %s != %s\n",coinid_str(coinid),namestr);
            return(0);
        }
        msig = alloc_multisig_addr(coinid,NUM_GATEWAYS,NXTaddr);
        addrobj = cJSON_GetObjectItem(obj,"address");
        redeemobj = cJSON_GetObjectItem(obj,"redeemScript");
        pubkeysobj = cJSON_GetObjectItem(obj,"pubkey");
        nxtobj = cJSON_GetObjectItem(obj,"NXTaddr");
        if ( nxtobj != 0 )
        {
            copy_cJSON(nxtstr,nxtobj);
            if ( strcmp(nxtstr,NXTaddr) != 0 )
                printf("WARNING: mismatched NXTaddr.%s vs %s\n",nxtstr,NXTaddr);
        }
        //printf("msig.%p %p %p %p\n",msig,addrobj,redeemobj,pubkeysobj);
        if ( addrobj != 0 && redeemobj != 0 && pubkeysobj != 0 )
        {
            m = cJSON_GetArraySize(pubkeysobj);
            //printf("m.%d\n",m);
            copy_cJSON(msig->redeemScript,redeemobj);
            copy_cJSON(msig->multisigaddr,addrobj);
            //printf("redeem.(%s) addr.(%s)\n",msig->redeemScript,msig->addr);
            if ( m == NUM_GATEWAYS )
            {
                for (j=0; j<m; j++)
                {
                    pobj = cJSON_GetArrayItem(pubkeysobj,j);
                    //printf("j.%d pobj.%p\n",j,pobj);
                    if ( pobj != 0 )
                    {
                        tmp = cJSON_GetObjectItem(pobj,"address"); copy_cJSON(msig->pubkeys[j].coinaddr,tmp);
                        tmp = cJSON_GetObjectItem(pobj,"pubkey"); copy_cJSON(msig->pubkeys[j].pubkey,tmp);
                        tmp = cJSON_GetObjectItem(pobj,"ipaddr"); copy_cJSON(msig->pubkeys[j].server,tmp);
                    } else { free(msig); msig = 0; }
                }
            }
            else
            {
                printf("unexpected number of pubkeys.%d\n",m);
                free(msig);
                return(0);
            }
        } else { printf("%p %p %p\n",addrobj,redeemobj,pubkeysobj); free(msig); msig = 0; }
        return(msig);
    }
    printf("decode msig:  error parsing.(%s)\n",cJSON_Print(obj));
    return(0);
}

struct multisig_addr *gen_multisig_addr(struct daemon_info *cp,int32_t coinid,char *NXTaddr,char pubkeys[NUM_GATEWAYS][128],char coinaddrs[NUM_GATEWAYS][64])
{
    struct gateway_info *gp = Global_gp;
    int32_t i,flag = 0;
    cJSON *json,*msigobj,*redeemobj;
    struct multisig_addr *msig;
    char *params,*retstr = 0;
    if ( gp->gatewayid < 0 ) return(0);
    msig = alloc_multisig_addr(coinid,NUM_GATEWAYS,NXTaddr);
    for (i=0; i<NUM_GATEWAYS; i++)
    {
        safecopy(msig->pubkeys[i].pubkey,pubkeys[i],sizeof(msig->pubkeys[i].pubkey));
        safecopy(msig->pubkeys[i].coinaddr,coinaddrs[i],sizeof(msig->pubkeys[i].coinaddr));
        safecopy(msig->pubkeys[i].server,gp->gateways[coinid][i],sizeof(msig->pubkeys[i].server));
    }
    params = createmultisig_json_params(msig);
    if ( params != 0 )
    {
        // printf("multisig params.(%s)\n",params);
        retstr = bitcoind_RPC(coinid_str(coinid),cp->numretries,gp->serverport[coinid],gp->userpass[coinid],"createmultisig",params);
        if ( retstr != 0 )
        {
            json = cJSON_Parse(retstr);
            if ( json == 0 ) printf("Error before: [%s]\n",cJSON_GetErrorPtr());
            else
            {
                if ( (msigobj= cJSON_GetObjectItem(json,"address")) != 0 )
                {
                    if ( (redeemobj= cJSON_GetObjectItem(json,"redeemScript")) != 0 )
                    {
                        copy_cJSON(msig->multisigaddr,msigobj);
                        copy_cJSON(msig->redeemScript,redeemobj);
                        flag = 1;
                        
                    } else printf("missing redeemScript in (%s)\n",retstr);
                } else printf("multisig missing address in (%s) params.(%s)\n",retstr,params);
                printf("addmultisig.(%s)\n",retstr);
                free(retstr);
                free_json(json);
            }
        } else printf("error issuing createmultisig.(%s)\n",params);
        free(params);
    } else printf("error generating msig params\n");
    if ( flag == 0 )
    {
        free(msig);
        return(0);
    }
    return(msig);
}

struct multisig_addr *gen_depositaddr(struct daemon_info *cp,int32_t coinid,char *NXTaddr)//,struct coin_reqaddr *tp)
{
    int32_t gatewayid;
    char coinaddrs[NUM_GATEWAYS][64],pubkeys[NUM_GATEWAYS][128];
    memset(coinaddrs,0,sizeof(coinaddrs));
    memset(pubkeys,0,sizeof(pubkeys));
    if ( Global_gp->gatewayid < 0 ) return(0);
    //printf("gen_depositaddr.%s txid.%s\n",NXTaddr,tp->NXTtxid);
    for (gatewayid=0; gatewayid<NUM_GATEWAYS; gatewayid++)
    {
        // printf("gen_depositaddr: contact gateway.%d\n",gatewayid);
        if ( get_coinaddr(gatewayid,coinaddrs[gatewayid],cp,coinid,NXTaddr) == 0 )
        {
            //queue_enqueue(&cp->genaddr_errors,tp);
            printf("error getting coinaddr for %s from gateway.%d\n",NXTaddr,gatewayid);
            break;
        }
        else if ( get_coinaddr_pubkey(gatewayid,pubkeys[gatewayid],cp,coinid,coinaddrs[gatewayid]) == 0 )
        {
            //queue_enqueue(&cp->genaddr_errors,tp);
            printf("error getting pubkey for %s %s from gateway.%d\n",NXTaddr,coinaddrs[gatewayid],gatewayid);
            break;
        }
    }
    if ( gatewayid == NUM_GATEWAYS )
        return(gen_multisig_addr(cp,coinid,NXTaddr,pubkeys,coinaddrs));
    printf("ERROR creating multisig addr for %s\n",NXTaddr);
    return(0);
}

int32_t update_acct_binding(int32_t coinid,char *NXTaddr,struct multisig_addr *msig)
{
    int32_t i;
    struct coin_acct *acct;
    printf("update_acct_binding selector NXT.%s %s msig.%p %p\n",NXTaddr,coinid_str(coinid),msig,get_deposit_addr(coinid,NXTaddr));
    acct = get_coin_acct(coinid,NXTaddr);
    if ( acct != 0 )
    {
        //pthread_mutex_lock(&cp->msigs_mutex);
        for (i=0; i<acct->nummsigs; i++)
        {
            printf("cmp %p %s.(%s) vs %p (%s)\n",acct->msigs[i],coinid_str(acct->msigs[i]->coinid),acct->msigs[i]->multisigaddr,msig,msig->multisigaddr);
            if ( msigcmp(acct->msigs[i],msig) == 0 )
                break;
        }
        if ( i == acct->nummsigs )
        {
            add_multisig_addr(coinid,acct,msig);
            printf("added: %s num.%d %p\n",get_deposit_addr(coinid,NXTaddr),acct->nummsigs,acct->msigs!=0?acct->msigs[0]:0);
        }
        else
        {
            printf("new msig identical to old msig.%d of %d\n",i,acct->nummsigs);
            return(0);
        }
        ///pthread_mutex_unlock(&cp->msigs_mutex);
    } else printf("%s cant create coin acct\n",NXTaddr);
    return(1);
}

#endif
