//
//  NXTorrent.h
//  Created by jl777, April 13th 2014
//  MIT License
//

// Todo:
// 1. MT safe hashtable gather
#ifndef gateway_NXTorrent_h
#define gateway_NXTorrent_h


#define NXTORRENT_SIG 0x48573945
#define NXTORRENT_CANCELADDR "1234567890123456789"
#define NXTORRENT_LISTINGADDR NXTORRENT_CANCELADDR
#define NXTORRENT_MAXLISTINGS_DISP 100

struct NXTorrent_info
{
    char ipaddr[32],NXTADDR[32],NXTACCTSECRET[512];
    struct hashtable *listings;
};
struct NXTorrent_info *Global_NXTorrent;

// AM funcids
#define NXTORRENT_LIST 'L'
#define NXTORRENT_FEEDBACK 'F'
#define NXTORRENT_CANCEL 'X'
#define NXTORRENT_MAKEOFFER 'O'
#define NXTORRENT_ACCEPT 'A'

// status values
#define NXTORRENT_INACTIVE 0
#define NXTORRENT_ACTIVE 1
#define NXTORRENT_CANCELLED 2
#define NXTORRENT_OFFERED 3
#define NXTORRENT_ACCEPTED 4
#define NXTORRENT_INTRANSIT 5
#define NXTORRENT_DISPUTED 6
#define NXTORRENT_COMPLETED 7
#define NUM_NXTORRENT_STATUS 8

#define NXTORRENT_NOLISTING "{\"error\":\"no listingid\"}"

struct _NXTorrent_feedback { int32_t rating; char *about; };
struct NXTorrent_feedback { struct _NXTorrent_feedback seller,buyer; };

struct NXTorrent_listing
{
    uint64_t modified,seller64bits,buyer64bits,listingAMbits,listing64bits,price;
    int32_t status,listedtime;
    char listingid[64],*jsontxt,*URI;
    struct NXTorrent_feedback feedback;
};

char *get_URI_contents(char *URI)
{
    char *contents;
    contents = fetch_URL(URI);
    if ( contents == 0 )
    {
        
    }
    else printf("fetched(%s)\n",contents);
    return(contents);
}

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
            // need to define and implement a way to specify filtering
        }
    }
    return(1);
}

