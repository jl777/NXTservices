//
//  NXTorrent.h
//  Created by jl777, April 13th 2014
//  MIT License
//

#ifndef gateway_NXTorrent_h
#define gateway_NXTorrent_h


#define NXTORRENT_SIG 0x48573945

struct NXTorrent_info
{
    char ipaddr[32],NXTADDR[32],NXTACCTSECRET[512];
    struct hashtable *listings;
};
struct NXTorrent_info *Global_NXTorrent;

#define NXTORRENT_LIST 'L'
#define NXTORRENT_FEEDBACK 'F'
#define NXTORRENT_CANCEL 'X'
#define NXTORRENT_MAKEOFFER 'O'
#define NXTORRENT_ACCEPT 'A'

#define NXTORRENT_INACTIVE 0
#define NXTORRENT_ACTIVE 1
#define NXTORRENT_CANCELLED 2
#define NXTORRENT_OFFERED 3
#define NXTORRENT_ACCEPTED 4
#define NXTORRENT_INTRANSIT 5
#define NXTORRENT_DISPUTED 6
#define NXTORRENT_COMPLETED 7
#define NXTORRENT_BUYERFLAG 0x1000
#define NXTORRENT_SELLERFLAG 0x2000

#define NXTORRENT_NOLISTING "{\"error\":\"no listingid\"}"


struct NXTorrent_listing
{
    uint64_t modified,seller64bits,buyer64bits,listingAMbits,price;
    int32_t status,listedtime;
    char listingid[64],*jsontxt,*URI;
};

int32_t passes_filter(struct NXTorrent_listing *lp,cJSON *filter)
{
    int32_t i,n;
    cJSON *obj;
    if ( filter != 0 && (filter->type&0xff) == cJSON_Array )
    {
        n = cJSON_GetArraySize(filter);
        for (i=0; i<n; i++)
        {
            obj = cJSON_GetArrayItem(filter,i);
        }
    }
    return(1);
}

char *NXTorrent_status_str(int32_t status)
{
    switch ( (status&7) )
    {
        case 0: return("inactive listing"); break;
        case NXTORRENT_ACTIVE: return("active listing"); break;
        case NXTORRENT_CANCELLED: return("cancelled listing"); break;
        case NXTORRENT_OFFERED: return("offer pending"); break;
        case NXTORRENT_ACCEPTED: return("offer accepted"); break;
        case NXTORRENT_INTRANSIT: return("goods in transit"); break;
        case NXTORRENT_DISPUTED: return("disputed listing"); break;
        case NXTORRENT_COMPLETED: return("completed listing"); break;
    }
    return("invalid listingid");
}

char *standard_retstr(char *retstr)
{
    cJSON *json;
    if ( retstr != 0 )
    {
        json = cJSON_CreateObject();
        cJSON_AddItemToObject(json,"result",cJSON_CreateString("good"));
        cJSON_AddItemToObject(json,"AMtxid",cJSON_CreateString(retstr));
        free(retstr);
        retstr = cJSON_Print(json);
        free_json(json);
    }
    return(retstr);
}

char *AM_NXTorrent(int32_t func,int32_t timestamp,char *nxtaddr,char *jsontxt)
{
    char AM[4096],*retstr;
    struct json_AM *ap = (struct json_AM *)AM;
    printf("func.(%c) AM_NXTorrent(%s)\n",func,jsontxt);
    set_json_AM(ap,NXTORRENT_SIG,func,nxtaddr,timestamp,jsontxt,1);
    retstr = submit_AM(&ap->H,0);
    return(retstr);
}

struct NXTorrent_listing *get_listingid(char *listingid)
{
    struct NXTorrent_info *gp = Global_NXTorrent;
    uint64_t hashval;
    hashval = search_hashtable(gp->listings,listingid);
    if ( hashval == HASHSEARCH_ERROR )
        return(0);
    else return(gp->listings->hashtable[hashval]);
}

int32_t get_listing_status(char *listingid)
{
    struct NXTorrent_listing *lp;
    if ( (lp = get_listingid(listingid)) != 0 )
        return(lp->status);
    return(-1);
}

