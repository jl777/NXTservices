 
// Created by jl777, Feb-Mar 2014
// MIT License
//
#include "jl777.h"

int32_t Historical_done;
struct NXThandler_info *Global_mp;

int32_t get_asset_in_acct(struct NXT_acct *acct,struct NXT_asset *ap,int32_t createflag)
{
    int32_t i;
    uint64_t assetbits = ap->assetbits;
    for (i=0; i<acct->numassets; i++)
    {
        if ( acct->assets[i]->assetbits == assetbits )  // need to dereference as hashtables get resized
            return(i);
    }
    if ( createflag != 0 )
    {
        if ( acct->numassets >= acct->maxassets )
        {
            acct->maxassets = acct->numassets + NXT_ASSETLIST_INCR;
            acct->txlists = realloc(acct->txlists,sizeof(*acct->txlists) * acct->maxassets);
            acct->assets = realloc(acct->assets,sizeof(*acct->assets) * acct->maxassets);
            acct->quantities = realloc(acct->quantities,sizeof(*acct->quantities) * acct->maxassets);
        }
        acct->assets[acct->numassets] = ap;
        acct->txlists[acct->numassets] = 0;
        acct->quantities[acct->numassets] = 0;
        return(acct->numassets++);
    }
    else return(-1);
}

struct NXT_assettxid_list *get_asset_txlist(struct NXThandler_info *mp,struct NXT_acct *np,char *assetid)
{
    int32_t ind,createdflag;
    struct NXT_asset *ap;
    if ( assetid == 0 || assetid[0] == 0 )
        return(0);
    //printf("get_asset_txlist\n");
    ap = MTadd_hashtable(&createdflag,mp->NXTassets_tablep,assetid);
    ind = get_asset_in_acct(np,ap,0);
    if ( ind > 0 )
        return(np->txlists[ind]);
    else return(0);
}

int32_t validate_nxtaddr(char *nxtaddr)
{
    // make sure it has publickey
    int32_t n = (int)strlen(nxtaddr);
    while ( n > 10 && (nxtaddr[n-1] == '\n' || nxtaddr[n-1] == '\r') )
        n--;
    if ( n < 1 )
        return(-1);
    return(0);
}

struct NXT_acct *add_NXT_acct(char *NXTaddr,struct NXThandler_info *mp,cJSON *obj)
{
    struct NXT_acct *ptr;
    int32_t createdflag;
    if ( obj != 0 )
    {
        copy_cJSON(NXTaddr,obj);
        ptr = get_NXTacct(&createdflag,mp,NXTaddr);
        if ( createdflag != 0 )
            return(ptr);
    }
    return(0);
}

struct NXT_assettxid *add_NXT_assettxid(struct NXT_asset **app,char *assetid,struct NXThandler_info *mp,cJSON *obj,char *txid)
{
    int32_t createdflag;
    struct NXT_asset *ap;
    struct NXT_assettxid *tp;
    if ( obj != 0 )
    {
        copy_cJSON(assetid,obj);
       // printf("add_NXT_assettxid A\n");
        *app = ap = MTadd_hashtable(&createdflag,mp->NXTassets_tablep,assetid);
        if ( createdflag != 0 )
            ap->assetbits = calc_nxt64bits(assetid);
        //printf("add_NXT_assettxid B\n");
        tp = MTadd_hashtable(&createdflag,mp->NXTasset_txids_tablep,txid);
        if ( createdflag != 0 )
        {
            tp->assetbits = calc_nxt64bits(assetid);
            tp->txidbits = calc_nxt64bits(txid);
            if ( ap->num >= ap->max )
            {
                ap->max = ap->num + NXT_ASSETLIST_INCR;
                ap->txids = realloc(ap->txids,sizeof(*ap->txids) * ap->max);
            }
            ap->txids[ap->num++] = tp;
            return(tp);
        }
    } else *app = 0;
    return(0);
}

