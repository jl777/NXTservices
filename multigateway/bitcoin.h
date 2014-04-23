//
//  bitcoin.h
//  Created by jl777, March 2014
//  MIT License
//

#ifndef gateway_bitcoin_h
#define gateway_bitcoin_h

// bitcoind commands used:  backupwallet getinfo gettxout getblockhash getblock
//                          getaccountaddress validateaddress createmultisig  
//                          getrawtransaction decoderawtransaction signrawtransaction submitrawtransaction



// lowest level bitcoind functions
int64_t issue_bitcoind_command(char *extract,struct daemon_info *cp,int32_t coinid,char *command,char *field,char *arg)
{
    struct gateway_info *gp = Global_gp;
    char *retstr;
    cJSON *obj,*json;
    int64_t val = 0;
    retstr = bitcoind_RPC(coinid_str(coinid),cp->numretries,gp->serverport[coinid],gp->userpass[coinid],command,arg);
    json = 0;
    if ( retstr != 0 && retstr[0] != 0 )
    {
        json = cJSON_Parse(retstr);
        if ( field != 0 )
        {
            if ( json != 0 )
            {
                if ( extract == 0 )
                    val = get_cJSON_int(json,field);
                else
                {
                    obj = cJSON_GetObjectItem(json,field);
                    copy_cJSON(extract,obj);
                    val = strlen(extract);
                }
            }
        }
        else if ( extract != 0 )
            copy_cJSON(extract,json);
        if ( json != 0 )
            free_json(json);
        free(retstr);
    }
    return(val);
}

int32_t validate_coinaddr(struct daemon_info *cp,int32_t coinid,char *coinaddr)
{
    char output[4096],quotes[128];
    int64_t len;
    if ( coinaddr[0] != '"' )
        sprintf(quotes,"\"%s\"",coinaddr);
    else safecopy(quotes,coinaddr,sizeof(quotes));
    len = issue_bitcoind_command(output,cp,coinid,"validateaddress","isvalid",quotes);
    if ( len != 0 && strcmp(output,"true") == 0 )
        return(0);
    else return(-1);
}

int64_t get_blockheight(struct daemon_info *cp,int32_t coinid)
{
    return(issue_bitcoind_command(0,cp,coinid,"getinfo","blocks",""));
}

struct coin_value *update_coin_value(struct daemon_info *cp,int32_t coinid,int32_t isinternal,int32_t isconfirmed,struct coin_txid *tp,int32_t i,struct coin_value *vp,int64_t value,char *coinaddr,char *script)
{
    if ( vp == 0 )
        vp = calloc(1,sizeof(*vp));
    vp->value = value;
    vp->parent = tp;
    vp->parent_vout = i;
    vp->isinternal += isinternal;
    safecopy(vp->coinaddr,coinaddr,sizeof(vp->coinaddr));
    if ( script != 0 )
    {
        if ( vp->script != 0 )
            free(vp->script);
        vp->script = clonestr(script);
    }
    vp->xp = update_crosschain_info(coinid,tp,vp,isconfirmed,vp->xp,value,coinaddr);
    vp->xp->isinternal += isinternal;
    tp->hasinternal += isinternal;
    vp->isconfirmed += isconfirmed;
    return(vp);
}