cJSON *gen_listing_json(struct NXTorrent_listing *lp)
{
    cJSON *json;
    char seller[64],buyer[64],numstr[64];
    json = cJSON_CreateObject();    
    expand_nxt64bits(seller,lp->seller64bits);
    expand_nxt64bits(buyer,lp->buyer64bits);
    cJSON_AddItemToObject(json,"seller",cJSON_CreateString(seller));
    cJSON_AddItemToObject(json,"listingid",cJSON_CreateString(lp->listingid));
    sprintf(numstr,"%.8f",dstr(lp->price));
    cJSON_AddItemToObject(json,"price",cJSON_CreateString(numstr));
    cJSON_AddItemToObject(json,"status",cJSON_CreateString(NXTorrent_status_str(lp->status)));
    cJSON_AddItemToObject(json,"listedtime",cJSON_CreateNumber(lp->listedtime));
    if ( buyer[0] != 0 )
        cJSON_AddItemToObject(json,"buyer",cJSON_CreateString(buyer));
    if ( lp->URI != 0 )
        cJSON_AddItemToObject(json,"URI",cJSON_CreateString(lp->URI));
    if ( lp->jsontxt != 0 )
        cJSON_AddItemToObject(json,"listing",cJSON_CreateString(lp->jsontxt));
    if ( lp->listingAMbits != 0 )
    {
        expand_nxt64bits(numstr,lp->listingAMbits);
        cJSON_AddItemToObject(json,"listingAMtxid",cJSON_CreateString(numstr));
    }
    return(json);
}

char *status_func(char *sender,int32_t valid,cJSON **objs,int32_t numobjs)
{
    char NXTaddr[64],listingid[64],*retstr = 0;
    struct NXTorrent_listing *lp;
    cJSON *json;
    copy_cJSON(NXTaddr,objs[0]);
    copy_cJSON(listingid,objs[1]);
    if ( NXTaddr[0] != 0 && listingid[0] != 0 )
    {
        if ( (lp = get_listingid(listingid)) != 0 )
        {
            json = gen_listing_json(lp);
            if ( json != 0 )
            {
                retstr = cJSON_Print(json);
                free_json(json);
                return(retstr);
            }
        }
    }
    retstr = clonestr(NXTORRENT_NOLISTING);
    return(retstr);
}

char *cancel_func(char *sender,int32_t valid,cJSON **objs,int32_t numobjs)
{
    char NXTaddr[64],listingid[64],buf[512],*retstr = 0;
    struct NXTorrent_listing *lp;
    copy_cJSON(NXTaddr,objs[0]);
    copy_cJSON(listingid,objs[1]);
    if ( NXTaddr[0] != 0 && listingid[0] != 0 )
    {
        if ( (lp = get_listingid(listingid)) != 0 )
        {
            if ( lp->status == NXTORRENT_OFFERED || lp->status == NXTORRENT_ACTIVE )
            {
                lp->status = NXTORRENT_CANCELLED;
                sprintf(buf,"{\"cancelled\":\"%s\"}",listingid);
                retstr = AM_NXTorrent(NXTORRENT_CANCEL,Global_mp->timestamp,NXTaddr,buf);
                retstr = standard_retstr(retstr);
            }
            else
            {
                sprintf(buf,"{\"error\":\"cant cancel\",\"reason\":\"%s\"}",NXTorrent_status_str(get_listing_status(listingid)));
                retstr = clonestr(buf);
            }
        }
        else retstr = clonestr(NXTORRENT_NOLISTING);
    }
    return(retstr);
}

char *listings_func(char *sender,int32_t valid,cJSON **objs,int32_t numobjs)
{
    char NXTaddr[64],*retstr = 0;
    struct NXTorrent_listing **all,*lp;
    int64_t i,num;
    cJSON *array;
    copy_cJSON(NXTaddr,objs[0]);
    if ( NXTaddr[0] != 0 )
    {
        all = (struct NXTorrent_listing **)hashtable_gather_modified(&num,Global_NXTorrent->listings,1);
        if ( all != 0 && num != 0 )
        {
            printf("num.%d\n",(int)num);
            array = cJSON_CreateArray();
            for (i=0; i<num; i++)
            {
                lp = all[i];
                if ( passes_filter(lp,objs[1]) > 0 )
                {
                    printf("add %s\n",lp->listingid);
                    cJSON_AddItemToArray(array,cJSON_CreateString(lp->listingid));
                }
            }
            free(all);
            retstr = cJSON_Print(array);
            free_json(array);
        }
        else retstr = clonestr(NXTORRENT_NOLISTING);
    }
    return(retstr);
}

