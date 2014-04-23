//
//  multigateway.h
//  Created by jl777, Mar/April 2014
//  MIT License
//

#ifndef gateway_directnet_h
#define gateway_directnet_h

#include "multidefines.h"
#include "multistructs.h"
int SUPPRESS_MULTIGATEWAY = 0;
struct gateway_info *Global_gp;
typedef char *(*json_handler)(char *NXTaddr,int32_t valid,cJSON **objs,int32_t numobjs);
struct crosschain_info *update_crosschain_info(int32_t coinid,struct coin_txid *tp,struct coin_value *vp,int32_t isconfirmed,struct crosschain_info *xp,uint64_t value,char *coinaddr);
int32_t sign_rawtransaction(char *deststr,unsigned long destsize,struct daemon_info *cp,int32_t coinid,struct rawtransaction *rp,char *rawbytes);
//int32_t set_pending_assetxfer(int32_t coinid,char *coinaddr,char *cointxid,int32_t vout,int64_t value,uint64_t nxt64bits,char *AMtxid,uint64_t NXTtxid,int64_t assetoshis);
int32_t xp_vout(struct crosschain_info *xp) { if ( xp->parent != 0 ) return(xp->parent->parent_vout); return(-1); }
char *xp_coinaddr(struct crosschain_info *xp) { if ( xp->parent != 0 ) return(xp->parent->coinaddr); return("<no addr>"); }
char *xp_txid(struct crosschain_info *xp) { if ( xp->parent != 0 && xp->parent->parent != 0 ) return(xp->parent->parent->txid); return("<no txid>"); }
char *xp_NXTaddr(struct crosschain_info *xp) { if ( xp->msig != 0 ) return(xp->msig->NXTaddr); return("<no NXTaddr>"); }
char *createrawtxid_json_params(char **localcoinaddrs,int32_t coinid,struct rawtransaction *rp);

#include "multifind.h"
#include "genjson.h"
#include "balances.h"
#include "multisig.h"
#include "bitcoin.h"
#include "daemon.h"
#include "gateway.h"
#include "multiwithdraw.h"
#include "multideposit.h"

int32_t is_gateway_addr(char *addr)
{
    if ( strcmp(addr,NXTISSUERACCT) == 0 || strcmp(addr,NXTACCTA) == 0 || strcmp(addr,NXTACCTB) == 0 || strcmp(addr,NXTACCTC) == 0 )
        return(1);
    return(0);
}

char *AM_get_coindeposit_address(int32_t sig,int32_t timestamp,char *nxtaddr,char *coinjsontxt)
{
    char AM[4096];
    struct json_AM *ap = (struct json_AM *)AM;
    printf("get deposit_addrs(%s)\n",coinjsontxt);
    set_json_AM(ap,sig,GET_COINDEPOSIT_ADDRESS,nxtaddr,timestamp,coinjsontxt,1);
    return(submit_AM(NXTISSUERACCT,&ap->H,0));
}

int32_t retrydepositconfirmed(void **ptrp,struct replicated_coininfo *lp)
{
    cJSON *argjson = *ptrp;
    if ( decode_depositconfirmed_json(argjson,0) != 0 )
        return(0);
    //printf("completed retryassetxfer (%s) AMtxid.%s\n",cJSON_Print(argjson),AMtxid);
    free_json(argjson);
    *ptrp = 0;
    return(1);
}

