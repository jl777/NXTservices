//
//  genjson.h
//  Created by jl777 April 6+7th, 2014
//  MIT License
//


#ifndef gateway_genjson_h
#define gateway_genjson_h


cJSON *gen_funds_json(struct balance_info *bp)
{
    cJSON *json;
    json = cJSON_CreateObject();
    add_satoshis_json(json,"deposits",bp->deposits);
    add_satoshis_json(json,"transfers",bp->transfers);
    add_satoshis_json(json,"withdraws",bp->withdraws);
    add_satoshis_json(json,"moneysends",bp->moneysends);
    add_satoshis_json(json,"remaining_transfers",bp->remaining_transfers);
    add_satoshis_json(json,"remaining_moneysends",bp->remaining_moneysends);
    add_satoshis_json(json,"balance",bp->balance);
    return(json);
}

cJSON *gen_unspent_json(struct unspent_info *up)
{
    cJSON *json;
    json = cJSON_CreateObject();
    add_satoshis_json(json,"maxunspent",up->maxunspent);
    add_satoshis_json(json,"unspent",up->unspent);
    add_satoshis_json(json,"maxavail",up->maxavail);
    add_satoshis_json(json,"minavail",up->minavail);
    add_satoshis_json(json,"smallest_msigunspent",up->smallest_msigunspent);
    cJSON_AddItemToObject(json,"confirmed",cJSON_CreateNumber(up->num));
    return(json);
}

cJSON *gen_hashtable_json(struct hashtable *hp)
{
    cJSON *json;
    json = cJSON_CreateObject();
    cJSON_AddItemToObject(json,"numitems",cJSON_CreateNumber(hp->numitems));
    cJSON_AddItemToObject(json,"structsize",cJSON_CreateNumber(hp->structsize));
    cJSON_AddItemToObject(json,"hashsize",cJSON_CreateNumber(hp->hashsize));
    cJSON_AddItemToObject(json,"numsearches",cJSON_CreateNumber(hp->numsearches));
    cJSON_AddItemToObject(json,"numiterations",cJSON_CreateNumber(hp->numiterations));
    if ( hp->numsearches != 0 )
        cJSON_AddItemToObject(json,"numiterations",cJSON_CreateNumber(hp->numsearches / (double)hp->numiterations));
    return(json);
    
}

cJSON *gen_queue_json(struct pingpong_queue *ppq)
{
    cJSON *json;
    json = cJSON_CreateObject();
    cJSON_AddItemToObject(json,"queuesize",cJSON_CreateNumber(ppq->pingpong[0].size+ppq->pingpong[1].size));
    return(json);
}

cJSON *gen_NXTcrosschain_json(struct crosschain_NXTinfo *nxt)
{
    cJSON *json;
    json = cJSON_CreateObject();
    add_nxt64bits_json(json,"AMtxid",nxt->confirmedAMbits);
    add_nxt64bits_json(json,"asset_txid",nxt->asset_txid);
    cJSON_AddItemToObject(json,"pending",cJSON_CreateNumber(nxt->pending));
    cJSON_AddItemToObject(json,"confirmed",cJSON_CreateNumber(nxt->confirmed));
    return(json);
}