int32_t addto_account_txlist(struct NXT_acct *acct,int32_t ind,struct NXT_assettxid *tp)
{
    if ( acct->txlists[ind] == 0 || acct->txlists[ind]->num >= acct->txlists[ind]->max )
    {
        if ( acct->txlists[ind] == 0 )
            acct->txlists[ind] = calloc(1,sizeof(*acct->txlists[ind]));
        acct->txlists[ind]->max = acct->txlists[ind]->num + NXT_ASSETLIST_INCR;
        acct->txlists[ind]->txids = realloc(acct->txlists[ind]->txids,sizeof(*acct->txlists[ind]->txids) * acct->txlists[ind]->max);
    }
    acct->txlists[ind]->txids[acct->txlists[ind]->num++] = tp;
    return(acct->txlists[ind]->num);
}

void *register_NXT_handler(char *name,struct NXThandler_info *mp,int32_t type,int32_t subtype,NXT_handler handler,uint32_t AMsigfilter,int32_t priority,char **assetlist,char **whitelist)
{
    struct NXT_protocol_parms PARMS;
    struct NXT_protocol *p = calloc(1,sizeof(*p));
    safecopy(p->name,name,sizeof(p->name));
    p->type = type; p->subtype = subtype;
    p->AMsigfilter = AMsigfilter;
    p->priority = priority;
    p->assetlist = assetlist; p->whitelist = whitelist;
    p->NXT_handler = handler;
    memset(&PARMS,0,sizeof(PARMS));
    PARMS.mode = NXTPROTOCOL_INIT;
    if ( Num_NXThandlers < (int32_t)(sizeof(NXThandlers)/sizeof(*NXThandlers)) )
    {
        printf("calling handlerinit.%s size.%d\n",name,Num_NXThandlers);
        p->handlerdata = (*handler)(mp,&PARMS,0);
        NXThandlers[Num_NXThandlers++] = p;
        printf("back handlerinit.%s size.%d\n",name,Num_NXThandlers);
    }
    return(p->handlerdata);
}

void call_handlers(struct NXThandler_info *mp,int32_t mode)
{
    int32_t i;
    struct NXT_protocol_parms PARMS;
    struct NXT_protocol *p;
    memset(&PARMS,0,sizeof(PARMS));
    PARMS.mode = mode;
    for (i=0; i<Num_NXThandlers; i++)
    {
        if ( (p= NXThandlers[i]) != 0 )
            (*p->NXT_handler)(mp,&PARMS,p->handlerdata);
    }
}

struct NXT_protocol *get_NXTprotocol(char *name)
{
    static struct NXT_protocol P;
    int32_t i;
    struct NXT_protocol *p;
    if ( name[0] == '/' )
        name++;
    if ( strcmp("NXTprotocol",name) == 0 )
    {
        strcpy(P.name,name);
        return(&P);
    }
    for (i=0; i<Num_NXThandlers; i++)
    {
        if ( (p= NXThandlers[i]) != 0 && strcmp(p->name,name) == 0 )
            return(p);
    }
    return(0);
}

char *NXTprotocol_json(cJSON *argjson)
{
    int32_t i,n=0;
    char *retstr;
    char numstr[512];
    struct NXT_protocol *p;
    cJSON *json,*array,*obj,*item;
    json = cJSON_CreateObject();
    array = cJSON_CreateArray();
    for (i=0; i<Num_NXThandlers; i++)
    {
        if ( (p= NXThandlers[i]) != 0 )
        {
            n++;
            item = cJSON_CreateObject();
            obj = cJSON_CreateString(p->name); cJSON_AddItemToObject(item,"name",obj);
            obj = cJSON_CreateNumber(p->type); cJSON_AddItemToObject(item,"type",obj);
            obj = cJSON_CreateNumber(p->subtype); cJSON_AddItemToObject(item,"subtype",obj);
            obj = cJSON_CreateNumber(p->priority); cJSON_AddItemToObject(item,"priority",obj);
            sprintf(numstr,"0x%08x",p->AMsigfilter);
            obj = cJSON_CreateString(numstr); cJSON_AddItemToObject(item,"AMsig",obj);
            if ( p->assetlist != 0 )
            {
                obj = gen_list_json(p->assetlist);
                cJSON_AddItemToObject(item,"assetlist",obj);
            }
            if ( p->whitelist != 0 )
            {
                obj = gen_list_json(p->whitelist);
                cJSON_AddItemToObject(item,"whitelist",obj);
            }
            cJSON_AddItemToArray(array,item);
        }
    }
    obj = cJSON_CreateNumber(n); cJSON_AddItemToObject(json,"numhandlers",obj);
    cJSON_AddItemToObject(json,"handlers",array);
    retstr = cJSON_Print(json);
    free_json(json);
    return(retstr);
}