//int32_t
void *Coinloop(void *ptr)
{
    int32_t i,coinid,flag,counter = 0;
    struct daemon_info *cp;
    struct gateway_info *gp = ptr;
    struct replicated_coininfo *lp;
    int64_t height,NXTheight;
    while ( Historical_done == 0 ) // must process all historical msig addresses and asset transfers
    {
        sleep(1);
        continue;
    }
    flag = 1;
    while ( flag != 0 )
    {
        for (coinid=flag=0; coinid<64; coinid++)
        {
            cp = get_daemon_info(coinid);
            if ( cp == 0 )
                continue;
            lp = get_replicated_coininfo(coinid);
            if ( lp != 0 )
            {
                process_pingpong_queue(&lp->genreqs,cp);            // create unique genaddr requests
                process_pingpong_queue(&lp->genaddr,cp);            // gen multisig addrs for accounts without one
                flag += queue_size(&lp->genreqs.pingpong[0]) + queue_size(&lp->genreqs.pingpong[0]);
                flag += queue_size(&lp->genaddr.pingpong[0]) + queue_size(&lp->genaddr.pingpong[0]);
            }
        }
    }
    printf("Start coinloop\n");
    while ( 1 )
    {
        for (coinid=0; coinid<64; coinid++)
        {
#ifdef DEBUG_MODE
            if ( coinid != DOGE_COINID )
                continue;
#endif
            cp = get_daemon_info(coinid);
            lp = get_replicated_coininfo(coinid);
            if ( lp == 0 )
                continue;
            if ( gp->gatewayid >= 0 && cp != 0 ) //NXTheight > lp->NXTheight &&
            {
                process_pingpong_queue(&lp->genreqs,cp);            // create unique genaddr requests
                process_pingpong_queue(&lp->genaddr,cp);            // gen multisig addrs for accounts without one
            }
            if ( cp == 0 )
                gp->initdone[coinid] = 1;
            if ( gp->initdone[coinid] == 0 && cp != 0 )
            {
                height = (*cp->get_blockheight)(cp,coinid);
                if ( gp->blockheight[coinid] <= (height - gp->min_confirms[coinid] - 1) )
                {
                    //printf("historical process %s.%ld\n",coinid_str(coinid),(long)cp->blockheight);
                    if ( (*cp->add_unique_txids)(cp,coinid,gp->blockheight[coinid],0) == 0 )
                    {
                        counter++;
                        gp->blockheight[coinid]++;
                    }
                    continue;
                }
                gp->initdone[coinid] = 1;
                printf("Start RTcoinloop.%s cp->height %ld vs latest %ld\n",coinid_str(coinid),(long)gp->blockheight[coinid],(long)(*cp->get_blockheight)(cp,coinid));
                for (i=0; i<gp->numinitlist[coinid]; i++)
                    (*cp->add_unique_txids)(cp,coinid,gp->initlist[coinid][i],0);
            }
            NXTheight = Global_mp->NXTheight;
            if ( NXTheight > lp->NXTheight )
            {
                //struct coin_acct *acct = get_coin_acct(DOGE_COINID,"8989816935121514892");
                //if ( acct != 0 )
                //    printf("acct.%p withdrawaddr.(%s)\n",acct,acct->withdrawaddr);
                transfer_assets(coinid);
            }
            if ( cp == 0 || gp->blockheight[coinid] <= (height = (*cp->get_blockheight)(cp,coinid)) )
            {
#ifndef DEBUG_MODE
                if ( NXTnetwork_healthy(Global_mp) > 0 )
#endif
                {
                    recalc_balance_info("multigateway",coinid_str(coinid),get_balance_info(coinid));
                    if ( cp != 0 )
                    printf("iter RTcoinloop.%s cp->height %ld vs latest %ld | totaldeposits %.8f unspent %.8f\n",coinid_str(coinid),(long)gp->blockheight[coinid],(long)height,dstr(get_unspent_info(coinid)->maxunspent),dstr(get_unspent_info(coinid)->unspent));
                    if ( cp != 0 && (*cp->add_unique_txids)(cp,coinid,gp->blockheight[coinid],1) == 0 ) // update universe of possible txids
                        flag = 1;
                    else flag = 0;
                    process_pingpong_queue(&lp->retrydepositconfirmed,lp);   // need to wait until add_unique_txids creates xp
                    //printf("NXTheight.%ld vs cp-> %ld\n",(long)NXTheight,(long)cp->NXTheight);
                    if ( cp == 0 || gp->blockheight[coinid] == height )
                    {
                        if ( cp != 0 )
                            transfer_assets(coinid);
                        process_pingpong_queue(&lp->withdrawreqs,lp);   // just wait for MIN_NXTCONFIRMS
                        process_pingpong_queue(&lp->withdraws,lp);      // get consensus and submit to google_authenticator
                        process_pingpong_queue(&lp->authenticate,lp);   // if 2FA enabled, do google_authenticator
                        process_pingpong_queue(&lp->sendcoin,lp);       // submit rawtransaction and publish AM
                    }
                    gp->blockheight[coinid] += flag;
                }
            }
            lp->NXTheight = NXTheight;
            if ( cp == 0 )
                sleep(10);
        }
    }
    return(0);
    //return(counter);
}

void init_coin_tables(struct daemon_info *cp,int32_t coinid,char *serverA,char *serverB,char *serverC)
{
    struct gateway_info *gp = Global_gp;
    struct replicated_coininfo *lp = get_replicated_coininfo(coinid);
    if ( lp == 0 )
        return;
    safecopy(gp->gateways[coinid][0],serverA==0?SERVER_NAMEA:serverA,sizeof(gp->gateways[coinid][0]));
    safecopy(gp->gateways[coinid][1],serverB==0?SERVER_NAMEB:serverB,sizeof(gp->gateways[coinid][1]));
    safecopy(gp->gateways[coinid][2],serverC==0?SERVER_NAMEC:serverC,sizeof(gp->gateways[coinid][2]));
    init_pingpong_queue(&lp->genreqs,"gen depositaddr requests",filter_unique_reqtxids,&lp->genaddr.pingpong[0],&lp->errorqueue);
    init_pingpong_queue(&lp->genaddr,"call_gen_depositaddr",call_gen_depositaddr,0,&lp->genaddr.pingpong[0]);
    
    init_pingpong_queue(&lp->retrydepositconfirmed,"retrydepositconfirmed",retrydepositconfirmed,0,0);
    
    init_pingpong_queue(&lp->withdrawreqs,"wait_for_NXTconfirms",wait_for_NXTconfirms,&lp->withdraws.pingpong[0],0);
    init_pingpong_queue(&lp->withdraws,"process_withdraws",process_withdraws,&lp->authenticate.pingpong[0],0);
    init_pingpong_queue(&lp->authenticate,"google authenticator",google_authenticator,&lp->sendcoin.pingpong[0],0);
    init_pingpong_queue(&lp->sendcoin,"send coin",sendcoin,0,0);
    
    if ( gp->userpass[coinid] != 0 && gp->userpass[coinid][0] != 0 )
    {
        cp->validate_coinaddr = validate_coinaddr;
        cp->get_blockheight = get_blockheight;
        cp->add_unique_txids = add_bitcoind_uniquetxids;
        cp->calc_rawtransaction = calc_rawtransaction;
        
        cp->gen_depositaddr = gen_depositaddr;
        cp->submit_withdraw = submit_withdraw;
    }
}