char *list_func(char *sender,int32_t valid,cJSON **objs,int32_t numobjs)
{
    char token[NXT_TOKEN_LEN+1],numstr[128],hashvalstr[64],NXTaddr[64],URI[512],buf[4096],*str,*jsontxt,*retstr = 0;
    struct NXTorrent_listing *lp;
    int32_t createdflag,i,n;
    uint64_t hashval,price = 0;
    cJSON *URIobj,*json,*item,*priceobj,*array = objs[1];
    copy_cJSON(NXTaddr,objs[0]);
    if ( NXTaddr[0] != 0 && numobjs > 1 && array != 0 )
    {
        if ( (array->type & 0xff) == cJSON_Array )
        {
            n = cJSON_GetArraySize(array);
            for (i=0; i<n; i++)
            {
                item = cJSON_GetArrayItem(array,i);
                URIobj = cJSON_GetObjectItem(item,"URI");
                if ( URIobj != 0 )
                    copy_cJSON(URI,URIobj);
                priceobj = cJSON_GetObjectItem(item,"price");
                copy_cJSON(numstr,priceobj);
                if ( numstr[0] != 0 )
                    price = (uint64_t)(atof(numstr) * SATOSHIDEN);
                printf("i.%d of %d: %p %p %s %f\n",i,n,URIobj,priceobj,numstr,atof(numstr));
            }
            
        }
        copy_cJSON(buf,objs[1]);
        stripwhite(buf,strlen(buf));
        jsontxt = clonestr(buf);
        issue_generateToken(token,buf,Global_NXTorrent->NXTACCTSECRET);
        token[NXT_TOKEN_LEN] = 0;
        sprintf(buf,"%s%s",token,NXTaddr);
        hashval = calc_decimalhash(buf);
        expand_nxt64bits(hashvalstr,hashval);
        lp = add_hashtable(&createdflag,&Global_NXTorrent->listings,hashvalstr);
        if ( createdflag != 0 )
        {
            if ( lp != 0 )
            {
                printf("create new listing\n");
                lp->seller64bits = calc_nxt64bits(NXTaddr);
                lp->listedtime = Global_mp->timestamp;
                lp->status = NXTORRENT_ACTIVE;
                lp->jsontxt = jsontxt;
                lp->price = price;
                if ( URI[0] != 0 )
                    lp->URI = clonestr(URI);
                json = gen_listing_json(lp);
                if ( json != 0 )
                {
                    retstr = cJSON_Print(json);
                    str = AM_NXTorrent(NXTORRENT_LIST,Global_mp->timestamp,NXTaddr,retstr);
                    if ( str == 0 )
                        retstr = clonestr("{\"error\":\"couldnt create listing AM\"}");
                    else
                    {
                        lp->listingAMbits = calc_nxt64bits(str);
                        free(str);
                    }
                }
                else retstr = clonestr("{\"error\":\"unexpected JSON parse error\"}");
            }
        }
        else retstr = clonestr("{\"error\":\"duplicate listing\"}");
    }
    return(retstr);
}

char *makeoffer_func(char *sender,int32_t valid,cJSON **objs,int32_t numobjs)
{
    char NXTaddr[64],listingid[64],errstr[513],bid[512],*retstr = 0;
    struct NXTorrent_listing *lp;
    copy_cJSON(NXTaddr,objs[0]);
    copy_cJSON(listingid,objs[1]);
    copy_cJSON(bid,objs[2]);
    if ( NXTaddr[0] != 0 && listingid[0] != 0 )
    {
        lp = get_listingid(listingid);
        if ( lp != 0 )
        {
            if ( lp->status == NXTORRENT_ACTIVE )
            {
                lp->status = NXTORRENT_OFFERED;
                lp->buyer64bits = calc_nxt64bits(NXTaddr);
                retstr = AM_NXTorrent(NXTORRENT_MAKEOFFER,Global_mp->timestamp,NXTaddr,bid);
                retstr = standard_retstr(retstr);
            }
            else
            {
                sprintf(errstr,"{\"error\":\"cant make offer\",\"reason\":\"%s\"}",NXTorrent_status_str(lp->status));
                retstr = clonestr(errstr);
            }
        } else retstr = clonestr(NXTORRENT_NOLISTING);
    }
    return(retstr);
}

char *acceptoffer_func(char *sender,int32_t valid,cJSON **objs,int32_t numobjs)
{
    char NXTaddr[64],buyer[64],listingid[64],buf[512],*retstr = 0;
    struct NXTorrent_listing *lp;
    copy_cJSON(NXTaddr,objs[0]);
    copy_cJSON(listingid,objs[1]);
    copy_cJSON(buyer,objs[2]);
    if ( NXTaddr[0] != 0 && listingid[0] != 0 && buyer[0] != 0 )
    {
        lp = get_listingid(listingid);
        if ( lp != 0 )
        {
            lp->buyer64bits = calc_nxt64bits(buyer);
            lp->status = NXTORRENT_ACCEPTED;
            sprintf(buf,"{\"accept\":\"%s\",\"buyer\":\"%s\"}",listingid,buyer);
            retstr = AM_NXTorrent(NXTORRENT_ACCEPT,Global_mp->timestamp,NXTaddr,buf);
            retstr = standard_retstr(retstr);
        } else retstr = clonestr(NXTORRENT_NOLISTING);
    }
    return(retstr);
}