char *NXTprotocol_json_handler(struct NXT_protocol *p,char *jsonstr)
{
    long len;
    cJSON *json;
    char *retjsontxt = 0;
    struct NXT_protocol_parms PARMS;
    //printf("NXTprotocol_json_handler.(%s)\n",jsonstr);
    if ( jsonstr != 0 )
    {
        json = cJSON_Parse(jsonstr);
        memset(&PARMS,0,sizeof(PARMS));
        PARMS.mode = NXTPROTOCOL_WEBJSON;
        PARMS.argjson = json;
        if ( strcmp(p->name,"NXTprotocol") == 0 )
            retjsontxt = NXTprotocol_json(json);
        else retjsontxt = (*p->NXT_handler)(Global_mp,&PARMS,p->handlerdata);
        //printf("retjsontxt.%p\n",retjsontxt);
        if ( json != 0 )
            free_json(json);
    }
    else if ( strcmp(p->name,"NXTprotocol") == 0 )
        retjsontxt = NXTprotocol_json(0);
    if ( retjsontxt != 0 )
    {
        //printf("got.(%s)\n",retjsontxt);
        len = strlen(retjsontxt);
        if ( len > p->retjsonsize )
            p->retjsontxt = realloc(p->retjsontxt,len+1);
        strcpy(p->retjsontxt,retjsontxt);
        free(retjsontxt);
        retjsontxt = p->retjsontxt;
    }
    else return("{\"result\":null}");
    return(retjsontxt);
}