int32_t init_multigateway(struct NXThandler_info *mp,struct gateway_info *gp)
{
    struct req_deposit *rp = 0;
    struct withdraw_info *wp = 0;
    struct coin_txid *tp = 0;
    int32_t i,coinid;
    struct replicated_coininfo *lp;
    static char *whitelist[NUM_GATEWAYS+1];
    struct daemon_info *cp;
    char *ipaddr,*coins[] = { "DOGE", "BTC", "LTC", ""};//"DRK", "" }; //"PPC","CGB",
    static int64_t variant;
    printf("call init_multigateway gatewayid.%d\n",gp->gatewayid);
    ipaddr = get_ipaddr();
    printf("IPaddr.(%s)\n",ipaddr);
    // need a bit of work to allow random selection of 3 servers out of a larger set, for now just harded coded to three servers
    for (i=0; coins[i][0]!=0; i++)
    {
        coinid = conv_coinstr(coins[i]);
        if ( coinid >= 0 )
        {
            cp = init_daemon_info(Global_gp,coins[i],0,0,0);
            printf("coinid.%d %s cp.%p\n",coinid,coins[i],cp);
            if ( cp != 0 )
            {
                cp->coinid = coinid;
                init_coin_tables(cp,coinid,Server_names[0],Server_names[1],Server_names[2]);
                gp->daemons[coinid] = cp;
                printf("Start %s height %ld\n",coinid_str(coinid),(long)(*cp->get_blockheight)(cp,coinid));
            }
            lp = get_replicated_coininfo(coinid);
            if ( lp == 0 )
                continue;
            lp->genaddr_txids = hashtable_create("genaddr_txids",HASHTABLES_STARTSIZE,sizeof(*rp),((long)&rp->NXTaddr[0] - (long)rp),sizeof(rp->NXTaddr),((long)&rp->modified - (long)rp));
            lp->coin_txids = hashtable_create("coin_txids",HASHTABLES_STARTSIZE,sizeof(*tp),((long)&tp->txid[0] - (long)tp),sizeof(tp->txid),((long)&tp->modified - (long)tp));
            lp->redeemtxids = hashtable_create("redeemtxids",HASHTABLES_STARTSIZE,sizeof(*wp),((long)&wp->redeemtxid[0] - (long)wp),sizeof(wp->redeemtxid),((long)&wp->modified - (long)wp));
            //gp->coin_values = hashtable_create("coin_values",HASHTABLES_STARTSIZE,sizeof(*vp),((long)&vp->coinaddr[0] - (long)vp),sizeof(vp->coinaddr),((long)&vp->modified - (long)vp));
        }
    }
    if ( gp->gatewayid >= 0 )
    {
        for (i=0; i<NUM_GATEWAYS; i++)
        {
            gp->wdsocks[i] = -1;
            gp->gensocks[i] = -1;
            whitelist[i] = Server_names[i];
        }
        whitelist[i] = 0;
        variant = MULTIGATEWAY_VARIANT;
        register_variant_handler(MULTIGATEWAY_VARIANT,process_directnet_syncwithdraw,MULTIGATEWAY_SYNCWITHDRAW,sizeof(struct withdraw_info),sizeof(struct withdraw_info),whitelist);
        register_variant_handler(MULTIGATEWAY_VARIANT,process_directnet_getpubkey,MULTIGATEWAY_GETCOINADDR,sizeof(struct directnet_getpubkey),sizeof(struct directnet_getpubkey),whitelist);
        register_variant_handler(MULTIGATEWAY_VARIANT,process_directnet_getpubkey,MULTIGATEWAY_GETPUBKEY,sizeof(struct directnet_getpubkey),sizeof(struct directnet_getpubkey),whitelist);
        if ( pthread_create(malloc(sizeof(pthread_t)),NULL,_server_loop,&variant) != 0 )
            printf("ERROR _server_loop\n");
    }
    if ( SUPPRESS_MULTIGATEWAY == 0 && pthread_create(malloc(sizeof(pthread_t)),NULL,Coinloop,gp) != 0 )
        printf("ERROR Coin_genaddrloop\n");
    return(1);
}