cJSON *gen_crosschain_json(int32_t coinid,struct crosschain_info *xp,char *NXTaddr)
{
    static struct crosschain_NXTinfo NXT;
    struct coin_value *vp;
    cJSON *json;
    json = cJSON_CreateObject();
    cJSON_AddItemToObject(json,"crosschain",cJSON_CreateString(coinid_str(coinid)));
    cJSON_AddItemToObject(json,"coin",cJSON_CreateString(coinid_str(coinid)));
    cJSON_AddItemToObject(json,"coinid",cJSON_CreateNumber(coinid));
    cJSON_AddItemToObject(json,"NXTaddr",cJSON_CreateString(NXTaddr));
    add_satoshis_json(json,"value",xp->value);
    add_satoshis_json(json,"assetoshis",xp->assetoshis);
    cJSON_AddItemToObject(json,coinid_str(coinid),cJSON_CreateString(xp->msig->multisigaddr));

    if ( (vp= xp->parent) != 0 )
    {
        if ( vp->parent != 0 )
            cJSON_AddItemToObject(json,"cointxid",cJSON_CreateString(vp->parent->txid));
        cJSON_AddItemToObject(json,"coinaddr",cJSON_CreateString(vp->coinaddr));
        cJSON_AddItemToObject(json,"vout",cJSON_CreateNumber(vp->parent_vout));
        if ( vp->spent != 0 )
        {
            cJSON_AddItemToObject(json,"spent",cJSON_CreateString(vp->spent->txid));
            cJSON_AddItemToObject(json,"spent_vin",cJSON_CreateNumber(vp->spent_vin));
        }
        else if ( vp->pendingspend != 0 )
        {
            cJSON_AddItemToObject(json,"pending",cJSON_CreateString(vp->pendingspend->txid));
            cJSON_AddItemToObject(json,"pending_vin",cJSON_CreateNumber(vp->pending_spendvin));
        }
    }
    cJSON_AddItemToObject(json,"NXTRTflag",cJSON_CreateNumber(xp->NXTRTflag));
    cJSON_AddItemToObject(json,"NXTxfermask",cJSON_CreateNumber(xp->NXTxfermask));
    cJSON_AddItemToObject(json,"coin_confirmed",cJSON_CreateNumber(xp->coin_confirmed));
    cJSON_AddItemToObject(json,"isinternal",cJSON_CreateNumber(xp->isinternal));
    if ( memcmp(&NXT,&xp->NXT,sizeof(NXT)) != 0 )
        cJSON_AddItemToObject(json,"NXTasset",gen_NXTcrosschain_json(&xp->NXT));
    return(json);
}

cJSON *gen_allcrosschain_json(int32_t coinid,struct crosschain_info **xps,int32_t num,char *NXTaddr)
{
    int32_t i;
    char tmpaddr[64];
    struct crosschain_info *xp;
    cJSON *json,*array,*item;
    printf("gen crosschain.%d num.%d\n",coinid,num);
    json = cJSON_CreateObject();
    array = cJSON_CreateArray();
    for (i=0; i<num; i++)
    {
        if ( (xp= xps[i]) != 0 && xp->msig != 0 && xp->msig->coinid == coinid )
        {
            expand_nxt64bits(tmpaddr,xp->nxt64bits);
            if ( NXTaddr != 0 && NXTaddr[0] != 0 && strcmp(tmpaddr,NXTaddr) != 0 )
                continue;
            item = gen_crosschain_json(coinid,xp,NXTaddr);
            if ( item != 0 )
                cJSON_AddItemToArray(array,item);
        }
    }
    return(json);
}

char *createmultisig_json_params(struct multisig_addr *msig)
{
    int32_t i;
    char *paramstr;
    cJSON *array,*mobj,*keys,*key;
    array = cJSON_CreateArray();
    keys = cJSON_CreateArray();
    for (i=0; i<msig->n; i++)
    {
        key = cJSON_CreateString(msig->pubkeys[i].pubkey);
        cJSON_AddItemToArray(keys,key);
    }
    mobj = cJSON_CreateNumber(msig->m);
    cJSON_AddItemToArray(array,mobj);
    cJSON_AddItemToArray(array,keys);
    paramstr = cJSON_Print(array);
    free_json(array);
    //printf("createmultisig_json_params.%s\n",paramstr);
    return(paramstr);
}

cJSON *gen_withdraws_json(int32_t coinid,struct withdraw_info **wps,int32_t num,char *NXTaddr)
{
    int32_t i;
    char *rawstr;
    cJSON *array,*item,*obj;
    struct withdraw_info *wp;
    array = cJSON_CreateObject();
    for (i=0; i<num; i++)
    {
        wp = wps[i];
        if ( NXTaddr != 0 && NXTaddr[0] != 0 && strcmp(NXTaddr,wp->NXTaddr) != 0 )
            continue;
        item = cJSON_CreateObject();
        cJSON_AddItemToObject(item,"NXTaddr",cJSON_CreateString(wp->NXTaddr));
        cJSON_AddItemToObject(item,"redeemtxid",cJSON_CreateString(wp->redeemtxid));
        if ( wp->comment[0] != 0 )
            cJSON_AddItemToObject(item,"comment",cJSON_CreateString(wp->comment));
        add_nxt64bits_json(item,"AMtxid",wp->AMtxidbits);
        add_satoshis_json(item,"amount",wp->amount);
        cJSON_AddItemToObject(item,"withdrawaddr",cJSON_CreateString(wp->withdrawaddr));
        cJSON_AddItemToObject(item,"cointxid",cJSON_CreateString(wp->cointxid));
        cJSON_AddItemToObject(item,"2FA",cJSON_CreateNumber(wp->twofactor));
        cJSON_AddItemToObject(item,"authenticated",cJSON_CreateNumber(wp->authenticated));
        add_satoshis_json(item,"moneysent",wp->moneysent);
        if ( wp->rawtx.numinputs != 0 || wp->rawtx.numoutputs != 0 )
        {
            rawstr = createrawtxid_json_params(0,coinid,&wp->rawtx);
            if ( rawstr != 0 )
            {
                obj = cJSON_Parse(rawstr);
                if ( obj != 0 )
                    cJSON_AddItemToObject(item,"rawtx",obj);
                free(rawstr);
            }
        }
        cJSON_AddItemToArray(array,item);
    }
    return(array);
}