char *feedback_func(char *sender,int32_t valid,cJSON **objs,int32_t numobjs)
{
    uint64_t nxt64bits;
    char NXTaddr[64],listingid[64],aboutbuyer[1024],aboutseller[1024],*str,*retstr = 0;
    struct NXTorrent_listing *lp;
    copy_cJSON(NXTaddr,objs[0]);
    copy_cJSON(listingid,objs[1]);
    copy_cJSON(aboutbuyer,objs[2]);
    copy_cJSON(aboutseller,objs[3]);
    if ( NXTaddr[0] != 0 && listingid[0] != 0 && (aboutbuyer[0] != 0 || aboutseller[0] != 0) )
    {
        lp = get_listingid(listingid);
        if ( lp != 0 )
        {
            nxt64bits= calc_nxt64bits(NXTaddr);
            if ( lp->buyer64bits == nxt64bits )
            {
                if ( aboutbuyer[0] != 0 || aboutseller[0] == 0 )
                    return(clonestr("{\"error\":\"buyer can only feedback seller\"}"));
                else str = aboutseller;
                lp->status |= NXTORRENT_BUYERFLAG;
            }
            else if ( lp->seller64bits == nxt64bits )
            {
                if ( aboutbuyer[0] == 0 || aboutseller[0] != 0 )
                    return(clonestr("{\"error\":\"seller can only feedback buyer\"}"));
                else str = aboutbuyer;
                lp->status |= NXTORRENT_SELLERFLAG;
            }
            else return(clonestr("{\"error\":\"neither buyer nor seller\"}"));
            retstr = AM_NXTorrent(NXTORRENT_FEEDBACK,Global_mp->timestamp,NXTaddr,str);
            retstr = standard_retstr(retstr);
        } else retstr = clonestr(NXTORRENT_NOLISTING);
    }
    return(retstr);
}

char *changeurl_func(char *sender,int32_t valid,cJSON **objs,int32_t numobjs)
{
    extern char NXTPROTOCOL_HTMLFILE[512];
    char URL[64],*retstr = 0;
    copy_cJSON(URL,objs[0]);
    if ( URL[0] != 0 )
    {
        strcpy(NXTPROTOCOL_HTMLFILE,URL);
        retstr = clonestr(URL);
    }
    return(retstr);
}