char *NXTorrent_status_str(int32_t status)
{
    switch ( (status & 7) )
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

int32_t conv_status_str(char *str)
{
    int32_t i;
    for (i=0; i<NUM_NXTORRENT_STATUS; i++)
    {
        if ( strcmp(str,NXTorrent_status_str(i)) == 0 )
            return(i);
    }
    return(-1);
}

void purge_NXTorrent_listing(struct NXTorrent_listing *lp,int32_t freeflag)
{
    if ( lp->jsontxt != 0 )
        free(lp->jsontxt);
    if ( lp->URI != 0 )
        free(lp->URI);
    if ( lp->feedback.buyer.about != 0 )
        free(lp->feedback.buyer.about);
    if ( lp->feedback.seller.about != 0 )
        free(lp->feedback.seller.about);
    memset(lp,0,sizeof(*lp));
    if ( freeflag != 0 )
        free(lp);
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

char *AM_NXTorrent(int32_t func,int32_t rating,char *destaddr,uint64_t listingbits,char *jsontxt)
{
    char AM[4096],*retstr;
    struct json_AM *ap = (struct json_AM *)AM;
    printf("listing.%s func.(%c) AM_NXTorrent(%s) -> NXT.(%s) rating.%d\n",nxt64str(listingbits),func,jsontxt,destaddr,rating);
    set_json_AM(ap,NXTORRENT_SIG,func,"0",rating,jsontxt,1);
    ap->H.nxt64bits = listingbits;
    retstr = submit_AM(destaddr,&ap->H,0);
    return(retstr);
}

struct NXTorrent_listing *get_listingid(char *listingid)
{
    struct NXTorrent_info *gp = Global_NXTorrent;
    uint64_t hashval;
    hashval = MTsearch_hashtable(gp->listings,listingid);
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

void init_NXTorrent_listing(struct NXTorrent_listing *lp,uint64_t price,char *seller,char *listingid,char *jsontxt,char *URI)
{
    printf("create new listing\n");
    lp->seller64bits = calc_nxt64bits(seller);
    strcpy(lp->listingid,listingid);
    lp->listing64bits = calc_nxt64bits(listingid);
    lp->listedtime = Global_mp->timestamp;
    lp->status = NXTORRENT_ACTIVE;
    lp->jsontxt = clonestr(jsontxt);
    lp->price = price;
    if ( URI[0] != 0 )
        lp->URI = clonestr(URI);
}

int32_t cmp_NXTorrent_listings(struct NXTorrent_listing *ref,struct NXTorrent_listing *lp)
{
    char refbuf[4096],buf[4096];
    if ( ref->seller64bits != lp->seller64bits )
        return(-1);
    if ( ref->buyer64bits != lp->buyer64bits )
        return(-2);
    if ( ref->listing64bits != lp->listing64bits )
        return(-3);
    if ( ref->listingAMbits != lp->listingAMbits )
        return(-4);
    if ( ref->listedtime != lp->listedtime )
        return(-5);
    if ( ref->price != lp->price )
        return(-6);
    if ( ref->status != lp->status )
        return(-7);
    //safecopy(refbuf,ref->jsontxt,sizeof(refbuf)); safecopy(buf,lp->jsontxt,sizeof(buf));
    //if ( strcmp(refbuf,buf) != 0 )
    //    return(-8);
    safecopy(refbuf,ref->URI,sizeof(refbuf)); safecopy(buf,lp->URI,sizeof(buf));
    if ( strcmp(refbuf,buf) != 0 )
        return(-9);
    safecopy(refbuf,ref->listingid,sizeof(refbuf)); safecopy(buf,lp->listingid,sizeof(buf));
    if ( strcmp(refbuf,buf) != 0 )
        return(-10);
    return(0);
}

cJSON *_gen_NXTorrent_feedback_json(struct _NXTorrent_feedback *ptr)
{
    cJSON *json = 0;
    if ( ptr->rating != 0 || ptr->about != 0 )
    {
        json = cJSON_CreateObject();
        cJSON_AddItemToObject(json,"rating",cJSON_CreateNumber(ptr->rating));
        if ( ptr->about != 0 )
            cJSON_AddItemToObject(json,"about",cJSON_CreateString(ptr->about));
    }
    return(json);
}

cJSON *gen_NXTorrent_feedback_json(struct NXTorrent_feedback *ptr)
{
    cJSON *buyer,*seller,*array = 0;
    buyer = _gen_NXTorrent_feedback_json(&ptr->buyer);
    seller = _gen_NXTorrent_feedback_json(&ptr->seller);
    if ( buyer != 0 || seller != 0 )
    {
        array = cJSON_CreateArray();
        if ( buyer != 0 )
            cJSON_AddItemToArray(array,seller);
        if ( seller != 0 )
            cJSON_AddItemToArray(array,buyer);
    }
    return(array);
}

cJSON *gen_URI_json(char *URI)
{
    char *contents;
    cJSON *json = 0;
    if ( (contents= get_URI_contents(URI)) != 0 )
        json = cJSON_CreateString(contents);
    return(json);
}

cJSON *gen_listing_json(struct NXTorrent_listing *lp,int32_t URIcontents)
{
    cJSON *json,*obj,*listingobj;
    char seller[64],buyer[64],numstr[64];
    json = cJSON_CreateObject();    
    expand_nxt64bits(seller,lp->seller64bits);
    expand_nxt64bits(buyer,lp->buyer64bits);
    cJSON_AddItemToObject(json,"seller",cJSON_CreateString(seller));
    cJSON_AddItemToObject(json,"listingid",cJSON_CreateString(lp->listingid));
    if ( lp->price != 0 )
    {
        sprintf(numstr,"%.8f",dstr(lp->price));
        cJSON_AddItemToObject(json,"price",cJSON_CreateString(numstr));
    }
    cJSON_AddItemToObject(json,"status",cJSON_CreateString(NXTorrent_status_str(lp->status)));
    cJSON_AddItemToObject(json,"listedtime",cJSON_CreateNumber(lp->listedtime));
    if ( buyer[0] != 0 )
        cJSON_AddItemToObject(json,"buyer",cJSON_CreateString(buyer));
    if ( lp->URI != 0 )
    {
        cJSON_AddItemToObject(json,"URI",cJSON_CreateString(lp->URI));
        if ( URIcontents != 0 && (obj= gen_URI_json(lp->URI)) != 0 )
            cJSON_AddItemToObject(json,"URIcontents",obj);
    }
    if ( lp->jsontxt != 0 )
    {
        listingobj = cJSON_Parse(lp->jsontxt);
        if ( listingobj != 0 )
            cJSON_AddItemToObject(json,"listing",listingobj);
    }
    if ( lp->listingAMbits != 0 )
    {
        expand_nxt64bits(numstr,lp->listingAMbits);
        cJSON_AddItemToObject(json,"listingAMtxid",cJSON_CreateString(numstr));
    }
    if ( (obj= gen_NXTorrent_feedback_json(&lp->feedback)) != 0 )
        cJSON_AddItemToObject(json,"feedback",obj);
    return(json);
}

int32_t decode_NXTorrent_json(struct NXTorrent_listing *lp,cJSON *json)
{
    char seller[64],numstr[64],buyer[64],status[64],URI[64],listing[1024],listingAMtxid[64];
    cJSON *obj;
    memset(lp,0,sizeof(*lp));
    if ( json != 0 )
    {
        lp->listedtime = (uint32_t)get_cJSON_int(json,"listedtime");
        obj = cJSON_GetObjectItem(json,"seller"); copy_cJSON(seller,obj);
        obj = cJSON_GetObjectItem(json,"listingid"); copy_cJSON(lp->listingid,obj);
        obj = cJSON_GetObjectItem(json,"price"); copy_cJSON(numstr,obj);
        obj = cJSON_GetObjectItem(json,"buyer"); copy_cJSON(buyer,obj);
        obj = cJSON_GetObjectItem(json,"URI"); copy_cJSON(URI,obj);
        obj = cJSON_GetObjectItem(json,"listing"); copy_cJSON(listing,obj);
        obj = cJSON_GetObjectItem(json,"listingAMtxid"); copy_cJSON(listingAMtxid,obj);
        obj = cJSON_GetObjectItem(json,"status"); copy_cJSON(status,obj);
      
        if ( numstr[0] != 0 )
            lp->price = (uint64_t)(atof(numstr) * SATOSHIDEN);
        if ( seller[0] != 0 )
            lp->seller64bits = calc_nxt64bits(seller);
        if ( lp->listingid[0] != 0 )
            lp->listing64bits = calc_nxt64bits(lp->listingid);
        if ( buyer[0] != 0 )
            lp->buyer64bits = calc_nxt64bits(buyer);
        if ( seller[0] != 0 )
            lp->listingAMbits = calc_nxt64bits(listingAMtxid);
        if ( URI[0] != 0 )
            lp->URI = clonestr(URI);
        if ( listing[0] != 0 )
            lp->jsontxt = clonestr(listing);
        lp->status = conv_status_str(status);
        return(0);
    }
    return(-1);
}

void update_NXTorrent_state(struct NXTorrent_info *dp,int32_t funcid,int32_t rating,cJSON *argjson,uint64_t listingbits,char *sender,char *receiver)
{
    int32_t createdflag;
    char listingid[64],buyer[64],seller[64];
    uint64_t senderbits,receiverbits;
    struct NXTorrent_listing L,*lp;
    if ( listingbits == 0 )
        return;
    if ( funcid == NXTORRENT_LIST )
    {
        if ( decode_NXTorrent_json(&L,argjson) >= 0 )
        {
            lp = MTadd_hashtable(&createdflag,&Global_NXTorrent->listings,L.listingid);
            if ( createdflag != 0 )
                *lp = L;
        }
    }
    else
    {
        senderbits = calc_nxt64bits(sender);
        receiverbits = calc_nxt64bits(receiver);
        expand_nxt64bits(listingid,listingbits);
        lp = MTadd_hashtable(&createdflag,&Global_NXTorrent->listings,listingid);
        switch ( funcid )
        {
            case NXTORRENT_CANCEL:
                lp->status = NXTORRENT_CANCELLED;
                break;
            case NXTORRENT_MAKEOFFER:
                lp->status = NXTORRENT_OFFERED;
                lp->buyer64bits = calc_nxt64bits(sender);
                break;
            case NXTORRENT_ACCEPT:
                lp->buyer64bits = calc_nxt64bits(receiver);
                lp->status = NXTORRENT_ACCEPTED;
                break;
            case NXTORRENT_FEEDBACK:
                expand_nxt64bits(buyer,lp->buyer64bits);
                expand_nxt64bits(seller,lp->seller64bits);
                if ( senderbits == lp->seller64bits && receiverbits == lp->buyer64bits )
                {
                    lp->feedback.buyer.rating = rating;
                    if ( argjson != 0 )
                        lp->feedback.buyer.about = cJSON_Print(argjson);

                }
                else if ( senderbits == lp->buyer64bits && receiverbits == lp->seller64bits )
                {
                    lp->feedback.seller.rating = rating;
                    if ( argjson != 0 )
                        lp->feedback.seller.about = cJSON_Print(argjson);
                }
                else
                {
                    printf("WARNING: unexpected feedback AM: sender.%s -> receiver.%s when seller.%s and buyer.%s\n",sender,receiver,seller,buyer);
                }
                break;
        }
    }
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
            json = gen_listing_json(lp,1);
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
    char NXTaddr[64],destaddr[64],listingid[64],buf[512],*retstr = 0;
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
                if ( lp->buyer64bits != 0 )
                    expand_nxt64bits(destaddr,lp->buyer64bits);
                else strcpy(destaddr,NXTORRENT_CANCELADDR);
                retstr = AM_NXTorrent(NXTORRENT_CANCEL,0,destaddr,lp->listing64bits,buf);
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
                    printf("num.%d add %s\n",(int32_t)num,lp->listingid);
                    cJSON_AddItemToArray(array,cJSON_CreateString(lp->listingid));
                }
                if ( num >= NXTORRENT_MAXLISTINGS_DISP )
                    break;
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
    char numstr[128],listingid[64],hashbuf[512],NXTaddr[64],URI[512],buf[4096],*str,*jsontxt,*retstr = 0;
    struct NXTorrent_listing *lp,L;
    int32_t cmpval,createdflag;
    uint64_t hashval,price = 0;
    cJSON *URIobj,*json,*priceobj,*listingobj = objs[1];
    copy_cJSON(NXTaddr,objs[0]);
    memset(URI,0,sizeof(URI));
    memset(numstr,0,sizeof(numstr));
    if ( NXTaddr[0] != 0 )
    {
        if ( listingobj != 0 )
        {
            URIobj = cJSON_GetObjectItem(listingobj,"URI");
            if ( URIobj != 0 )
                copy_cJSON(URI,URIobj);
            priceobj = cJSON_GetObjectItem(listingobj,"price");
            copy_cJSON(numstr,priceobj);
            if ( numstr[0] != 0 )
                price = (uint64_t)(atof(numstr) * SATOSHIDEN);
        }
        copy_cJSON(buf,listingobj);
        stripwhite(buf,strlen(buf));
        jsontxt = buf;

        sprintf(hashbuf,"%s%s",buf,NXTaddr);
        hashval = calc_decimalhash(hashbuf);
        expand_nxt64bits(listingid,hashval);
        lp = MTadd_hashtable(&createdflag,&Global_NXTorrent->listings,listingid);
        if ( createdflag != 0 )
        {
            if ( lp != 0 )
            {
                init_NXTorrent_listing(lp,price,NXTaddr,listingid,jsontxt,URI);
                json = gen_listing_json(lp,0);
                if ( json != 0 )
                {
                    if ( decode_NXTorrent_json(&L,json) < 0 )
                        retstr = clonestr("{\"error\":\"error parsing listing json\"}");
                    else if ( (cmpval= cmp_NXTorrent_listings(lp,&L)) < 0 )
                    {
                        sprintf(buf,"{\"error\":\"error %d comparing listing json\"}",cmpval);
                        retstr = clonestr(buf);
                    }
                    else
                    {
                        purge_NXTorrent_listing(&L,0);
                        retstr = cJSON_Print(json);
                        str = AM_NXTorrent(NXTORRENT_LIST,0,NXTORRENT_LISTINGADDR,lp->listing64bits,retstr);
                        if ( str == 0 )
                            retstr = clonestr("{\"error\":\"couldnt create listing AM\"}");
                        else
                        {
                            lp->listingAMbits = calc_nxt64bits(str);
                            free(str);
                        }
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
    char NXTaddr[64],listingid[64],seller[64],errstr[513],bid[512],*retstr = 0;
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
                expand_nxt64bits(seller,lp->seller64bits);
                retstr = AM_NXTorrent(NXTORRENT_MAKEOFFER,0,seller,lp->listing64bits,bid);
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
            retstr = AM_NXTorrent(NXTORRENT_ACCEPT,0,buyer,lp->listing64bits,buf);
            retstr = standard_retstr(retstr);
        } else retstr = clonestr(NXTORRENT_NOLISTING);
    }
    return(retstr);
}

char *feedback_func(char *sender,int32_t valid,cJSON **objs,int32_t numobjs)
{
    int32_t rating;
    uint64_t nxt64bits;
    char NXTaddr[64],listingid[64],ratingstr[64],destaddr[64],aboutbuyer[1024],aboutseller[1024],*about,*retstr = 0;
    struct NXTorrent_listing *lp;
    copy_cJSON(NXTaddr,objs[0]);
    copy_cJSON(listingid,objs[1]);
    copy_cJSON(ratingstr,objs[2]);
    copy_cJSON(aboutbuyer,objs[3]);
    copy_cJSON(aboutseller,objs[4]);
    about = 0;
    if ( NXTaddr[0] != 0 && listingid[0] != 0 && (aboutbuyer[0] != 0 || aboutseller[0] != 0) )
    {
        lp = get_listingid(listingid);
        if ( lp != 0 )
        {
            nxt64bits = calc_nxt64bits(NXTaddr);
            if ( lp->buyer64bits == nxt64bits )
            {
                if ( aboutbuyer[0] != 0 || aboutseller[0] == 0 )
                    return(clonestr("{\"error\":\"buyer can only feedback seller\"}"));
                else about = aboutseller;
                expand_nxt64bits(destaddr,lp->seller64bits);
                lp->feedback.seller.rating = rating = atoi(ratingstr);
            }
            else if ( lp->seller64bits == nxt64bits )
            {
                if ( aboutbuyer[0] == 0 || aboutseller[0] != 0 )
                    return(clonestr("{\"error\":\"seller can only feedback buyer\"}"));
                else about = aboutbuyer;
                expand_nxt64bits(destaddr,lp->buyer64bits);
                lp->feedback.buyer.rating = rating = atoi(ratingstr);
            }
            else return(clonestr("{\"error\":\"neither buyer nor seller\"}"));
            // will wait for AM processing to set feedback abouts
            retstr = AM_NXTorrent(NXTORRENT_FEEDBACK,rating,destaddr,lp->listing64bits,about);
            retstr = standard_retstr(retstr);
        } else retstr = clonestr(NXTORRENT_NOLISTING);
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
    static char *feedback[] = { (char *)feedback_func, "feedback", "", "NXT", "listingid", "rating", "aboutbuyer", "aboutseller", 0 };
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
    char *sender,*receiver;
    sender = parms->sender; receiver = parms->receiver; ap = parms->AMptr; //txid = parms->txid;
    if ( ap->jsonflag != 0 )
    {
        jsontxt = (ap->jsonflag == 1) ? ap->jsonstr : decode_json(&ap->jsn);
        if ( jsontxt != 0 )
        {
            printf("process_NXTorrent_AM got jsontxt.(%s)\n",jsontxt);
            argjson = cJSON_Parse(jsontxt);
            if ( argjson != 0 )
            {
                update_NXTorrent_state(dp,ap->funcid,ap->timestamp,argjson,ap->H.nxt64bits,sender,receiver);
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