cJSON *gen_coininfo_json(int32_t coinid,char *NXTaddr)
{
    struct gateway_info *gp = Global_gp;
    struct replicated_coininfo *lp;
    int32_t i,max;
    cJSON *json;
    char userpass[512];
    json = cJSON_CreateObject();
    
    cJSON_AddItemToObject(json,"assetname",cJSON_CreateString(coinid_str(coinid)));
    cJSON_AddItemToObject(json,"funds",gen_funds_json(get_balance_info(coinid)));
    cJSON_AddItemToObject(json,"unspent",gen_unspent_json(get_unspent_info(coinid)));
    lp = get_replicated_coininfo(coinid);
    if ( lp != 0 )
    {
        cJSON_AddItemToObject(json,"crosschains",cJSON_CreateNumber(lp->numxps));
        cJSON_AddItemToObject(json,"transfers",cJSON_CreateNumber(lp->numtransfers));
        cJSON_AddItemToObject(json,"redemptions",cJSON_CreateNumber(lp->numredemptions));
        cJSON_AddItemToObject(json,"moneysends",cJSON_CreateNumber(lp->nummoneysends));
        if ( lp->numxps != 0 )
            cJSON_AddItemToObject(json,"crosschain",gen_allcrosschain_json(coinid,lp->xps,lp->numxps,NXTaddr));
        if ( lp->numredemptions != 0 )
            cJSON_AddItemToObject(json,"withdraws",gen_withdraws_json(coinid,lp->wps,lp->numredemptions,NXTaddr));
        
        if ( gp->gatewayid >= 0 )
        {
            cJSON_AddItemToObject(json,lp->genaddr_txids->name,gen_hashtable_json(lp->genaddr_txids));
            cJSON_AddItemToObject(json,lp->genreqs.name,gen_queue_json(&lp->genreqs));
            cJSON_AddItemToObject(json,lp->genaddr.name,gen_queue_json(&lp->genaddr));
            cJSON_AddItemToObject(json,lp->authenticate.name,gen_queue_json(&lp->authenticate));
            cJSON_AddItemToObject(json,lp->sendcoin.name,gen_queue_json(&lp->sendcoin));
        }
        cJSON_AddItemToObject(json,lp->retrydepositconfirmed.name,gen_queue_json(&lp->retrydepositconfirmed));
        //cJSON_AddItemToObject(json,lp->retryassetxfer.name,gen_queue_json(&lp->retryassetxfer));
        cJSON_AddItemToObject(json,lp->withdrawreqs.name,gen_queue_json(&lp->withdrawreqs));
        cJSON_AddItemToObject(json,lp->withdraws.name,gen_queue_json(&lp->withdraws));
        
        cJSON_AddItemToObject(json,lp->coin_txids->name,gen_hashtable_json(lp->coin_txids));
        cJSON_AddItemToObject(json,lp->redeemtxids->name,gen_hashtable_json(lp->redeemtxids));
    }
    cJSON_AddItemToObject(json,"assetid",cJSON_CreateString(assetid_str(coinid)));
    cJSON_AddItemToObject(json,"serverport",cJSON_CreateString(gp->serverport[coinid]));
    max = 512;
    for (i=0; i<max; i++)
    {
        userpass[i] = gp->userpass[coinid][i];
        if ( userpass[i] == 0 )
            break;
        else if ( userpass[i] == ':' )
            max = i+5;
    }
    userpass[i] = 0;
    strcat(userpass,"....");
    cJSON_AddItemToObject(json,"userpass",cJSON_CreateString(userpass));
    /*array = cJSON_CreateArray();
    for (i=0; i<NUM_GATEWAYS; i++)
        cJSON_AddItemToArray(array,cJSON_CreateString(cp->gateways[i]));
    cJSON_AddItemToObject(json,"gateways",array);
    
    cJSON_AddItemToObject(json,"coinid",cJSON_CreateNumber(cp->coinid));
    cJSON_AddItemToObject(json,"min_confirms",cJSON_CreateNumber(cp->min_confirms));
    cJSON_AddItemToObject(json,"pollseconds",cJSON_CreateNumber(cp->pollseconds));
    cJSON_AddItemToObject(json,"nummsigs",cJSON_CreateNumber(cp->nummsigs));
    cJSON_AddItemToObject(json,"numwithdraws",cJSON_CreateNumber(cp->numwithdraws));
    cJSON_AddItemToObject(json,"numdeposits",cJSON_CreateNumber(cp->numdeposits));
    cJSON_AddItemToObject(json,"initdone",cJSON_CreateNumber(cp->initdone));
    cJSON_AddItemToObject(json,"blockheight",cJSON_CreateNumber(cp->blockheight));
    cJSON_AddItemToObject(json,"NXTheight",cJSON_CreateNumber(cp->NXTheight));
    add_satoshis_json(json,"txfee",cp->txfee);
    add_satoshis_json(json,"markeramount",cp->markeramount);*/
    
    return(json);
}