void process_vins(struct daemon_info *cp,int32_t coinid,struct coin_txid *tp,int32_t isconfirmed,cJSON *vins)
{
    cJSON *obj,*txidobj,*coinbaseobj;
    int32_t i,vout,oldnumvins;
    char txid[4096],coinbase[4096];
    struct coin_value *vp;
    struct coin_txid *vintp;
    if ( vins != 0 )
    {
        oldnumvins = tp->numvins;
        tp->numvins = cJSON_GetArraySize(vins);
        tp->vins = realloc(tp->vins,tp->numvins * sizeof(*tp->vins));
        if ( oldnumvins < tp->numvins )
            memset(&tp->vins[oldnumvins],0,(tp->numvins-oldnumvins) * sizeof(*tp->vins));
        for (i=0; i<tp->numvins; i++)
        {
            obj = cJSON_GetArrayItem(vins,i);
            if ( tp->numvins == 1  )
            {
                coinbaseobj = cJSON_GetObjectItem(obj,"coinbase");
                copy_cJSON(coinbase,coinbaseobj);
                if ( strlen(coinbase) > 1 )
                {
                    //printf("got coinbase.(%s)\n",coinbase);
                    vp = calloc(1,sizeof(*vp));
                    vp->parent_vout = 0;
                    vp->parent = tp;
                    vp->iscoinbase = 1;
                    vp->coinbase = clonestr(coinbase);
                    tp->vins[0] = vp;
                    //printf("txid.%s is coinbase.%s\n",tp->txid,coinbase);
                    return;
                }
            }
            txidobj = cJSON_GetObjectItem(obj,"txid");
            if ( txidobj != 0 && cJSON_GetObjectItem(obj,"vout") != 0 )
            {
                vout = (int)get_cJSON_int(obj,"vout");
                copy_cJSON(txid,txidobj);
                //printf("got txid.%s in i.%d\n",txid,i);
                vintp = find_coin_txid(coinid,txid);
                if ( vintp != 0 && vintp->vouts != 0 )
                {
                    if ( search_multisig_addrs(coinid,vintp->vouts[vout]->coinaddr) != 0 && tp->hasinternal == 0 )
                    {
                        printf("unexpected non-internal txid.%s vin.%d internal address %s %s.%d!\n",tp->txid,i,vintp->vouts[vout]->coinaddr,txid,vout);
                        tp->hasinternal = 1;
                    }
                    if ( vout >= 0 && vout < vintp->numvouts )
                    {
                        vp = vintp->vouts[vout];
                        if ( vp == 0 || vp->parent_vout != vout || vp->parent != vintp )
                            printf("error finding vin txid.%s vout.%d | %p %d %p %p\n",txid,vout,vp,vp->parent_vout,vp->parent,vintp);
                        else
                        {
                            tp->vins[i] = vp;
                            if ( isconfirmed != 0 )
                            {
                                vp->spent = tp, vp->spent_vin = i;
                                if ( vp->pendingspend != 0 && (tp != vp->pendingspend || i != vp->pending_spendvin) )
                                    printf("WARNING: confirm spend %p.%d, pending %p.%d\n",tp,i,vp->pendingspend,vp->pending_spendvin);
                            }
                            else vp->pendingspend = tp, vp->pending_spendvin = i;
                        }
                    }
                    else printf("vout.%d error when txid.%s numvouts.%d\n",vout,txid,vintp->numvouts);
                }
            } else printf("tp->numvins.%d missing txid.vout for %s\n",tp->numvins,tp->txid);
        }
    }
}

cJSON *script_has_address(int32_t *nump,cJSON *scriptobj)
{
    int32_t i,n;
    cJSON *addresses,*addrobj;
    if ( scriptobj == 0 )
        return(0);
    addresses = cJSON_GetObjectItem(scriptobj,"addresses");
    *nump = 0;
    if ( addresses != 0 )
    {
        *nump = n = cJSON_GetArraySize(addresses);
        for (i=0; i<n; i++)
        {
            addrobj = cJSON_GetArrayItem(addresses,i);
            return(addrobj);
        }
    }
    return(0);
}