char *dispNXTacct_func(char *sender,int32_t valid,cJSON **objs,int32_t numobjs)
{
    int32_t created,coinid;
    struct NXT_acct *np;
    struct coin_acct *acct;
    char coinname[64],NXTaddr[64],assetid[64];
    //printf("dispNXTacct_func.(%s) numobjs.%d\n",sender,numobjs);
    copy_cJSON(NXTaddr,objs[0]);
    if ( NXTaddr[0] != 0 )
    {
        copy_cJSON(coinname,objs[1]);
        copy_cJSON(assetid,objs[2]);
        if ( coinname[0] == 0 )
        {
            np = get_NXTacct(&created,Global_mp,NXTaddr);
            if ( np != 0 )
                return(cJSON_Print(gen_NXTacct_json(np,0,assetid[0]==0?0:assetid)));
        }
        else
        {
            coinid = conv_coinstr(coinname);
            if ( coinid >= 0 )
            {
                acct = get_coin_acct(coinid,NXTaddr);
                if ( acct != 0 )
                    return(cJSON_Print(gen_coinacct_json(acct,NXTaddr)));
                else printf("couldnt get account for NXT.%s %s coinid.%d\n",NXTaddr,coinname,coinid);
            } else printf("dispNXTacct_func (%s) -> %d\n",coinname,coinid);
        }
    }
    return(0);
}


char *dispcoininfo_func(char *sender,int32_t valid,cJSON **objs,int32_t numobjs)
{
    int32_t created,coinid;
    struct NXT_acct *np;
    struct coin_acct *acct;
    char coinname[64],NXTaddr[64],nxtaddr[64];
    //printf("dispNXTacct_func.(%s) numobjs.%d\n",sender,numobjs);
    copy_cJSON(NXTaddr,objs[0]);
    if ( NXTaddr[0] != 0 )
    {
        copy_cJSON(coinname,objs[1]);
        copy_cJSON(nxtaddr,objs[2]);
        if ( coinname[0] == 0 )
        {
            np = get_NXTacct(&created,Global_mp,NXTaddr);
            if ( np != 0 )
                return(cJSON_Print(gen_NXTacct_json(np,0,0)));
        }
        else
        {
            coinid = conv_coinstr(coinname);
            if ( coinid >= 0 )
            {
                acct = get_coin_acct(coinid,NXTaddr);
                if ( acct != 0 )
                    return(cJSON_Print(gen_coinacct_json(acct,nxtaddr)));
                else printf("couldnt get account for NXT.%s %s coinid.%d\n",nxtaddr,coinname,coinid);
            } else printf("dispNXTacct_func (%s) -> %d\n",coinname,coinid);
        }
    }
    return(0);
}

char *genDepositaddrs_func(char *sender,int32_t valid,cJSON **objs,int32_t numobjs)
{
    cJSON *json;
    char coinsjsontxt[64],NXTaddr[64],*retstr = 0;
    printf("sender.%s valid.%d\n",sender,valid);
    copy_cJSON(NXTaddr,objs[0]);
    copy_cJSON(coinsjsontxt,objs[1]);
    if ( NXTaddr[0] != 0 )
    {
        retstr = AM_get_coindeposit_address(GATEWAY_SIG,Global_mp->timestamp,NXTaddr,coinsjsontxt);
        if ( retstr != 0 )
        {
            json = cJSON_CreateObject();
            cJSON_AddItemToObject(json,"result",cJSON_CreateString("good"));
            cJSON_AddItemToObject(json,"AMtxid",cJSON_CreateString(retstr));
            free(retstr);
            retstr = cJSON_Print(json);
            free_json(json);
        }
    }
    return(retstr);
}