cJSON *gen_coins_json(char *NXTaddr)
{
    struct replicated_coininfo *lp;
    int coinid;
    cJSON *json,*array;
    array = cJSON_CreateArray();
    for (coinid=0; coinid<64; coinid++)
    {
        lp = get_replicated_coininfo(coinid);
        if ( lp != 0 )
        {
            json = cJSON_CreateObject();
            cJSON_AddItemToObject(json,coinid_str(coinid),gen_coininfo_json(coinid,NXTaddr));
            cJSON_AddItemToArray(array,json);
        }
    }
    return(array);
}

cJSON *gen_asset_json(struct NXT_asset *asset,uint64_t quantity,struct NXT_assettxid_list *txlists)
{
    int32_t i;
    char sender[64],receiver[64];
    struct NXT_assettxid *txid;
    cJSON *json,*array,*item;
    json = cJSON_CreateObject();
    cJSON_AddItemToObject(json,"assetid",cJSON_CreateString(asset->H.assetid));
    add_satoshis_json(json,"quantity",quantity);
    if ( txlists->num > 0 )
    {
        array = cJSON_CreateArray();
        for (i=0; i<txlists->num; i++)
        {
            txid = txlists->txids[i];
            if ( txid != 0 )
            {
                item = cJSON_CreateObject();
                cJSON_AddItemToObject(item,"txid",cJSON_CreateString(txid->H.txid));
                expand_nxt64bits(sender,txid->senderbits);
                cJSON_AddItemToObject(item,"sender",cJSON_CreateString(sender));
                expand_nxt64bits(receiver,txid->receiverbits);
                cJSON_AddItemToObject(item,"receiver",cJSON_CreateString(receiver));
                add_satoshis_json(item,"quantity",txid->quantity);
                if ( txid->price != 0 )
                    add_satoshis_json(item,"price",txid->price);
                if ( txid->comment != 0 )
                    cJSON_AddItemToObject(item,"comment",cJSON_CreateString(txid->comment));
                if ( txid->guid != 0 )
                    cJSON_AddItemToObject(item,"hash",cJSON_CreateString(txid->guid));
                cJSON_AddItemToArray(array,item);
            }
        }
        cJSON_AddItemToObject(json,"txids",array);
    }
    return(json);
}

cJSON *gen_assets_json(char *assetid,struct NXT_asset **assets,uint64_t *quantities,struct NXT_assettxid_list **txlists,int32_t num)
{
    int32_t i;
    cJSON *array,*item;
    array = cJSON_CreateArray();
    for (i=0; i<num; i++)
    {
        if ( assetid == 0 || strcmp(assetid,assets[i]->H.assetid) == 0 )
        {
            item = gen_asset_json(assets[i],quantities[i],txlists[i]);
            if ( item != 0 )
                cJSON_AddItemToArray(array,item);
        }
    }
    return(array);
}