int32_t process_NXT_event(struct NXThandler_info *mp,int32_t histmode,char *txid,int64_t type,int64_t subtype,struct NXT_AMhdr *AMhdr,char *sender,char *receiver,char *assetid,int64_t assetoshis,char *comment,cJSON *json)
{
    int32_t i,count,highest_priority;
    NXT_handler NXT_handler = 0;
    void *handlerdata = 0;
    struct NXT_protocol *p;
    struct NXT_protocol_parms PARMS;
    count = 0;
    highest_priority = -1;
    memset(&PARMS,0,sizeof(PARMS));
    if ( Num_NXThandlers == 0 )
        printf("WARNING: process_NXT_event Num_NXThandlers.%d\n",Num_NXThandlers);
    for (i=0; i<Num_NXThandlers; i++)
    {
        if ( (p= NXThandlers[i]) != 0 )
        {
            //if ( assetid != 0 && strcmp(assetid,DOGE_COINASSET) == 0 )
            //printf("type.%ld subtype.%ld vs (%d %d) AMhdr.%p\n",(long)type,(long)subtype,p->type,p->subtype,AMhdr);
            if ( AMhdr != 0 || ((p->type < 0 || p->type == type) && (p->subtype < 0 || p->subtype == subtype)) )
            {
                //if ( assetid != 0 && strcmp(assetid,DOGE_COINASSET) == 0 )
                //    printf("assetid.%p assetlist.%p\n",assetid,p->assetlist);
                if ( AMhdr != 0 )
                {
                    //printf("%x vs %x nxt.%lx\n",p->AMsigfilter,AMhdr->sig,(long)AMhdr->nxt64bits);
                    if ( p->AMsigfilter != 0 && p->AMsigfilter != AMhdr->sig )
                        continue;
                    //if ( strcmp(sender,NXTISSUERACCT) == 0 )
                    //    expand_nxt64bits(sender,AMhdr->nxt64bits);
                }
                else if ( p->assetlist != 0 && assetid != 0 && listcmp(p->assetlist,assetid) != 0 )
                    continue;
               //if ( assetid != 0 && strcmp(assetid,DOGE_COINASSET) == 0 )
               //     printf("AMhdr.%p whitelist.%p listcmp %d %s receiver.%s\n",AMhdr,p->whitelist,listcmp(p->whitelist,sender),sender,receiver);
                if ( strcmp(sender,GENESISACCT) == 0 || strcmp(receiver,GENESISACCT) == 0 || //is_gateway_addr(sender) != 0 ||
                     (AMhdr == 0 || p->whitelist == 0 || listcmp(p->whitelist,sender) == 0) ||
                     (AMhdr != 0 && cmp_nxt64bits(sender,AMhdr->nxt64bits) == 0) )
                {
                    if ( assetid != 0 && strcmp(assetid,DOGE_COINASSET) == 0 )
                       printf("priority.%d vs %d assetoshis %.8f (%s)\n",p->priority,highest_priority,dstr(assetoshis),comment);
                    if ( p->priority > highest_priority )
                    {
                        count = 1;
                        highest_priority = p->priority;
                        NXT_handler = p->NXT_handler;
                        handlerdata = p->handlerdata;
                        PARMS.argjson = json; PARMS.txid = txid; PARMS.sender = sender; PARMS.receiver = receiver;
                        PARMS.type = (int32_t)type; PARMS.subtype = (int32_t)subtype; PARMS.priority = highest_priority; PARMS.histflag = histmode;
                        if ( AMhdr != 0 )
                        {
                            PARMS.mode = NXTPROTOCOL_AMTXID;
                            PARMS.AMptr = AMhdr;
                        }
                        else
                        {
                            PARMS.mode = NXTPROTOCOL_TYPEMATCH;
                            PARMS.assetid = assetid;
                            PARMS.assetoshis = assetoshis;
                            PARMS.comment = comment;
                        }
                        //printf("call NXT_handler\n");
                        (*NXT_handler)(mp,&PARMS,handlerdata);
                   }
                    else if ( p->priority == highest_priority )
                        count++;
                }
                //printf("end iter\n");
            }
        }
    }
    if ( count > 1 )
        printf("WARNING: (%s) claimed %d times, priority.%d\n",cJSON_Print(json),count,highest_priority);  // this is bad, also leaks mem!
    if ( 0 && highest_priority >= 0 && NXT_handler != 0 )
    {
        //printf("count.%d handler.%p NXThandler_info_handler.%p AMhdr.%p\n",count,NXT_handler,multigateway_handler,AMhdr);
        PARMS.argjson = json; PARMS.txid = txid; PARMS.sender = sender; PARMS.receiver = receiver;
        PARMS.type = (int32_t)type; PARMS.subtype = (int32_t)subtype; PARMS.priority = highest_priority; PARMS.histflag = histmode;
        if ( AMhdr != 0 )
        {
            PARMS.mode = NXTPROTOCOL_AMTXID;
            PARMS.AMptr = AMhdr;
        }
        else
        {
            PARMS.mode = NXTPROTOCOL_TYPEMATCH;
            PARMS.assetid = assetid;
            PARMS.assetoshis = assetoshis;
            PARMS.comment = comment;
        }
        (*NXT_handler)(mp,&PARMS,handlerdata);///histmode,txid,AMhdr,sender,receiver,assetid,assetoshis,comment,json);
    }
    return(count != 0);
}

void transfer_asset_balance(struct NXThandler_info *mp,struct NXT_assettxid *tp,uint64_t assetbits,char *sender,char *receiver,int64_t quantity)
{
    int32_t createdflag,srcind,destind;
    struct NXT_acct *src,*dest;
    struct NXT_asset *ap;
    char assetid[MAX_NXTADDR_LEN];
    if ( sender == 0 || sender[0] == 0 || receiver == 0 || receiver[0] == 0 )
    {
        printf("illegal transfer asset (%s -> %s)\n",sender,receiver);
        return;
    }
    expand_nxt64bits(assetid,assetbits);
    //printf("transfer_asset_balance\n");
    ap = MTadd_hashtable(&createdflag,mp->NXTassets_tablep,assetid);
    if ( createdflag != 0 )
        printf("transfer_asset_balance: unexpected hashtable creation??\n");
    src = get_NXTacct(&createdflag,mp,sender);
    srcind = get_asset_in_acct(src,ap,1);
    dest = get_NXTacct(&createdflag,mp,receiver);
    destind = get_asset_in_acct(dest,ap,1);
    if ( srcind >= 0 && destind >= 0 )
    {
        src->quantities[srcind] -= quantity;
        addto_account_txlist(src,srcind,tp);
        dest->quantities[destind] += quantity;
        addto_account_txlist(dest,destind,tp);
    } else printf("error finding inds for assetid.%s %s.%d %s.%d\n",assetid,sender,srcind,receiver,destind);
}