char *redeem_func(char *sender,int32_t valid,cJSON **objs,int32_t numobjs)
{
//http://127.0.0.1:7777/multigateway?[{"requestType":"redeem","NXT":"8989816935121514892","coin":"DOGE","amount":"1.123","comment":{"redeem":"DOGE"}},{"token":"d79fth95u83cu087m4js5pr7oqktvg537dctmf7sclmv3thqn2q7sbpgasdbego0a31ue2g4tunv5v3sr66fmdrhd1rptnc7qtl38bplvdug1udpmkibjhqp7kb2vhd6svvar4fdioum4otoo2i9jhj97fboibne"}]
    cJSON *json;
    int32_t coinid,deadline = 666;
    uint64_t redeemtxid,amount;
    char coinname[64],NXTaddr[64],numstr[512],comment[1024],cmd[4096],*str,*retstr = 0;
    copy_cJSON(NXTaddr,objs[0]);
    copy_cJSON(coinname,objs[1]);
    copy_cJSON(numstr,objs[2]);
    copy_cJSON(comment,objs[3]);
    stripwhite(comment,strlen(comment));
    coinid = conv_coinstr(coinname);
    amount = (uint64_t)(atof(numstr) * SATOSHIDEN);
    str = assetid_str(coinid);
    printf("sender.%s valid.%d: NXT.%s %d.(%s) %s (%s)=%.8f (%s)\n",sender,valid,NXTaddr,coinid,coinname,str,numstr,dstr(amount),comment);
    if ( NXTaddr[0] != 0 && coinid >= 0 && amount > 0 )
    {
        sprintf(cmd,"%s=transferAsset&secretPhrase=%s&recipient=%s&asset=%s&quantityQNT=%ld&feeNQT=%ld&deadline=%d&comment=%s",NXTSERVER,Global_mp->NXTACCTSECRET,NXTISSUERACCT,str,(long)amount,(long)MIN_NQTFEE,deadline,comment);
        retstr = issue_curl(cmd);
        if ( retstr != 0 )
        {
            json = cJSON_Parse(retstr);
            if ( json != 0 )
            {
                redeemtxid = get_satoshi_obj(json,"transaction");
                if ( redeemtxid == 0 )
                {
                    str = cJSON_Print(json);
                    printf("ERROR WITH ASSET TRANSFER.(%s) -> \n%s\n",cmd,str);
                    free(str);
                }
                else printf("got txid.%ld %s\n",(unsigned long)redeemtxid,nxt64str(redeemtxid));
                free_json(json);
            } else printf("error issuing asset.(%s) -> %s\n",cmd,retstr);
        }
        else printf("error redeeming transfer\n");
    }
    return(retstr);
}

char *multigateway_json_commands(struct NXThandler_info *mp,struct gateway_info *gp,cJSON *argjson,char *sender,int32_t valid)
{
    //http://127.0.0.1:7777/multigateway?[{%22requestType%22:%22genDepositaddrs%22,%22NXT%22:%2211445347041779652448%22,%22coins%22:{%22DOGE%22:%22DNyEii2mnvVL1BM1kTZkmKDcf8SFyqAX4Z%22}},{%22token%22:%22bd855r204lqfskfg4que97uef8sh2blnh7ol9mcq2mmee1i2lu3a2ftnpqudlto04kqa15gpc14hu6c1dvs1p0vfjbkha5q423lqo8fbf5k0ketomirtkvveefqib5r03bl2pk2i3ju48dl7k4bpcqfehppch81h%22}]
    static char *changeurl[] = { (char *)changeurl_func, "changeurl", "", "URL", 0 };
    static char *genDepositaddrs[] = { (char *)genDepositaddrs_func, "genDepositaddrs", "V", "NXT", "coins", 0 };
    static char *dispNXTacct[] = { (char *)dispNXTacct_func, "dispNXTacct", "", "NXT", "coin", "assetid", 0 };
    static char *dispcoininfo[] = { (char *)dispNXTacct_func, "dispcoininfo", "", "NXT", "coin", "nxtaddr", 0 };
    static char *redeem[] = { (char *)redeem_func, "redeem", "V", "NXT", "coin", "amount", "comment", 0 };
    static char **commands[] = { dispNXTacct, genDepositaddrs, dispcoininfo, redeem, changeurl };
    int32_t i,j;
    cJSON *obj,*nxtobj,*objs[8];
    char NXTaddr[64],command[4096],**cmdinfo;
    memset(objs,0,sizeof(objs));
    command[0] = 0;
    memset(NXTaddr,0,sizeof(NXTaddr));
    if ( argjson != 0 )
    {
        obj = cJSON_GetObjectItem(argjson,"requestType");
        nxtobj = cJSON_GetObjectItem(argjson,"NXT");
        copy_cJSON(NXTaddr,nxtobj);
        copy_cJSON(command,obj);
        //printf("(%s) command.(%s) NXT.(%s)\n",cJSON_Print(argjson),command,NXTaddr);
    }
    //printf("multigateway_json_commands sender.(%s) valid.%d\n",sender,valid);
    for (i=0; i<(int32_t)(sizeof(commands)/sizeof(*commands)); i++)
    {
        cmdinfo = commands[i];
        printf("needvalid.(%c) sender.(%s) valid.%d %d of %d: cmd.(%s) vs command.(%s)\n",cmdinfo[2][0],sender,valid,i,(int32_t)(sizeof(commands)/sizeof(*commands)),cmdinfo[1],command);
        if ( strcmp(cmdinfo[1],command) == 0 )
        {
            if ( cmdinfo[2][0] != 0 )
            {
                if ( sender[0] == 0 || valid != 1 || strcmp(NXTaddr,sender) != 0 )
                {
                    printf("verification valid.%d missing for %s sender.(%s) vs NXT.(%s)\n",valid,cmdinfo[1],NXTaddr,sender);
                    return(0);
                }
            }
            for (j=3; cmdinfo[j]!=0&&j<3+(int32_t)(sizeof(objs)/sizeof(*objs)); j++)
                objs[j-3] = cJSON_GetObjectItem(argjson,cmdinfo[j]);
            return((*(json_handler)cmdinfo[0])(sender,valid,objs,j-3));
        }
    }
    return(0);
}