cJSON *gen_coinacct_json(struct coin_acct *acct,char *NXTaddr)
{
    int32_t i;
    struct multisig_addr *msig;
    struct replicated_coininfo *lp;
    cJSON *json,*array,*item;
    if ( acct->nummsigs == 0 )
        return(0);
    json = cJSON_CreateObject();
    cJSON_AddItemToObject(json,"coin",cJSON_CreateString(coinid_str(acct->coinid)));
    cJSON_AddItemToObject(json,"funds",gen_funds_json(&acct->funds));
    cJSON_AddItemToObject(json,"withdrawaddr",cJSON_CreateString(acct->withdrawaddr));
    cJSON_AddItemToObject(json,"enable_2FA",cJSON_CreateNumber(acct->enable_2FA));
    cJSON_AddItemToObject(json,"nummsigs",cJSON_CreateNumber(acct->nummsigs));
    if ( acct->nummsigs != 0 )
    {
        array = cJSON_CreateArray();
        for (i=0; i<acct->nummsigs; i++)
        {
            msig = acct->msigs[i];
            if ( msig != 0 )
            {
                item = cJSON_CreateObject();
                cJSON_AddItemToObject(item,"addr",cJSON_CreateString(msig->multisigaddr));
                cJSON_AddItemToObject(item,"gatewayA",cJSON_CreateString(msig->pubkeys[0].server));
                cJSON_AddItemToObject(item,"gatewayB",cJSON_CreateString(msig->pubkeys[1].server));
                cJSON_AddItemToObject(item,"gatewayC",cJSON_CreateString(msig->pubkeys[2].server));
                cJSON_AddItemToArray(array,item);
            }
        }
        cJSON_AddItemToObject(json,"multisigs",array);
    }
    lp = get_replicated_coininfo(acct->coinid);
    if ( lp != 0 )
    {
        cJSON_AddItemToObject(json,"crosschain",gen_allcrosschain_json(acct->coinid,lp->xps,lp->numxps,NXTaddr));
        cJSON_AddItemToObject(json,"withdraws",gen_withdraws_json(acct->coinid,lp->wps,lp->numredemptions,NXTaddr));
    }
    return(json);
}

cJSON *gen_coinaccts_json(struct coin_acct *coinaccts,int32_t num,char *NXTaddr)
{
    int32_t i;
    cJSON *array,*item;
    array = cJSON_CreateArray();
    for (i=0; i<num; i++)
    {
        item = gen_coinacct_json(&coinaccts[i],NXTaddr);
        if ( item != 0 )
            cJSON_AddItemToArray(array,item);
    }
    return(array);
}

cJSON *gen_NXTacct_json(struct NXT_acct *np,char *coinname,char *assetid)
{
    int32_t i;
    cJSON *item,*obj;
    item = cJSON_CreateObject();
    printf("gen_NXTacct_json %s\n",np->H.NXTaddr);
    cJSON_AddItemToObject(item,"NXTaddr",cJSON_CreateString(np->H.NXTaddr));
    if ( np->numcoinaccts != 0 )
    {
        if( coinname == 0 )
            cJSON_AddItemToObject(item,"coinaccts",gen_coinaccts_json(np->coinaccts,np->numcoinaccts,np->H.NXTaddr));
        else
        {
            for (i=0; i<np->numcoinaccts; i++)
            {
                if ( strcmp(coinid_str(np->coinaccts[i].coinid),coinname) == 0 )
                {
                    obj = gen_coinacct_json(&np->coinaccts[i],np->H.NXTaddr);
                    if ( obj != 0 )
                        cJSON_AddItemToObject(item,coinname,obj);
                }
            }
        }
    }
    cJSON_AddItemToObject(item,"numassets",cJSON_CreateNumber(np->numassets));
    if ( np->numassets != 0 )
        cJSON_AddItemToObject(item,"assets",gen_assets_json(assetid,np->assets,np->quantities,np->txlists,np->numassets));
    cJSON_AddItemToObject(item,"numcoinaccts",cJSON_CreateNumber(np->numcoinaccts));
    return(item);
}

cJSON *gen_NXTaccts_json(char *assetid)
{
    int64_t i,changed;
    cJSON *array;
    struct NXT_acct **nps,*np;
    nps = (struct NXT_acct **)hashtable_gather_modified(&changed,*Global_mp->NXTaccts_tablep,1); // no MT support (yet) hopefully safe enough for testing
    if ( nps == 0 )
        return(0);
    array = cJSON_CreateArray();
    for (i=0; i<changed; i++)
    {
        np = nps[i];
        cJSON_AddItemToArray(array,gen_NXTacct_json(np,0,assetid));
    }
    free(nps);
    return(array);
}