int32_t process_NXTtransaction(struct NXThandler_info *mp,int32_t histmode,char *txid)
{
    char cmd[4096],sender[64],receiver[64],assetid[MAX_NXTADDR_LEN],*assetidstr,*commentstr = 0;
    cJSON *senderobj,*attachment,*message,*assetjson,*hashobj,*commentobj;
    unsigned char buf[4096];
    char AMstr[4096],comment[1024];
    struct NXT_AMhdr *hdr;
    struct NXT_guid *gp;
    struct NXT_asset *ap;
    struct NXT_assettxid *tp;
    int32_t createdflag,processed = 0;
    int64_t type,subtype,n,assetoshis = 0;
    union NXTtype retval;
    memset(assetid,0,sizeof(assetid));
    sprintf(cmd,"%s=getTransaction&transaction=%s",NXTSERVER,txid);
    retval = extract_NXTfield(0,cmd,0,0);
    if ( retval.json != 0 )
    {
        //printf("%s\n",cJSON_Print(retval.json));
        hdr = 0; assetidstr = 0;
        sender[0] = receiver[0] = 0;
        hashobj = cJSON_GetObjectItem(retval.json,"hash");
        if ( hashobj != 0 )
        {
            copy_cJSON((char *)buf,hashobj);
            //printf("guid add hash.%s %p %p\n",(char *)buf,mp->NXTguid_tablep,mp->NXTguid_tablep);
            gp = MTadd_hashtable(&createdflag,mp->NXTguid_tablep,(char *)buf);
            if ( createdflag != 0 )
                safecopy(gp->H.txid,txid,sizeof(gp->H.txid));
            else
            {
                printf("duplicate transaction hash??: %s already there, new tx.%s both hash.%s | Probably from history thread\n",gp->H.txid,txid,gp->guid);
                //mark_tx_hashconflict(mp,histmode,txid,gp);
                free_json(retval.json);
                return(0);
            }
        }
        type = get_cJSON_int(retval.json,"type");
        subtype = get_cJSON_int(retval.json,"subtype");

        senderobj = cJSON_GetObjectItem(retval.json,"sender");
        if ( senderobj == 0 )
            senderobj = cJSON_GetObjectItem(retval.json,"accountId");
        else if ( senderobj == 0 )
            senderobj = cJSON_GetObjectItem(retval.json,"account");
        add_NXT_acct(sender,mp,senderobj);
        add_NXT_acct(receiver,mp,cJSON_GetObjectItem(retval.json,"recipient"));

        attachment = cJSON_GetObjectItem(retval.json,"attachment");
        if ( attachment != 0 )
        {
            message = cJSON_GetObjectItem(attachment,"message");
            if ( message != 0 )
            {
                copy_cJSON(AMstr,message);
                //printf("AM message.(%s).%ld\n",AMstr,strlen(AMstr));
                n = strlen(AMstr);
                if ( (n&1) != 0 || n > 2000 )
                    printf("warning: odd message len?? %ld\n",(long)n);
                decode_hex((void *)buf,(int32_t)(n>>1),AMstr);
                hdr = (struct NXT_AMhdr *)buf;
                //printf("txid.%s NXT.%s -> NXT.%s (%s)\n",txid,sender,receiver,((struct json_AM *)buf)->jsonstr);
            }
            else if ( (assetjson= cJSON_GetObjectItem(attachment,"asset")) != 0 )
            {
                tp = add_NXT_assettxid(&ap,assetid,mp,assetjson,txid);
                commentobj = cJSON_GetObjectItem(attachment,"comment");
                copy_cJSON(comment,commentobj);
               // printf("assetid.%s comment.%s (%s)\n",assetid,comment,cJSON_Print(attachment));
                if ( tp != 0 && type == 2 )
                {
                    if ( comment[0] != 0 )
                        commentstr = tp->comment = clonestr(comment);
                    tp->quantity = get_cJSON_int(attachment,"quantityQNT");
                    //printf("quantity.%ld\n",(long)tp->quantity);
                    assetoshis = tp->quantity;
                    switch ( subtype )
                    {
                        case 0:
                            if ( strcmp(receiver,GENESISACCT) == 0 )
                            {
                                tp->senderbits = calc_nxt64bits(GENESISACCT);
                                tp->receiverbits = calc_nxt64bits(sender);
                                transfer_asset_balance(mp,tp,tp->assetbits,GENESISACCT,sender,tp->quantity);
                            }
                            else printf("non-GENESIS sender on Issuer Asset?\n");
                            break;
                        case 1:
                            tp->senderbits = calc_nxt64bits(sender);
                            tp->receiverbits = calc_nxt64bits(receiver);

                            transfer_asset_balance(mp,tp,tp->assetbits,sender,receiver,tp->quantity);
                            break;
                        case 2:
                        case 3: // bids and asks, no indication they are filled at this point, so nothing to do
                            break;
                    }
                }
                else add_NXT_assettxid(&ap,assetid,mp,assetjson,txid);
                assetidstr = assetid;
            }
        }
        //printf("call process event hdr.%p %x\n",hdr,hdr->sig);
        //printf("hist.%d before handler init_NXThashtables: %p %p %p %p\n",mp->histmode,mp->NXTguid_tablep,mp->NXTaccts_tablep,mp->NXTassets_tablep, mp->NXTasset_txids_tablep);
        processed += process_NXT_event(mp,histmode,txid,type,subtype,hdr,sender,receiver,assetidstr,assetoshis,commentstr,retval.json);
       // printf("hist.%d after handler init_NXThashtables: %p %p %p %p\n",mp->histmode,mp->NXTguid_tablep,mp->NXTaccts_tablep,mp->NXTassets_tablep, mp->NXTasset_txids_tablep);
        free_json(retval.json);
    }
    else printf("unexpected error iterating (%s) txid.(%s)\n",cmd,txid);
    return(processed);
}