char *multigateway_jsonhandler(cJSON *argjson)
{
    struct NXThandler_info *mp = Global_mp;
    struct gateway_info *gp = Global_gp;
    long len;
    int32_t valid;
    cJSON *json,*obj,*tokenobj,*secondobj,*parmsobj = 0;
    char sender[64],*parmstxt=0,encoded[NXT_TOKEN_LEN+1],*retstr = 0;
    sender[0] = 0;
    valid = -1;
   // printf("multigateway_jsonhandler argjson.%p\n",argjson);
    if ( argjson != 0 )
    {
        parmstxt = cJSON_Print(argjson);
        len = strlen(parmstxt);
        stripwhite(parmstxt,len);
    }
    if ( argjson == 0 )
    {
        json = cJSON_CreateObject();
        obj = cJSON_CreateNumber(gp->gatewayid); cJSON_AddItemToObject(json,"gatewayid",obj);
        obj = cJSON_CreateString(mp->NXTADDR); cJSON_AddItemToObject(json,"NXTaddr",obj);
        obj = cJSON_CreateString(mp->ipaddr); cJSON_AddItemToObject(json,"ipaddr",obj);
        obj = gen_NXTaccts_json(0);
        if ( obj != 0 )
            cJSON_AddItemToObject(json,"NXTaccts",obj);
        obj = gen_coins_json(0);
        if ( obj != 0 )
            cJSON_AddItemToObject(json,"coins",obj);
        retstr = cJSON_Print(json);
        free_json(json);
        return(retstr);
    }
    else if ( (argjson->type&0xff) == cJSON_Array && cJSON_GetArraySize(argjson) == 2 )
    {
        parmsobj = cJSON_GetArrayItem(argjson,0);
        if ( parmstxt != 0 )
            free(parmstxt);
        parmstxt = cJSON_Print(parmsobj);
        len = strlen(parmstxt);
        stripwhite(parmstxt,len);

        secondobj = cJSON_GetArrayItem(argjson,1);
        tokenobj = cJSON_GetObjectItem(secondobj,"token");
        copy_cJSON(encoded,tokenobj);
        //printf("website.(%s) encoded.(%s) len.%ld\n",parmstxt,encoded,strlen(encoded));
        if ( strlen(encoded) == NXT_TOKEN_LEN )
            issue_decodeToken(sender,&valid,parmstxt,encoded);
        argjson = parmsobj;
    }
    retstr = multigateway_json_commands(mp,gp,argjson,sender,valid);
#ifdef DEBUG_MODE
    if ( retstr == 0 && argjson != 0 && argjson != parmsobj )
    {
        char buf[1024];
        //printf("issue token.(%s)\n",parmstxt);
        issue_generateToken(encoded,parmstxt,mp->NXTACCTSECRET);
        encoded[NXT_TOKEN_LEN] = 0;
        sprintf(buf,"[%s,{\"token\":\"%s\"}]",parmstxt,encoded);
        //stripwhite(buf,strlen(buf));
        retstr = clonestr(buf);
        printf("%s\n",retstr);
    }
#endif
    if ( parmstxt != 0 )
        free(parmstxt);
    return(retstr);
}