char *NXTorrent_json_commands(struct NXTorrent_info *gp,cJSON *argjson,char *sender,int32_t valid)
{
    static char *changeurl[] = { (char *)changeurl_func, "changeurl", "", "URL", 0 };
    static char *listings[] = { (char *)listings_func, "listings", "", "NXT", "filters", 0 };
    static char *list[] = { (char *)list_func, "list", "", "NXT", "listing", 0 };
    static char *status[] = { (char *)status_func, "status", "", "NXT", "listingid", 0 };
    static char *cancel[] = { (char *)cancel_func, "cancel", "", "NXT", "listingid", 0 };
    static char *makeoffer[] = { (char *)makeoffer_func, "makeoffer", "", "NXT", "listingid", "bid", 0 };
    static char *acceptoffer[] = { (char *)acceptoffer_func, "acceptoffer", "", "NXT", "listingid", "buyer", 0 };
    static char *feedback[] = { (char *)feedback_func, "feedback", "", "NXT", "listingid", "aboutbuyer", "aboutseller", 0 };
    static char **commands[] = { changeurl, listings, list, status, cancel, makeoffer, acceptoffer, feedback };
    int32_t i,j;
    cJSON *obj,*nxtobj,*objs[8];
    char NXTaddr[64],command[4096],**cmdinfo;
    printf("NXTorrent_json_commands\n");
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

char *NXTorrent_jsonhandler(cJSON *argjson)
{
    struct NXTorrent_info *gp = Global_NXTorrent;
    long len;
    int32_t valid;
    cJSON *json,*obj,*parmsobj,*tokenobj,*secondobj;
    char sender[64],*parmstxt,encoded[NXT_TOKEN_LEN],*retstr = 0;
    sender[0] = 0;
    valid = -1;
    printf("NXTorrent_jsonhandler argjson.%p\n",argjson);
    if ( argjson == 0 )
    {
        json = cJSON_CreateObject();
        obj = cJSON_CreateString(gp->NXTADDR); cJSON_AddItemToObject(json,"NXTaddr",obj);
        obj = cJSON_CreateString(gp->ipaddr); cJSON_AddItemToObject(json,"ipaddr",obj);
       
        obj = gen_NXTaccts_json(0);
        if ( obj != 0 )
            cJSON_AddItemToObject(json,"NXTaccts",obj);
        return(cJSON_Print(json));
    }
    else if ( (argjson->type&0xff) == cJSON_Array && cJSON_GetArraySize(argjson) == 2 )
    {
        parmsobj = cJSON_GetArrayItem(argjson,0);
        secondobj = cJSON_GetArrayItem(argjson,1);
        tokenobj = cJSON_GetObjectItem(secondobj,"token");
        copy_cJSON(encoded,tokenobj);
        parmstxt = cJSON_Print(parmsobj);
        len = strlen(parmstxt);
        stripwhite(parmstxt,len);
        printf("website.(%s) encoded.(%s) len.%ld\n",parmstxt,encoded,strlen(encoded));
        if ( strlen(encoded) == NXT_TOKEN_LEN )
            issue_decodeToken(sender,&valid,parmstxt,encoded);
        free(parmstxt);
        argjson = parmsobj;
    }
    retstr = NXTorrent_json_commands(gp,argjson,sender,valid);
    return(retstr);
}

void process_NXTorrent_AM(struct NXTorrent_info *dp,struct NXT_protocol_parms *parms)
{
    cJSON *argjson;
    char *jsontxt;
    struct json_AM *ap;
    char NXTaddr[64],*sender,*receiver;
    sender = parms->sender; receiver = parms->receiver; ap = parms->AMptr; //txid = parms->txid;
    expand_nxt64bits(NXTaddr,ap->H.nxt64bits);
    if ( strcmp(NXTaddr,sender) != 0 )
    {
        printf("unexpected NXTaddr %s != sender.%s when receiver.%s\n",NXTaddr,sender,receiver);
        return;
    }
    if ( ap->jsonflag != 0 )
    {
        jsontxt = (ap->jsonflag == 1) ? ap->jsonstr : decode_json(&ap->jsn);
        if ( jsontxt != 0 )
        {
            printf("process_NXTorrent_AM got jsontxt.(%s)\n",jsontxt);
            argjson = cJSON_Parse(jsontxt);
            if ( argjson != 0 )
            {
                free_json(argjson);
            }
        }
    }
}

void process_NXTorrent_typematch(struct NXTorrent_info *dp,struct NXT_protocol_parms *parms)
{
    char NXTaddr[64],*sender,*receiver,*txid;
    sender = parms->sender; receiver = parms->receiver; txid = parms->txid;
    safecopy(NXTaddr,sender,sizeof(NXTaddr));
    printf("got txid.(%s) type.%d subtype.%d sender.(%s) -> (%s)\n",txid,parms->type,parms->subtype,sender,receiver);
}

void *NXTorrent_handler(struct NXThandler_info *mp,struct NXT_protocol_parms *parms,void *handlerdata)
{
    struct NXTorrent_info *gp = handlerdata;
    struct NXTorrent_listing *lp = 0;
    if ( parms->txid == 0 )     // indicates non-transaction event
    {
        if ( parms->mode == NXTPROTOCOL_WEBJSON )
            return(NXTorrent_jsonhandler(parms->argjson));
        else if ( parms->mode == NXTPROTOCOL_NEWBLOCK )
            printf("NXTorrent new RTblock %d time %ld microseconds %ld\n",mp->RTflag,time(0),(long)microseconds());
        else if ( parms->mode == NXTPROTOCOL_IDLETIME )
            printf("NXTorrent new idletime %d time %ld microseconds %ld \n",mp->RTflag,time(0),(long)microseconds());
        else if ( parms->mode == NXTPROTOCOL_INIT )
        {
            printf("NXTorrent NXThandler_info init %d\n",mp->RTflag);
            gp = Global_NXTorrent = calloc(1,sizeof(*Global_NXTorrent));
            strcpy(gp->ipaddr,mp->ipaddr);
            strcpy(gp->NXTADDR,mp->NXTADDR);
            strcpy(gp->NXTACCTSECRET,mp->NXTACCTSECRET);
            gp->listings = hashtable_create("listings",HASHTABLES_STARTSIZE,sizeof(*lp),((long)&lp->listingid[0] - (long)lp),sizeof(lp->listingid),((long)&lp->modified - (long)lp));
        }
        return(gp);
    }
    else if ( parms->mode == NXTPROTOCOL_AMTXID )
        process_NXTorrent_AM(gp,parms);
    else if ( parms->mode == NXTPROTOCOL_TYPEMATCH )
        process_NXTorrent_typematch(gp,parms);
    return(gp);
}


#endif