int32_t process_NXTblock(int32_t *heightp,char *nextblock,struct NXThandler_info *mp,int32_t histmode,char *blockidstr)
{
    char cmd[4096],txid[1024];
    cJSON *transactions,*nextjson;
    int64_t n;
    union NXTtype retval;
    int32_t i,timestamp=0,errcode,processed = 0;
    *heightp = 0;
    sprintf(cmd,"%s=getBlock&block=%s",NXTSERVER,blockidstr);
   // printf("cmd.(%s)\n",cmd);
    retval = extract_NXTfield(0,cmd,0,0);
    if ( retval.json != 0 )
    {
        //printf("%s\n",cJSON_Print(retval.json));
        errcode = (int32_t)get_cJSON_int(retval.json,"errorCode");
        if ( errcode == 0 )
        {
            *heightp = (int32_t)get_cJSON_int(retval.json,"height");
            timestamp = (int32_t)get_cJSON_int(retval.json,"timestamp");
            nextjson = cJSON_GetObjectItem(retval.json,"nextBlock");
            if ( nextjson != 0 )
                copy_cJSON(nextblock,nextjson);
            else nextblock[0] = 0;
            transactions = cJSON_GetObjectItem(retval.json,"transactions");
            n = get_cJSON_int(retval.json,"numberOfTransactions");
            if ( n != cJSON_GetArraySize(transactions) )
                printf("JSON parse error!! %ld != %d\n",(long)n,cJSON_GetArraySize(transactions));
            if ( n != 0 )
            {
                for (i=0; i<n; i++)
                {
                    copy_cJSON(txid,cJSON_GetArrayItem(transactions,i));
                    //printf("hist.%d i.%d of %ld process NXTtxid init_NXThashtables: %p %p %p %p\n",mp->histmode,i,(long)n,mp->NXTguid_tablep,mp->NXTaccts_tablep,mp->NXTassets_tablep, mp->NXTasset_txids_tablep);
                    if ( txid[0] != 0 )
                    {
                        //printf("%s ",txid);
                        processed += process_NXTtransaction(mp,histmode,txid);
                    }
                }
                //printf("n.%ld\n",(long)n);
            }
        }
        free_json(retval.json);
    }
    return(timestamp);
}