uint64_t process_vouts(char *debugstr,struct daemon_info *cp,int32_t coinid,struct coin_txid *tp,int32_t isconfirmed,cJSON *vouts)
{
    struct gateway_info *gp = Global_gp;
    uint64_t value,unspent = 0;
    char coinaddr[4096],script[4096];
    cJSON *obj,*scriptobj,*addrobj,*hexobj;
    int32_t i,nval,numaddresses,oldnumvouts,isinternal;
    struct coin_value *vp;
    if ( vouts != 0 )
    {
        oldnumvouts = tp->numvouts;
        tp->numvouts = cJSON_GetArraySize(vouts);
        tp->vouts = realloc(tp->vouts,tp->numvouts*sizeof(*tp->vouts));
        if ( oldnumvouts < tp->numvouts )
            memset(&tp->vouts[oldnumvouts],0,(tp->numvouts-oldnumvouts) * sizeof(*tp->vouts));
        isinternal = 0;
        for (i=0; i<tp->numvouts; i++)
        {
            obj = cJSON_GetArrayItem(vouts,i);
            if ( obj != 0 )
            {
                addrobj = hexobj = 0;
                nval = (int)get_cJSON_int(obj,"n");
                scriptobj = cJSON_GetObjectItem(obj,"scriptPubKey");
                value = (int64_t)(get_cJSON_float(obj,"value") * SATOSHIDEN);
                if ( scriptobj != 0 )
                {
                    addrobj = script_has_address(&numaddresses,scriptobj);
                    hexobj = cJSON_GetObjectItem(scriptobj,"hex");
                }
                if ( nval == i && scriptobj != 0 && addrobj != 0 && hexobj != 0 )
                {
                    copy_cJSON(script,hexobj);
                    copy_cJSON(coinaddr,addrobj);
                    if ( i == 0 && strcmp(gp->internalmarker[coinid],coinaddr) == 0 )
                        isinternal = 1;
                    vp = update_coin_value(cp,coinid,i>1?isinternal:0,isconfirmed,tp,i,tp->vouts[i],value,coinaddr,script);
                    if ( vp != 0 )
                    {
                        tp->vouts[i] = vp;
                        if ( vp->spent == 0 && vp->pendingspend == 0 )
                            unspent += value;
                    }
                }
                else if ( value > 0 )   // OP_RETURN has no value and no address
                    printf("unexpected error nval.%d for i.%d of %d | scriptobj %p (%s) addrobj.%p debugstr.(%s)\n",nval,i,tp->numvouts,scriptobj,tp->txid,addrobj,debugstr);
            }
        }
    }
    return(unspent);
}

#ifdef DEBUG_MODE
uint64_t calc_coin_unspent(struct daemon_info *cp,int32_t coinid,struct coin_txid *tp)
{
    struct gateway_info *gp = Global_gp;
    int32_t i,flag;
    char *retstr,*unspentstr;
    cJSON *json;
    struct coin_value *vp;
    int64_t unspent = 0;
    for (i=0; i<tp->numvouts; i++)
    {
        vp = tp->vouts[i];
        if ( vp != 0 )
        {
            unspentstr = unspent_json_params(tp->txid,i);
            retstr = bitcoind_RPC(coinid_str(coinid),cp->numretries,gp->serverport[coinid],gp->userpass[coinid],"gettxout",unspentstr);
            flag = 0;
            if ( retstr != 0 && retstr[0] != 0 )
            {
                json = cJSON_Parse(retstr);
                if ( json != 0 )
                {
                    unspent += (int64_t)(get_cJSON_float(json,"value") * SATOSHIDEN);
                    flag = 1;
                    free_json(json);
                }
                free(retstr);
            }
            if ( (vp->spent == 0 && flag == 0) || (vp->spent != 0 && flag != 0) )
            {
                printf("unspent tracking mismatch?? vp->spent %p vs flag.%d for txid.%s vout.%d (%s)\n",vp->spent,flag,tp->txid,i,unspentstr);
                cp->mismatch++;
            }
            free(unspentstr);
        }
    }
    return(unspent);
}
#endif