void multigateway_assetevent(char *sender,char *receiver,char *txid,char *assetid,int64_t assetoshis,char *comment)
{
    struct replicated_coininfo *lp;
    struct daemon_info *cp;
    struct coin_txid *tp;
    struct crosschain_info *xp;
    int32_t vout,createdflag,coinid,authenticatorflag = 1;
    char withdrawaddr[512],coinname[16],cointxid[64];
    cJSON *json=0,*obj;
    struct coin_acct *acct;
    struct withdraw_info *wp;
    memset(withdrawaddr,0,sizeof(withdrawaddr));
    if ( comment == 0 )
        comment = "";
    else replace_backslashquotes(comment);
    coinid = conv_assetid(assetid);
    lp = get_replicated_coininfo(coinid);
    
    if ( lp != 0 )
    {
        printf("assetcommand NXT.%s -> %s | %s %s -> (%s) assetid.%s %.8f (%s)\n",sender,receiver,assetid,txid,nxt64str(calc_nxt64bits(txid)),assetid,dstr(assetoshis),comment);
        json = cJSON_Parse(comment);
        if ( json != 0 )
        {
           // printf("assetcommand NXT.%s -> %s | %s %s -> (%s) assetid.%s %.8f coinid.%d (%s) -> (%s)\n",sender,receiver,assetid,txid,nxt64str(calc_nxt64bits(txid)),assetid,dstr(assetoshis),coinid,comment,cJSON_Print(json));
            obj = cJSON_GetObjectItem(json,"crosschain");
            if ( obj != 0 )
            {
                copy_cJSON(coinname,obj);
                if ( conv_coinstr(coinname) == coinid )
                {
                    vout = (int)get_cJSON_int(json,"vout");
                    obj = cJSON_GetObjectItem(json,"cointxid");
                    if ( assetoshis != get_cJSON_int(json,"assetoshis") )
                        printf("assetevent warning: assetoshis %ld != %ld json.assetoshis\n",(long)assetoshis,(long)get_cJSON_int(json,"assetoshis"));
                    copy_cJSON(cointxid,obj);
                    if ( cointxid[0] != 0 )
                    {
                        tp = MTadd_hashtable(&createdflag,&lp->coin_txids,cointxid);
                        if ( tp != 0 )
                        {
                            tp->NXTxferbits = calc_nxt64bits(txid);
                            printf("set %p NXTxferbits %ld %s\n",tp,(unsigned long)tp->NXTxferbits,txid);
                            if ( vout >= 0 && vout < tp->numvouts && tp->vouts[vout] != 0 && (xp= tp->vouts[vout]->xp) != 0 )
                            {
                                if ( xp->NXT.asset_txid == 0 )
                                {
                                    xp->NXT.asset_txid = calc_nxt64bits(txid);
                                    printf("link asset transfer to crosschain cointxid.%s NXTtxid.%s\n",cointxid,txid);
                                }
                                else
                                    printf("????? unexpected duplicate assettransfer for cointxid.%s NXTtxid.%s\n",cointxid,txid);
                            }
                            else if ( get_daemon_info(coinid) == 0 )
                                deposit_transfer_NXTacct(coinid,sender,assetoshis);
                        }
                    } else printf("bad format asset transfer comment (%s)\n",comment);
                }
                else printf("invalid coinname.%s for coinid.%d from asset.%s\n",coinname,coinid,assetid);
            }
            else
            {
                if ( strcmp(sender,NXTISSUERACCT) == 0 || strcmp(sender,GENESISACCT) == 0 || is_gateway_addr(sender) != 0 )
                    return;
                obj = cJSON_GetObjectItem(json,"redeem");
                if ( obj != 0 )
                {
                    copy_cJSON(coinname,obj);
                    if ( conv_coinstr(coinname) == coinid )
                    {
                        obj = cJSON_GetObjectItem(json,"coinaddr");
                        copy_cJSON(withdrawaddr,obj);
                        cp = get_daemon_info(coinid);
                        acct = get_coin_acct(coinid,sender);
                        if ( acct != 0 )
                        {
                            printf("acct withdraw.(%s)\n",acct->withdrawaddr);
                            if ( withdrawaddr[0] == 0 )
                                safecopy(withdrawaddr,acct->withdrawaddr,sizeof(withdrawaddr)), comment = 0;
                            if ( strcmp(withdrawaddr,acct->withdrawaddr) == 0 )
                                authenticatorflag = 0;
                        }
                        if ( withdrawaddr[0] == 0 || (cp != 0 && cp->validate_coinaddr != 0 && (*cp->validate_coinaddr)(cp,coinid,withdrawaddr) < 0) )
                        {
                            printf("no withdraw or invalid address.(%s) for NXT.%s %.8f\n",withdrawaddr,sender,dstr(assetoshis));
                            return;
                        }
                        wp = MTadd_hashtable(&createdflag,&lp->redeemtxids,txid);
                        if ( createdflag != 0 )
                        {
                            wp->coinid = coinid;
                            wp->amount = assetoshis;
                            safecopy(wp->NXTaddr,sender,sizeof(wp->NXTaddr));
                            safecopy(wp->withdrawaddr,withdrawaddr,sizeof(wp->withdrawaddr));
                            safecopy(wp->comment,comment,sizeof(wp->comment));
                            wp->twofactor = authenticatorflag;
                            printf("NXT.%s withdraw authenticator.%d %s amount %.8f\n",sender,authenticatorflag,coinid_str(coinid),dstr(wp->amount));
                            queue_enqueue(&lp->withdrawreqs.pingpong[0],wp);
                        }
                        else
                        {
                            if ( wp->amount != assetoshis )
                                printf("multigateway_handler %s rp->amount %.8f != %.8f assetoshis sender.%s\n",coinid_str(coinid),dstr(wp->amount),dstr(assetoshis),sender);
                            if ( strcmp(wp->NXTaddr,sender) != 0 )
                                printf("multigateway_handler %s rp->NXTaddr %s != sender.%s\n",coinid_str(coinid),wp->NXTaddr,sender);
                            if ( strcmp(wp->redeemtxid,txid) != 0 )
                                printf("multigateway_handler %s rp->NXTtxid %s != txid %s, sender.%s\n",coinid_str(coinid),wp->redeemtxid,txid,sender);
                            if ( comment != 0 && strcmp(wp->comment,comment) != 0 )
                                printf("multigateway_handler %s comment %s != %s, sender.%s\n",coinid_str(coinid),wp->comment,comment,sender);
                        }
                    }
                    else printf("invalid redeem coinname.%s for coinid.%d from asset.%s\n",coinname,coinid,assetid);
                }
            }
        }
    }
    if ( json != 0 )
        free_json(json);
}