int32_t NXTnetwork_healthy(struct NXThandler_info *mp)
{
#ifdef DEBUG_MODE
    int32_t minconfirms = 1;
#else
    int32_t minconfirms = MIN_NXTCONFIRMS;
#endif
    if ( mp->fractured_prob > .01 )
        return(-1);
    if ( mp->RTflag >= minconfirms && mp->deadman < (60/mp->pollseconds)+1 )
        return(1);
    else return(0);
}

void *NXTloop(void *ptr)
{
    struct NXThandler_info *mp = ptr;
    int32_t i,height,histmode=0,numforging = 0;
    char nextblock[64],terminationblock[64];
    numforging = set_current_NXTblock(terminationblock);
    printf("NXTloop: %s\n",terminationblock);
    //printf("hist.%d NXtloop init_NXThashtables: %p %p %p %p\n",mp->histmode,mp->NXTguid_tablep,mp->NXTaccts_tablep,mp->NXTassets_tablep, mp->NXTasset_txids_tablep);
    if ( mp->origblockidstr != 0 )
    {
        histmode = 1;
        safecopy(nextblock,mp->origblockidstr,sizeof(nextblock));
        printf("HIST.%s -> %s\n",mp->blockidstr,terminationblock);
    }
    else
    {
        safecopy(nextblock,terminationblock,sizeof(nextblock));
        terminationblock[0] = 0;
        printf("start RT NXTloop numforging.%d from (%s)\n",numforging,nextblock);
    }
    while ( terminationblock[0] == 0 || strcmp(nextblock,terminationblock) != 0 )
    {
        //printf("hist.%d iter init_NXThashtables: %p %p %p %p\n",mp->histmode,mp->NXTguid_tablep,mp->NXTaccts_tablep,mp->NXTassets_tablep, mp->NXTasset_txids_tablep);
        safecopy(mp->lastblock,mp->blockidstr,sizeof(mp->lastblock));
        safecopy(mp->blockidstr,nextblock,sizeof(mp->blockidstr));
        if ( mp->blockidstr[0] != 0 )
        {
            mp->timestamp = process_NXTblock(&height,nextblock,mp,histmode,mp->blockidstr);
            if ( height > mp->NXTheight )
                mp->NXTheight = height;
            printf("NXT.%d %s timestamp.%d lag.%d NEW block.(%s) vs lastblock.(%s) -> (%s)\n",height,histmode?"HIST":"",mp->timestamp,issue_getTime()-mp->timestamp,mp->blockidstr,mp->lastblock,nextblock);
            if ( histmode == 0 )
            {
                mp->deadman = 0;
                if ( Historical_done != 0 )
                {
                    call_handlers(mp,NXTPROTOCOL_NEWBLOCK);
                    mp->RTflag++;   // wait for first block before doing any side effects
                }
            }
            else if ( strcmp(nextblock,mp->lastblock) == 0 )
                break;
        }
        if ( histmode == 0 )
        {
            mp->deadman++;
            do
            {
                //printf("hist.%d do while init_NXThashtables: %p %p %p %p\n",mp->histmode,mp->NXTguid_tablep,mp->NXTaccts_tablep,mp->NXTassets_tablep, mp->NXTasset_txids_tablep);
                if ( mp->upollseconds != 0 || NXTnetwork_healthy(mp) > 0 )
                {
                    if ( mp->upollseconds != 0 )
                    {
                        for (i=0; i<10; i++)
                        {
                            usleep(mp->upollseconds);
                            call_handlers(mp,NXTPROTOCOL_IDLETIME);
                        }
                    }
                    else
                    {
                        sleep(mp->pollseconds);
                        call_handlers(mp,NXTPROTOCOL_IDLETIME);
                    }
                    gen_testforms();
                }
                set_next_NXTblock(nextblock,mp->blockidstr);
            }
            while ( nextblock[0] == 0 || strcmp(nextblock,mp->blockidstr) == 0 );
        }
    }
    if ( histmode != 0 )
    {
        mp = Global_mp;
        printf("HIST Waiting for hashtable queue to catchup: %ld\n",(long)time(0));
        while ( queue_size(&mp->hashtable_queue) != 0 )
            usleep(1);
        printf("HIST Historical processing has finished: %ld\n",(long)time(0));
        Historical_done = 1;
        while ( 1 ) sleep(1);
    }
    return(0);
}