void update_coin_values(struct daemon_info *cp,int32_t coinid,struct coin_txid *tp,int64_t blockheight,int32_t RTmode)
{
    struct gateway_info *gp = Global_gp;
    char *rawtransaction=0,*retstr=0,*str=0,txid[4096];
    cJSON *json;
    //int64_t unspent;
    int32_t isconfirmed;
    tp->numvouts = tp->numvins = 0;
    tp->height = (int32_t)blockheight;
    if ( RTmode == 0 )
        isconfirmed = 1;
    else isconfirmed = (blockheight <= (gp->blockheight[coinid] - gp->min_confirms[coinid]));
    sprintf(txid,"\"%s\"",tp->txid);
    rawtransaction = bitcoind_RPC(coinid_str(coinid),cp->numretries,gp->serverport[coinid],gp->userpass[coinid],"getrawtransaction",txid);
    if ( rawtransaction != 0 && rawtransaction[0] != 0 )
    {
        str = malloc(strlen(rawtransaction)+4);
        //printf("got rawtransaction.(%s)\n",rawtransaction);
        sprintf(str,"\"%s\"",rawtransaction);
        retstr = bitcoind_RPC(coinid_str(coinid),cp->numretries,gp->serverport[coinid],gp->userpass[coinid],"decoderawtransaction",str);
        if ( retstr != 0 && retstr[0] != 0 )
        {
            //printf("got decodetransaction.(%s)\n",retstr);
            json = cJSON_Parse(retstr);
            if ( json != 0 )
            {//unspent = 
                process_vouts(retstr,cp,coinid,tp,isconfirmed,cJSON_GetObjectItem(json,"vout"));
                process_vins(cp,coinid,tp,isconfirmed,cJSON_GetObjectItem(json,"vin"));
                free_json(json);
            }
        } else printf("error decoding.(%s)\n",str);
        free(str);
    } else printf("error with getrawtransaction %s %s\n",coinid_str(coinid),txid);
    if ( retstr != 0 )
        free(retstr);
    if ( rawtransaction != 0 )
        free(rawtransaction);
#ifdef DEBUG_MODE
   /* if ( 1 )
    {
        int64_t checkunspent = calc_coin_unspent(cp,coinid,tp);
        if ( checkunspent != 0 && checkunspent != unspent )
            printf("%s txid.%s %.8f unspent != checkunspent %.8f\n",coinid_str(coinid),tp->txid,dstr(unspent),dstr(checkunspent));
    }*/
#endif
}

int32_t add_bitcoind_uniquetxids(struct daemon_info *cp,int32_t coinid,int64_t blockheight,int32_t RTmode)
{
    struct gateway_info *gp = Global_gp;
    struct replicated_coininfo *lp = get_replicated_coininfo(coinid);
    char txid[4096],numstr[128],*blockhashstr,*blocktxt;
    cJSON *json,*txidobj,*txobj;
    int32_t i,n,createdflag,iter;
    struct coin_txid *tp;
    int64_t block;
    if ( lp == 0 )
        return(0);
    for (iter=0; iter<1+RTmode; iter++)
    {
        block = (long)blockheight - iter*gp->min_confirms[coinid];
        sprintf(numstr,"%ld",(long)block);
        blockhashstr = bitcoind_RPC(coinid_str(coinid),cp->numretries,gp->serverport[coinid],gp->userpass[coinid],"getblockhash",numstr);
        if ( blockhashstr == 0 || blockhashstr[0] == 0 )
        {
            printf("couldnt get blockhash for %ld at %ld\n",(long)block,(long)blockheight);
            return(-1);
        }
        if ( RTmode != 0 )
            fprintf(stderr,"%s block.%ld blockhash.%s\n",coinid_str(coinid),(long)block,blockhashstr);
        sprintf(txid,"\"%s\"",blockhashstr);
        blocktxt = bitcoind_RPC(coinid_str(coinid),cp->numretries,gp->serverport[coinid],gp->userpass[coinid],"getblock",txid);
        if ( blocktxt != 0 && blocktxt[0] != 0 )
        {
            json = cJSON_Parse(blocktxt);
            if ( json != 0 )
            {
                txobj = cJSON_GetObjectItem(json,"tx");
                n = cJSON_GetArraySize(txobj);
                for (i=0; i<n; i++)
                {
                    txidobj = cJSON_GetArrayItem(txobj,i);
                    copy_cJSON(txid,txidobj);
                    //printf("blocktxt.%ld i.%d of n.%d %s\n",(long)block,i,n,txid);
                    if ( txid[0] != 0 )
                    {
                        tp = MTadd_hashtable(&createdflag,&lp->coin_txids,txid);
                        //printf("call update_coin_values block.%ld txid.%s tp %p, createdflag.%d\n",(long)block,txid,tp,createdflag);
                        update_coin_values(cp,coinid,tp,block,RTmode);
                    }
                }
                free_json(json);
            } else printf("getblock error parsing.(%s)\n",txid);
            free(blocktxt);
        }
        else
        {
            printf("error getting blocktxt from (%s)\n",blockhashstr);
            free(blockhashstr);
            return(-1);
        }
        free(blockhashstr);
    }
    return(0);
}

#endif