void *multigateway_handler(struct NXThandler_info *mp,struct NXT_protocol_parms *parms,void *handlerdata)
{
    static int acctinitflag;
    struct replicated_coininfo *lp;
    cJSON *argjson;
    struct json_AM *ap;
    struct gateway_info *gp = handlerdata;
    char NXTaddr[64],*txid,*sender,*receiver;
   // char *jsontxt;
    int32_t createdflag,coinid;
    if ( parms->txid == 0 )     // indicates non-transaction event
    {
        if ( parms->mode == NXTPROTOCOL_WEBJSON )
            return(multigateway_jsonhandler(parms->argjson));
        if ( parms->mode == NXTPROTOCOL_NEWBLOCK )
            printf("multigateway new RTblock %d timestamp.%d\n",mp->RTflag,issue_getTime());
        else if ( parms->mode == NXTPROTOCOL_IDLETIME )
        {
            if ( acctinitflag == 0 )
            {
                printf(">>>>>>>>>>> init gateway accts\n");
                get_NXTacct(&createdflag,mp,NXTISSUERACCT);
                get_NXTacct(&createdflag,mp,NXTACCTA);
                get_NXTacct(&createdflag,mp,NXTACCTB);
                get_NXTacct(&createdflag,mp,NXTACCTC);

                printf(">>>>>>>>>>> done init gateway accts\n");
                acctinitflag = 1;
            }
            //printf("NXT.%d multigateway new idletime RTflag.%d timestamp.%d\n",mp->NXTheight,mp->RTflag,issue_getTime());
            //for (i=0; i<10; i++)
             ///   if ( Coinloop(gp) == 0 )
             //       break;
        }
        else if ( parms->mode == NXTPROTOCOL_INIT )
        {
            gp = calloc(1,sizeof(*gp));
            gp->gatewayid = get_gatewayid(mp->ipaddr);
            if ( mp->upollseconds != 0 )
                printf("WARNING: multigateway shouldnt poll so fast\n"), sleep(5);
            printf("multigateway init %d gatewayid.%d\n",mp->RTflag,gp->gatewayid);
            Global_gp = gp;
            init_multigateway(mp,gp);
        }
        return(gp);
    }
    gp->timestamp = mp->timestamp;
    txid = parms->txid; sender = parms->sender; receiver = parms->receiver; ap = parms->AMptr;
    if ( txid == 0 ) txid = "";
    if ( parms->mode == NXTPROTOCOL_AMTXID )
    {
        expand_nxt64bits(NXTaddr,ap->H.nxt64bits);
        if ( (argjson = parse_json_AM(ap)) != 0 )
        {
            printf("func.(%c) %s -> %s txid.(%s) assetid.(%s) JSON.(%s)\n",ap->funcid,sender,receiver,txid,parms->assetid,ap->jsonstr);
            switch ( ap->funcid )
            {
                    // user created AM's
                case GET_COINDEPOSIT_ADDRESS:
                    update_coinacct_addresses(ap->H.nxt64bits,argjson,txid,-1);
                    break;
                    
                    // gateway created AM's
                case BIND_DEPOSIT_ADDRESS:
                    if ( is_gateway_addr(sender) != 0 )
                    {
                        struct multisig_addr *msig;
                        if ( (msig= decode_msigjson(NXTaddr,argjson)) != 0 )
                            if ( update_acct_binding(msig->coinid,NXTaddr,msig) == 0 )
                                free(msig);
                    } else printf("sender.%s == NXTaddr.%s\n",sender,NXTaddr);
                    break;
                case DEPOSIT_CONFIRMED:
                    // need to mark cointxid with AMtxid to prevent confirmation process generating AM each time
                    if ( is_gateway_addr(sender) != 0 && (coinid= decode_depositconfirmed_json(argjson,txid)) >= 0 )
                    {
                        if ( (lp= get_replicated_coininfo(coinid)) != 0 )
                        {
                            queue_enqueue(&lp->retrydepositconfirmed.pingpong[0],argjson);
                            argjson = 0;    // preserve it as it is in retry queue
                        }
                    }
                    break;
                case MONEY_SENT:
                    if ( is_gateway_addr(sender) != 0 )
                        update_money_sent(argjson,txid);
                    break;
                default: printf("funcid.(%c) not handled\n",ap->funcid);
            }
            if ( argjson != 0 )
                free_json(argjson);
        } else printf("can't JSON parse (%s)\n",ap->jsonstr);
    }
    else if ( parms->assetid != 0 && parms->assetoshis > 0 )
        multigateway_assetevent(sender,receiver,txid,parms->assetid,parms->assetoshis,parms->comment);
    return(0);
}

#endif