void init_NXThashtables(struct NXThandler_info *mp)
{
    struct NXT_acct *np = 0;
    struct NXT_asset *ap = 0;
    struct NXT_assettxid *tp = 0;
    struct NXT_guid *gp = 0;
    static struct hashtable *NXTasset_txids,*NXTaddrs,*NXTassets,*NXTguids;
    if ( NXTguids == 0 )
        NXTguids = hashtable_create("NXTguids",HASHTABLES_STARTSIZE,sizeof(struct NXT_guid),((long)&gp->guid[0] - (long)gp),sizeof(gp->guid),((long)&gp->H.modified - (long)gp));
    if ( NXTasset_txids == 0 )
        NXTasset_txids = hashtable_create("NXTasset_txids",HASHTABLES_STARTSIZE,sizeof(struct NXT_assettxid),((long)&tp->H.txid[0] - (long)tp),sizeof(tp->H.txid),((long)&tp->H.modified - (long)tp));
    if ( NXTassets == 0 )
        NXTassets = hashtable_create("NXTassets",HASHTABLES_STARTSIZE,sizeof(struct NXT_asset),((long)&ap->H.assetid[0] - (long)ap),sizeof(ap->H.assetid),((long)&ap->H.modified - (long)ap));
    if ( NXTaddrs == 0 )
        NXTaddrs = hashtable_create("NXTaddrs",HASHTABLES_STARTSIZE,sizeof(struct NXT_acct),((long)&np->H.NXTaddr[0] - (long)np),sizeof(np->H.NXTaddr),((long)&np->H.modified - (long)np));
    if ( mp != 0 )
    {
        mp->NXTguid_tablep = &NXTguids;
        mp->NXTaccts_tablep = &NXTaddrs;
        mp->NXTassets_tablep = &NXTassets;
        mp->NXTasset_txids_tablep = &NXTasset_txids;
        printf("hist.%d init_NXThashtables: %p %p %p %p\n",mp->histmode,NXTguids,NXTaddrs,NXTassets,NXTasset_txids);
    }
}

void start_NXTloops(struct NXThandler_info *mp,char *histstart)
{
    static struct NXThandler_info histM;
    //mp->RTmp = mp;
    //printf("start_NXTloops\n");
    init_NXThashtables(mp);
    if ( pthread_create(malloc(sizeof(pthread_t)),NULL,process_hashtablequeues,mp) != 0 )
        printf("ERROR hist process_hashtablequeues\n");
    if ( histstart != 0 && histstart[0] != 0 )
    {
        histM = *mp;
        histM.origblockidstr = histstart;
        histM.histmode = 1;
        init_NXThashtables(&histM);
        if ( pthread_create(malloc(sizeof(pthread_t)),NULL,NXTloop,&histM) != 0 )
            printf("ERROR start_Histloop\n");
    } else Historical_done = 1;
    if ( pthread_create(malloc(sizeof(pthread_t)),NULL,NXTloop,mp) != 0 )
        printf("ERROR NXTloop\n");
    gen_testforms();

    //printf("after start init_NXThashtables: %p %p %p %p\n",mp->NXTguid_tablep,mp->NXTaccts_tablep,mp->NXTassets_tablep, mp->NXTasset_txids_tablep);
 //   NXTloop(mp);
}