// functions returning char *
char *unspent_json_params(char *txid,int32_t vout)
{
    char *unspentstr;
    cJSON *array,*nobj,*txidobj;
    array = cJSON_CreateArray();
    nobj = cJSON_CreateNumber(vout);
    txidobj = cJSON_CreateString(txid);
    cJSON_AddItemToArray(array,txidobj);
    cJSON_AddItemToArray(array,nobj);
    unspentstr = cJSON_Print(array);
    free_json(array);
    return(unspentstr);
}

char *create_depositconfirmed_jsontxt(int32_t coinid,char *coinaddr,char *cointxid,int32_t vout,int64_t value,uint64_t asset_txid,int64_t assetoshis,char *NXTaddr)
{
    char *jsontxt,txid[64];
    cJSON *json,*obj;
    if ( Global_gp->gatewayid < 0 )
        return(0);
    //printf("%s createasset.(%s).%d\n",coinid_str(coinid),cointxid,vout);
    json = cJSON_CreateObject();
    obj = cJSON_CreateNumber(coinid); cJSON_AddItemToObject(json,"coinid",obj);
    obj = cJSON_CreateNumber(vout); cJSON_AddItemToObject(json,"vout",obj);
    obj = cJSON_CreateString(coinaddr); cJSON_AddItemToObject(json,"coinaddr",obj);
    obj = cJSON_CreateString(coinid_str(coinid)); cJSON_AddItemToObject(json,"coin",obj);
    obj = cJSON_CreateString(cointxid); cJSON_AddItemToObject(json,"cointxid",obj);
    obj = cJSON_CreateString(NXTaddr); cJSON_AddItemToObject(json,"NXTaddr",obj);
    expand_nxt64bits(txid,asset_txid);
    //obj = cJSON_CreateString(txid); cJSON_AddItemToObject(json,"NXTtxid",obj);
    add_satoshis_json(json,"assetoshis",assetoshis);
    add_satoshis_json(json,"value",value);
    jsontxt = cJSON_Print(json);
    free_json(json);
    return(jsontxt);
}

char *create_moneysent_jsontxt(int32_t coinid,struct withdraw_info *wp)
{
    cJSON *json,*obj;
    char *jsontxt;
    json = cJSON_CreateObject();
    obj = cJSON_CreateString(wp->NXTaddr); cJSON_AddItemToObject(json,"NXTaddr",obj);
    obj = cJSON_CreateNumber(coinid); cJSON_AddItemToObject(json,"coinid",obj);
    obj = cJSON_CreateString(coinid_str(coinid)); cJSON_AddItemToObject(json,"coin",obj);
    obj = cJSON_CreateString(wp->redeemtxid); cJSON_AddItemToObject(json,"redeemtxid",obj);
    obj = cJSON_CreateNumber(wp->amount); cJSON_AddItemToObject(json,"value",obj);
    obj = cJSON_CreateString(wp->withdrawaddr); cJSON_AddItemToObject(json,"coinaddr",obj);
    obj = cJSON_CreateString(wp->cointxid); cJSON_AddItemToObject(json,"cointxid",obj);
    jsontxt = cJSON_Print(json);
    free_json(json);
    return(jsontxt);
}

long calc_pubkey_jsontxt(char *jsontxt,struct pubkey_info *ptr,char *postfix)
{
    sprintf(jsontxt,"{\"address\":\"%s\",\"pubkey\":\"%s\",\"ipaddr\":\"%s\"}%s",ptr->coinaddr,ptr->pubkey,ptr->server,postfix);
    return(strlen(jsontxt));
}

char *create_multisig_json(struct multisig_addr *msig)
{
    long len;
    char jsontxt[2048],pubkeyjsontxt[1024];
    len = calc_pubkey_jsontxt(pubkeyjsontxt,&msig->pubkeys[0],",");
    len += calc_pubkey_jsontxt(pubkeyjsontxt+len,&msig->pubkeys[1],",");
    len += calc_pubkey_jsontxt(pubkeyjsontxt+len,&msig->pubkeys[2],"");
    sprintf(jsontxt,"{\"NXTaddr\":\"%s\",\"address\":\"%s\",\"redeemScript\":\"%s\",\"coin\":\"%s\",\"coinid\":\"%d\",\"pubkey\":[%s]}",msig->NXTaddr,msig->multisigaddr,msig->redeemScript,coinid_str(msig->coinid),msig->coinid,pubkeyjsontxt);
    printf("(%s) pubkeys len.%ld msigjsonlen.%ld\n",jsontxt,len,strlen(jsontxt));
    return(clonestr(jsontxt));
}

#endif
