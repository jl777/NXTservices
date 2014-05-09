//
//  NXTorrent.h
//  Created by jl777, April 13th 2014
//  MIT License
//

// Todo:
// 1. MT safe hashtable gather
#ifndef gateway_NXTorrent_h
#define gateway_NXTorrent_h

//1. Searching for all bids you (or any account) have posted (so, search for bids by account number, and by status too, so you can find open ones, rejected ones, accepted ones). It would be helpful to get back the bid ID, the item ID, the bid amount, the bid's timestamp, and the item status.

//3. It would be helpful to have a status message to get returned from cancel bid and accept bid too, so I can display if it worked or failed.

#define NXTORRENT_SIG 0x38473947
#define NXTORRENT_CANCELADDR "1234567890123456789"
#define NXTORRENT_LISTINGADDR NXTORRENT_CANCELADDR
#define NXTORRENT_MAXLISTINGS_DISP 100
#define MAX_NXTORRENT_RATING 100

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
#define NXTORRENT_EXPIRED 8
#define NUM_NXTORRENT_STATUS 9

struct _NXTorrent_feedback { int32_t rating; char *about; };
struct NXTorrent_feedback { struct _NXTorrent_feedback seller,buyer; };

struct NXTorrent_listing
{
    uint64_t modified,seller64bits,buyer64bits,listing64bits,assetidbits;
    int64_t price,amount;
    int32_t coinid,duration;
    char category[3][16],tag[3][16],coinaddr[64],listingid[64],title[128],*descr,*URI;

    int32_t status;
    long listedtime,expiration;
    uint64_t listingAMbits;
    struct search_tag *categories[3],*tags[3];
    struct NXTorrent_feedback feedback;
};
struct search_tag { uint64_t modified; struct NXTorrent_listing **lps; int32_t numlps; char key[16]; };
struct rawfeedback { uint64_t other64bits; int32_t rawrating,timestamp; float rating; };
struct feedback_info { uint64_t modified; struct rawfeedback *raw; int32_t numfeedbacks; char NXTaddr[24]; };

struct NXTorrent_info
{
    char NXTADDR[32],NXTACCTSECRET[512];
    int32_t timestamp;
    struct hashtable *listings,*categories,*tags,*buyers,*sellers;
    portable_mutex_t category_mutex,tag_mutex,buyer_mutex,seller_mutex;
};
struct NXTorrent_info *Global_NXTorrent;

int32_t calc_raw_NXTtx(char *utxbytes,char *sighash,uint64_t assetidbits,int64_t amount,uint64_t other64bits);

struct NXTorrent_filter *calc_NXTorrent_filter(cJSON *json)
{
    struct NXTorrent_filter *filter = 0;
    
    return(filter);
}

int32_t is_NXTorrent_filtered(struct NXTorrent_filter *filter,struct NXTorrent_listing *lp)
{
    return(1);
}

int32_t passes_filter(struct NXTorrent_listing *lp,struct NXTorrent_filter *filter,int32_t timestamp)
{
    if ( lp->expiration < timestamp )
        lp->status = NXTORRENT_EXPIRED;
    if ( lp->status == NXTORRENT_ACTIVE || lp->status == NXTORRENT_OFFERED )
    {
        if ( filter != 0 && is_NXTorrent_filtered(filter,lp) != 0 )
            return(0);
        return(1);
    }
    return(0);
}

double calc_NXTorrent_weight(double dpreds[6])
{
    return(calc_dpreds_ave(dpreds) / MAX_NXTORRENT_RATING);
}

double map_NXTorrent_rating(double ave,double aveabs,double min,double max,double dpreds[6],double rating)
{
    double otherave,otherabs;
    if ( ave == 0. || aveabs == 0. )
        return(rating);
    otherave = calc_dpreds_ave(dpreds);
    otherabs = calc_dpreds_abs(dpreds);
    if ( otherave == 0. || otherabs == 0. )
        return(0.);
    rating -= otherave, rating += ave;
    rating /= otherabs, rating *= otherabs;
    if ( rating > MAX_NXTORRENT_RATING )
        rating = MAX_NXTORRENT_RATING;
    else if ( rating < -MAX_NXTORRENT_RATING )
        rating = -MAX_NXTORRENT_RATING;
    return(rating);
}

double recalc_NXTorrent_feedback(struct hashtable **tablep,char *NXTaddr)
{
    double direct[6],indirect[6],weighed[6],refdpreds[6],rating,ave,aveabs;
    char otherNXTaddr[64];
    int32_t i,createdflag;
    uint64_t my64bits;
    struct NXT_acct *np;
    struct rawfeedback *raw;
    struct feedback_info *fb;
    memset(direct,0,sizeof(direct));
    memset(indirect,0,sizeof(indirect));
    memset(weighed,0,sizeof(weighed));
    np = get_NXTacct(&createdflag,Global_mp,Global_NXTorrent->NXTADDR);
    memcpy(refdpreds,np->hisfeedbacks,sizeof(refdpreds));
    ave = calc_dpreds_ave(refdpreds);
    aveabs = calc_dpreds_abs(refdpreds);
    my64bits = calc_nxt64bits(NXTaddr);
    fb = MTadd_hashtable(&createdflag,tablep,NXTaddr);
    if ( createdflag == 0 )
    {
        for (i=0; i<fb->numfeedbacks; i++)
        {
            raw = &fb->raw[i];
            if ( raw->other64bits == my64bits )
                update_dpreds(direct,raw->rating);
            else
            {
                expand_nxt64bits(otherNXTaddr,raw->other64bits);
                np = get_NXTacct(&createdflag,Global_mp,otherNXTaddr);
                rating = map_NXTorrent_rating(ave,aveabs,refdpreds[4],refdpreds[5],np->hisfeedbacks,raw->rating);
                if ( rating != 0. )
                {
                    update_dpreds(indirect,rating);
                    update_dpreds(weighed,rating * MAX(0.,calc_NXTorrent_weight(np->myfb_tohim)));
                }
            }
        }
    }
    return(calc_dpreds_ave(direct) + calc_dpreds_ave(indirect)/10. + calc_dpreds_ave(weighed)/3.);
}

struct feedback_info *update_NXTorrent_feedback(portable_mutex_t *mutex,struct hashtable **tablep,char *NXTaddr,uint64_t other64bits,int32_t rating,int32_t timestamp)
{
    struct feedback_info *fb;
    struct rawfeedback *raw;
    struct NXT_acct *np;
    char otherNXTaddr[64];
    int32_t createdflag;
    if ( NXTaddr == 0 || NXTaddr[0] == 0)
        return(0);
    fb = MTadd_hashtable(&createdflag,tablep,NXTaddr);
    portable_mutex_lock(mutex);
    fb->raw = realloc(fb->raw,sizeof(*fb->raw) * (fb->numfeedbacks+1));
    raw = &fb->raw[fb->numfeedbacks];
    raw->other64bits = other64bits;
    raw->rawrating = rating;
    raw->timestamp = timestamp;
    fb->numfeedbacks++;
    portable_mutex_unlock(mutex);
    expand_nxt64bits(otherNXTaddr,other64bits);
    np = get_NXTacct(&createdflag,Global_mp,otherNXTaddr);
    update_dpreds(np->hisfeedbacks,rating);
    if ( strcmp(otherNXTaddr,Global_NXTorrent->NXTADDR) == 0 )
    {
        np = get_NXTacct(&createdflag,Global_mp,otherNXTaddr);
        update_dpreds(np->myfb_tohim,rating);
    }
    return(fb);
}

struct search_tag *add_search_tag(portable_mutex_t *mutex,struct hashtable **tablep,char *key,struct NXTorrent_listing *lp)
{
    int32_t i;
    struct search_tag *tag;
    int32_t createdflag;
    if ( key == 0 || key[0] == 0)
        return(0);
    tag = MTadd_hashtable(&createdflag,tablep,key);
    portable_mutex_lock(mutex);
    for (i=0; i<tag->numlps; i++)
        if ( tag->lps[i] == lp )
            break;
    if ( i == tag->numlps )
    {
        tag->lps = realloc(tag->lps,sizeof(*tag->lps) * (tag->numlps+1));
        tag->lps[tag->numlps] = lp;
        tag->numlps++;
    } else printf("unexpected duplicate lp for key.(%s) i.%d\n",key,i);
    portable_mutex_unlock(mutex);
    return(tag);
}

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
        case NXTORRENT_EXPIRED: return("expired listing"); break;
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

char *NXTorrent_retjson(int32_t retval,char *retmsg,char *attachment,int32_t freeflag)
{
    long len;
    char *jsonstr,*retstr;
    if ( retmsg == 0 )
    {
        if ( retval < 0 )
            retmsg = "error";
        else if ( retval < NUM_NXTORRENT_STATUS )
            retmsg = NXTorrent_status_str(retval);
        else retmsg = "success";
    }
    len = strlen(retmsg);
    len += 1024;
    if ( attachment != 0 )
        len += strlen(attachment);
    jsonstr = malloc(len);
    sprintf(jsonstr,"{\"retcode\":%d,\"retmsg\":\"%s\"",retval,retmsg);
    if ( attachment != 0 && attachment[0] != 0 )
        sprintf(jsonstr+strlen(jsonstr),",\"attachment\":%s}",attachment);
    else strcat(jsonstr,"}");
    retstr = clonestr(jsonstr);
    free(jsonstr);
    if ( freeflag != 0 && attachment != 0 )
        free(attachment);
    return(retstr);
}

char *standard_AMretstr(int32_t status,char *retstr)
{
    cJSON *json;
    char *attachment = 0;
    if ( retstr != 0 )
    {
        json = cJSON_CreateObject();
        cJSON_AddItemToObject(json,"result",cJSON_CreateString("good"));
        cJSON_AddItemToObject(json,"AMtxid",cJSON_CreateString(retstr));
        free(retstr);
        attachment = cJSON_Print(json);
        free_json(json);
        return(NXTorrent_retjson(status,0,attachment,1));
    }
    else return(NXTorrent_retjson(-1,"error submitting AM",0,0));
}

void purge_NXTorrent_listing(struct NXTorrent_listing *lp,int32_t freeflag)
{
    if ( lp->descr != 0 )
        free(lp->descr);
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
    hashval = MTsearch_hashtable(&gp->listings,listingid);
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

int32_t cmp_NXTorrent_listings(struct NXTorrent_listing *ref,struct NXTorrent_listing *lp)
{
    int32_t i;
    char refbuf[4096],buf[4096];
    if ( ref->seller64bits != lp->seller64bits )
        return(-1);
    if ( ref->buyer64bits != lp->buyer64bits )
        return(-2);
    if ( ref->listing64bits != lp->listing64bits )
    {
        printf("listingbits %s vs %s\n",nxt64str(ref->listing64bits),nxt64str2(lp->listing64bits));
        return(-3);
    }
    if ( ref->listingAMbits != lp->listingAMbits )
        return(-4);
    //if ( ref->listedtime != lp->listedtime )
    //    return(-5);
    if ( ref->price != lp->price )
        return(-6);
    if ( ref->status != lp->status )
        return(-7);
    safecopy(refbuf,ref->descr,sizeof(refbuf)); safecopy(buf,lp->descr,sizeof(buf));
    if ( strcmp(refbuf,buf) != 0 )
        return(-8);
    safecopy(refbuf,ref->URI,sizeof(refbuf)); safecopy(buf,lp->URI,sizeof(buf));
    if ( strcmp(refbuf,buf) != 0 )
        return(-9);
    safecopy(refbuf,ref->listingid,sizeof(refbuf)); safecopy(buf,lp->listingid,sizeof(buf));
    if ( strcmp(refbuf,buf) != 0 )
        return(-10);
    if ( ref->assetidbits != lp->assetidbits )
        return(-11);
    if ( ref->amount != lp->amount )
        return(-12);
    if ( ref->coinid != lp->coinid )
        return(-13);
    if ( ref->duration != lp->duration )
        return(-14);
    if ( strcmp(ref->coinaddr,lp->coinaddr) != 0 )
        return(-15);
    if ( strcmp(ref->title,lp->title) != 0 )
        return(-16);
    for (i=0; i<3; i++)
    {
        if ( strcmp(ref->category[i],lp->category[i]) != 0 )
            return(-17-i*2);
        if ( strcmp(ref->tag[i],lp->tag[i]) != 0 )
            return(-17-i*2-1);
    }
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
    {
        json = cJSON_CreateString(contents);
        free(contents);
    }
    return(json);
}

cJSON *gen_NXTsubatomic_json(int32_t coinid,uint64_t assetidbits,int64_t amount,char *coinaddr)
{
    cJSON *json = 0;
    char assetidstr[64],numstr[64];
    if ( amount > 0 && (coinid == NXT_COINID || coinaddr[0] != 0) )
    {
        json = cJSON_CreateObject();
        cJSON_AddItemToObject(json,"coin",cJSON_CreateString(coinid_str(coinid)));
        if ( coinid == NXT_COINID && assetidbits != 0 )
        {
            expand_nxt64bits(assetidstr,assetidbits);
            cJSON_AddItemToObject(json,"assetid",cJSON_CreateString(assetidstr));
        }
        else if ( coinid != NXT_COINID && coinaddr[0] != 0 )
            cJSON_AddItemToObject(json,"coinaddr",cJSON_CreateString(coinaddr));
        sprintf(numstr,"%.8f",dstr(amount));
        cJSON_AddItemToObject(json,"amount",cJSON_CreateString(numstr));
    }
    return(json);
}
    
cJSON *gen_listing_json(struct NXTorrent_listing *lp,int32_t URIcontents,int32_t AMflag)
{
    cJSON *json,*obj;
    double buyer_rating,seller_rating;
    char seller[64],buyer[64],numstr[64];
    json = cJSON_CreateObject();    
    expand_nxt64bits(seller,lp->seller64bits);
    expand_nxt64bits(buyer,lp->buyer64bits);
    cJSON_AddItemToObject(json,"seller",cJSON_CreateString(seller));
    if ( AMflag == 0 )
    {
        buyer_rating = recalc_NXTorrent_feedback(&Global_NXTorrent->buyers,seller);
        seller_rating = recalc_NXTorrent_feedback(&Global_NXTorrent->sellers,seller);
        sprintf(numstr,"%.2f",buyer_rating),cJSON_AddItemToObject(json,"buyer_rating",cJSON_CreateString(numstr));
        sprintf(numstr,"%.2f",seller_rating),cJSON_AddItemToObject(json,"seller_rating",cJSON_CreateString(numstr));
    }
    cJSON_AddItemToObject(json,"listingid",cJSON_CreateString(lp->listingid));
    cJSON_AddItemToObject(json,"title",cJSON_CreateString(lp->title));
    if ( lp->price != 0 )
    {
        sprintf(numstr,"%.8f",dstr(lp->price));
        cJSON_AddItemToObject(json,"price",cJSON_CreateString(numstr));
    }
    if ( (obj= gen_NXTsubatomic_json(lp->coinid,lp->assetidbits,lp->amount,lp->coinaddr)) != 0 )
        cJSON_AddItemToObject(json,"NXTsubatomic",obj);
    cJSON_AddItemToObject(json,"status",cJSON_CreateString(NXTorrent_status_str(lp->status)));
    cJSON_AddItemToObject(json,"listedtime",cJSON_CreateNumber(lp->listedtime));
    cJSON_AddItemToObject(json,"expiration",cJSON_CreateNumber(lp->expiration));
    sprintf(numstr,"%.8f",(double)lp->duration/3600.);
    cJSON_AddItemToObject(json,"duration",cJSON_CreateString(numstr));
    if ( buyer[0] != 0 )
        cJSON_AddItemToObject(json,"buyer",cJSON_CreateString(buyer));
    cJSON_AddItemToObject(json,"category1",cJSON_CreateString(lp->category[0]));
    cJSON_AddItemToObject(json,"category2",cJSON_CreateString(lp->category[1]));
    cJSON_AddItemToObject(json,"category3",cJSON_CreateString(lp->category[2]));
    cJSON_AddItemToObject(json,"tag1",cJSON_CreateString(lp->tag[0]));
    cJSON_AddItemToObject(json,"tag2",cJSON_CreateString(lp->tag[1]));
    cJSON_AddItemToObject(json,"tag3",cJSON_CreateString(lp->tag[2]));
    if ( lp->URI != 0 )
    {
        cJSON_AddItemToObject(json,"URI",cJSON_CreateString(lp->URI));
        if ( URIcontents != 0 && (obj= gen_URI_json(lp->URI)) != 0 )
            cJSON_AddItemToObject(json,"URIcontents",obj);
    }
    if ( lp->descr != 0 )
            cJSON_AddItemToObject(json,"description",cJSON_CreateString(lp->descr));
    if ( lp->listingAMbits != 0 )
    {
        expand_nxt64bits(numstr,lp->listingAMbits);
        cJSON_AddItemToObject(json,"listingAMtxid",cJSON_CreateString(numstr));
    }
    if ( (obj= gen_NXTorrent_feedback_json(&lp->feedback)) != 0 )
        cJSON_AddItemToObject(json,"feedback",obj);
    return(json);
}

int32_t isvalid_NXTsubatomic(char *txbytes,char *sighash,int32_t *coinidp,int64_t *amountp,char *coinaddr_assetid,char *str)
{
    char coin[64],amountstr[64];
    cJSON *json;
    json = cJSON_Parse(str);
    *coinidp = -1;
    *amountp = 0;
    coinaddr_assetid[0] = 0;
    if ( txbytes != 0 )
        txbytes[0] = 0;
    if ( sighash != 0 )
        sighash[0] = 0;
    if ( json != 0 )
    {
        if ( extract_cJSON_str(coin,sizeof(coin),json,"coin") > 0 && extract_cJSON_str(amountstr,sizeof(amountstr),json,"amount") > 0 )
        {
            *amountp = (int64_t)(atof(amountstr) * SATOSHIDEN);
            if ( *amountp > 0 )
            {
                *coinidp = conv_coinstr(coin);
                if ( *coinidp == NXT_COINID )
                {
                    extract_cJSON_str(coinaddr_assetid,64,json,"assetid");
                    if ( txbytes != 0 )
                        extract_cJSON_str(txbytes,64,json,"txbytes");
                    if ( sighash != 0 )
                        extract_cJSON_str(sighash,64,json,"sighash");
                    return(1);
                }
                else
                {
                    extract_cJSON_str(coinaddr_assetid,64,json,"coinaddr");
                    if ( strlen(coinaddr_assetid) > 6 )
                        return(1);
                }
            }
        }
    }
    return(0);
}

int32_t decode_NXTorrent_json(struct NXTorrent_listing *lp,cJSON *json)
{
    char seller[1024],numstr[1024],buyer[1024],status[1024],URI[1024],description[1024],listingAMtxid[1024],*tmpstr;
    cJSON *obj;
    memset(lp,0,sizeof(*lp));
    if ( json != 0 )
    {
        lp->listedtime = get_cJSON_int(json,"listedtime");
        obj = cJSON_GetObjectItem(json,"duration"), copy_cJSON(numstr,obj);
        lp->duration = 3600. * atof(numstr);
        lp->expiration = get_cJSON_int(json,"expiration");
        obj = cJSON_GetObjectItem(json,"seller"), copy_cJSON(seller,obj);
        obj = cJSON_GetObjectItem(json,"listingid"), copy_cJSON(lp->listingid,obj);
        obj = cJSON_GetObjectItem(json,"price"), copy_cJSON(numstr,obj);
        obj = cJSON_GetObjectItem(json,"buyer"), copy_cJSON(buyer,obj);
        obj = cJSON_GetObjectItem(json,"URI"), copy_cJSON(URI,obj);
        obj = cJSON_GetObjectItem(json,"description"), copy_cJSON(description,obj);
        obj = cJSON_GetObjectItem(json,"listingAMtxid"), copy_cJSON(listingAMtxid,obj);
        obj = cJSON_GetObjectItem(json,"status"), copy_cJSON(status,obj);
        obj = cJSON_GetObjectItem(json,"NXTsubatomic");
        if ( obj != 0 )
        {
            tmpstr = cJSON_Print(obj);
            if ( isvalid_NXTsubatomic(0,0,&lp->coinid,&lp->amount,lp->coinaddr,tmpstr) != 0 && lp->coinid == NXT_COINID )
            {
                lp->assetidbits = calc_nxt64bits(lp->coinaddr);
                memset(lp->coinaddr,0,sizeof(lp->coinaddr));
            }
            free(tmpstr);
        }
        extract_cJSON_str(lp->title,sizeof(lp->title),json,"title");
        extract_cJSON_str(lp->category[0],sizeof(lp->category[0]),json,"category1");
        extract_cJSON_str(lp->category[1],sizeof(lp->category[1]),json,"category2");
        extract_cJSON_str(lp->category[2],sizeof(lp->category[2]),json,"category3");
        extract_cJSON_str(lp->tag[0],sizeof(lp->tag[0]),json,"tag1");
        extract_cJSON_str(lp->tag[1],sizeof(lp->tag[1]),json,"tag2");
        extract_cJSON_str(lp->tag[2],sizeof(lp->tag[2]),json,"tag3");
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
        if ( description[0] != 0 )
            lp->descr = clonestr(description);
        lp->status = conv_status_str(status);
        return(0);
    }
    return(-1);
}

char *list_func(char *sender,int32_t valid,cJSON **objs,int32_t numobjs)
{
    char tag1[1024],tag2[1024],tag3[1024],description[1024],duration[1024],coinaddr_assetid[64];
    char NXTsubatomic[1024],title[1024],pricestr[1024],category1[1024],category2[1024],category3[1024];
    char listingid[64],NXTaddr[64],URI[1024],buf[4096],*str,*retstr = 0;
    struct NXTorrent_listing *lp,L;
    int32_t validsubatomic,cmpval,createdflag,coinid;
    int64_t amount;
    uint32_t crc;
    uint64_t hashval,assetidbits,nxt64bits,price = 0;
    cJSON *json;
    copy_cJSON(NXTaddr,objs[0]);
    copy_cJSON(title,objs[1]), title[sizeof(lp->title)-1] = 0;
    copy_cJSON(pricestr,objs[2]);
    price = (int64_t)(atof(pricestr) * SATOSHIDEN);
    copy_cJSON(category1,objs[3]), category1[sizeof(lp->category[0])-1] = 0;
    copy_cJSON(category2,objs[4]), category2[sizeof(lp->category[1])-1] = 0;
    copy_cJSON(category3,objs[5]), category3[sizeof(lp->category[2])-1] = 0;
    copy_cJSON(tag1,objs[6]), tag1[sizeof(lp->tag[0])-1] = 0;
    copy_cJSON(tag2,objs[7]), tag2[sizeof(lp->tag[1])-1] = 0;
    copy_cJSON(tag3,objs[8]), tag3[sizeof(lp->tag[2])-1] = 0;
    copy_cJSON(URI,objs[9]);
    copy_cJSON(description,objs[10]);
    copy_cJSON(duration,objs[11]);
    copy_cJSON(NXTsubatomic,objs[12]);
    coinid = NXT_COINID;
    assetidbits = amount = 0;
    coinaddr_assetid[0] = 0;
    if ( NXTsubatomic[0] != 0 )
    {
        replace_backslashquotes(NXTsubatomic);
        validsubatomic = isvalid_NXTsubatomic(0,0,&coinid,&amount,coinaddr_assetid,NXTsubatomic);
        if ( coinid == NXT_COINID )
            assetidbits = calc_nxt64bits(coinaddr_assetid), coinaddr_assetid[0] = 0;
    } else validsubatomic = 0;
    //    static char *list[] = { (char *)list_func, "list", "", "NXT", "title", "price", "category1", "category2", "category3", "tag1", "tag2", "tag3", "URI", "description", "duration", 0 };
    if ( NXTaddr[0] != 0 && (price > 0 || validsubatomic != 0) )
    {
        memset(&L,0,sizeof(L));
        nxt64bits = calc_nxt64bits(NXTaddr);
        L.seller64bits = nxt64bits;
        L.assetidbits = assetidbits;
        L.price = price, L.amount = amount;
        L.coinid = coinid;
        L.duration = 3600 * atoi(duration);
        if ( L.duration <= 0 )
            L.duration = 24 * 3600;
        safecopy(L.title,title,sizeof(L.title));
        safecopy(L.coinaddr,coinaddr_assetid,sizeof(L.coinaddr));
        safecopy(L.category[0],category1,sizeof(L.category[0]));
        safecopy(L.category[1],category2,sizeof(L.category[1]));
        safecopy(L.category[2],category3,sizeof(L.category[2]));
        safecopy(L.tag[0],tag1,sizeof(L.tag[0]));
        safecopy(L.tag[1],tag2,sizeof(L.tag[1]));
        safecopy(L.tag[2],tag3,sizeof(L.tag[2]));
        
        hashval = (nxt64bits ^ (nxt64bits << 32L));
        hashval &= ~((1L << 32L) - 1);
        crc = _crc32(hashval>>32,&L,sizeof(L));
        if ( URI[0] != 0 )
            L.URI = clonestr(URI), crc = _crc32(crc,URI,strlen(URI));
        if ( description[0] != 0 )
            L.descr = clonestr(description), crc = _crc32(crc,description,strlen(description));
        hashval |= crc;
        expand_nxt64bits(listingid,hashval);
        safecopy(L.listingid,listingid,sizeof(L.listingid));
        L.listing64bits = calc_nxt64bits(L.listingid);
        L.listedtime = issue_getTime();
        printf("listedtime %d\n",(int)L.listedtime);
        L.expiration = (L.listedtime + L.duration);

        lp = MTadd_hashtable(&createdflag,&Global_NXTorrent->listings,listingid);
        if ( createdflag != 0 )
        {
            *lp = L;
            json = gen_listing_json(lp,0,1);
            if ( json != 0 )
            {
                printf("GET.(%s)\n",cJSON_Print(json));
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
                    free(retstr);
                    if ( is_valid_NXTtxid(retstr) != 0 )
                        lp->listingAMbits = calc_nxt64bits(str);
                    return(standard_AMretstr(lp->status,str));
                }
                free_json(json);
            }
            else retstr = clonestr("{\"error\":\"unexpected JSON parse error\"}");
        }
        else retstr = clonestr("{\"error\":\"duplicate listing\"}");
    }
    return(NXTorrent_retjson(-1,"cant create listing",retstr,1));
}

void update_NXTorrent_state(struct NXTorrent_info *dp,int32_t funcid,int32_t rating,cJSON *argjson,uint64_t listingbits,char *sender,char *receiver,uint64_t AMtxidbits)
{
    int32_t i,createdflag;
    char listingid[64],buyer[64],seller[64];
    uint64_t senderbits,receiverbits;
    struct NXTorrent_listing L,*lp;
    if ( listingbits == 0 )
    {
        printf("update_NXTorrent_state: invalid listingbits.0\n");
        return;
    }
    if ( funcid == NXTORRENT_LIST )
    {
        if ( decode_NXTorrent_json(&L,argjson) >= 0 )
        {
            lp = MTadd_hashtable(&createdflag,&Global_NXTorrent->listings,L.listingid);
            if ( createdflag != 0 )
                *lp = L;
            if ( lp->status == 0 )
            {
                lp->expiration = lp->listedtime + lp->duration;
                for (i=0; i<3; i++)
                {
                    lp->categories[i] = add_search_tag(&Global_NXTorrent->category_mutex,&Global_NXTorrent->categories,lp->category[i],lp);
                    lp->tags[i] = add_search_tag(&Global_NXTorrent->tag_mutex,&Global_NXTorrent->tags,lp->tag[i],lp);
                }
                lp->status = NXTORRENT_ACTIVE;
            }
            if ( lp->listingAMbits != AMtxidbits )
            {
                if ( lp->listingAMbits != 0 )
                    printf("update NXTorrentstate: unexpected nonz AMtxidbits %s vs %s\n",nxt64str(lp->listingAMbits),nxt64str2(AMtxidbits));
                lp->listingAMbits = AMtxidbits;
            }
        } else printf("update_NXTorrent_state: error decoding argjson\n");
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
                if ( rating > MAX_NXTORRENT_RATING )
                    rating = MAX_NXTORRENT_RATING;
                else if ( rating < -MAX_NXTORRENT_RATING )
                    rating = -MAX_NXTORRENT_RATING;
                if ( senderbits == lp->seller64bits && receiverbits == lp->buyer64bits )
                {
                    lp->feedback.buyer.rating = rating;
                    update_NXTorrent_feedback(&Global_NXTorrent->buyer_mutex,&Global_NXTorrent->buyers,buyer,lp->seller64bits,rating,Global_NXTorrent->timestamp);
                    if ( argjson != 0 )
                        lp->feedback.buyer.about = cJSON_Print(argjson);
                }
                else if ( senderbits == lp->buyer64bits && receiverbits == lp->seller64bits )
                {
                    lp->feedback.seller.rating = rating;
                    update_NXTorrent_feedback(&Global_NXTorrent->seller_mutex,&Global_NXTorrent->sellers,seller,lp->buyer64bits,rating,Global_NXTorrent->timestamp);
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
            json = gen_listing_json(lp,1,0);
            if ( json != 0 )
            {
                retstr = cJSON_Print(json);
                free_json(json);
                return(NXTorrent_retjson(lp->status,0,retstr,1));
            }
        }
    }
    return(NXTorrent_retjson(-1,"nolisting",0,0));
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
                sprintf(buf,"{\"cancelled\":\"%s\"}",listingid);
                if ( lp->buyer64bits != 0 )
                    expand_nxt64bits(destaddr,lp->buyer64bits);
                else strcpy(destaddr,NXTORRENT_CANCELADDR);
                retstr = AM_NXTorrent(NXTORRENT_CANCEL,0,destaddr,lp->listing64bits,buf);
                if ( is_valid_NXTtxid(retstr) != 0 )
                    lp->status = NXTORRENT_CANCELLED;
                return(standard_AMretstr(lp->status,retstr));
            }
            else
            {
                sprintf(buf,"{\"error\":\"cant cancel\",\"reason\":\"%s\"}",NXTorrent_status_str(get_listing_status(listingid)));
                retstr = clonestr(buf);
            }
        }
    }
    return(NXTorrent_retjson(-1,"nolisting",retstr,1));
}

char *listings_func(char *sender,int32_t valid,cJSON **objs,int32_t numobjs)
{
    struct NXTorrent_filter *filter;
    char NXTaddr[64],numstr[64],*retstr = 0;
    struct NXTorrent_listing **all,*lp;
    int64_t i,num,n=0;
    int32_t remaining;
    cJSON *array,*item;
    copy_cJSON(NXTaddr,objs[0]);
    //printf("listings\n");
    if ( NXTaddr[0] != 0 )
    {
        all = (struct NXTorrent_listing **)hashtable_gather_modified(&num,Global_NXTorrent->listings,1);
        if ( all != 0 && num != 0 )
        {
           // printf("num.%d\n",(int)num);
            array = cJSON_CreateArray();
            filter = calc_NXTorrent_filter(objs[1]);
            for (i=0; i<num; i++)
            {
                lp = all[i];
                if ( passes_filter(lp,filter,Global_NXTorrent->timestamp) > 0 )
                {
                    n++;
                    //printf("num.%d add %s\n",(int32_t)num,lp->listingid);
                    item = cJSON_CreateObject();
                    cJSON_AddItemToObject(item,"listingid",cJSON_CreateString(lp->listingid));
                    cJSON_AddItemToObject(item,"title",cJSON_CreateString(lp->title));
                    cJSON_AddItemToObject(item,"category1",cJSON_CreateString(lp->category[0]));
                    if ( lp->price > 0 )
                    {
                        sprintf(numstr,"%.8f",dstr(lp->price));
                        cJSON_AddItemToObject(item,"price",cJSON_CreateString(numstr));
                    }
                    else
                    {
                        if ( lp->amount > 0 && (lp->coinid == NXT_COINID || lp->coinaddr[0] != 0) )
                            cJSON_AddItemToObject(item,"NXTsubatomic",gen_NXTsubatomic_json(lp->coinid,lp->assetidbits,lp->amount,lp->coinaddr));
                    }
                    cJSON_AddItemToObject(item,"status",cJSON_CreateString(NXTorrent_status_str(lp->status)));
                    remaining = (int32_t)(lp->expiration - Global_NXTorrent->timestamp);
                    sprintf(numstr,"%d:%02d:%02d",remaining/3600,(remaining/60)%60,remaining%60);
                    cJSON_AddItemToObject(item,"timeleft",cJSON_CreateString(numstr));
                    cJSON_AddItemToArray(array,item);
                }
                if ( i >= NXTORRENT_MAXLISTINGS_DISP )
                    break;
            }
            free(all);
            if ( n > 0 )
            {
                //item ID, title, category (or at least category1), status, and price
                retstr = cJSON_Print(array);
                return(NXTorrent_retjson(0,"search results",retstr,1));
            }
            free_json(array);
        }
    }
    return(NXTorrent_retjson(-1,"no matches",0,0));
}

char *makeoffer_func(char *sender,int32_t valid,cJSON **objs,int32_t numobjs)
{
    char sighash[1024],txbytes[1024];
    char NXTaddr[64],listingid[1024],bidjsonstr[1024],seller[64],coinaddr_assetid[64],errstr[513],bid[1024],comments[1024];
    char *str,*retstr = 0;
    struct NXTorrent_listing *lp = 0;
    cJSON *json;
    uint64_t assetidbits;
    int64_t amount;
    int32_t coinid,validsubatomic;
    copy_cJSON(NXTaddr,objs[0]);
    copy_cJSON(listingid,objs[1]);
    copy_cJSON(bid,objs[2]);
    copy_cJSON(comments,objs[3]);
    bidjsonstr[0] = 0;
    if ( listingid[0] != 0 )
        lp = get_listingid(listingid);
    assetidbits = 0;
    validsubatomic = 0;
    if ( bid[0] != 0 )
    {
        replace_backslashquotes(bid);
        if ( bid[0] == '{' )
        {
            validsubatomic = isvalid_NXTsubatomic(txbytes,sighash,&coinid,&amount,coinaddr_assetid,bid);
            if ( coinid != NXT_COINID )
                str = "coinaddr";
            else
            {
                str = "assetid";
                assetidbits = calc_nxt64bits(coinaddr_assetid);
            }
        }
        else
        {
            coinid = NXT_COINID;
            amount = (int64_t)(atof(bid) * SATOSHIDEN);
            coinaddr_assetid[0] = 0;
            str = "assetid";
        }
        if ( coinid == NXT_COINID )
            calc_raw_NXTtx(txbytes,sighash,assetidbits,amount,lp->seller64bits);
        else calc_raw_NXTtx(txbytes,sighash,0,SATOSHIDEN,lp->seller64bits);
        sprintf(bidjsonstr,"{\"coin\":\"%s\",\"%s\":\"%s\",\"amount\":\"%.8f\"",coinid_str(coinid),str,coinaddr_assetid,dstr(amount));
        if ( txbytes[0] != 0 )
            sprintf(bidjsonstr+strlen(bidjsonstr),",\"txbytes\":\"%s\"",txbytes);
        if ( sighash[0] != 0 )
            sprintf(bidjsonstr+strlen(bidjsonstr),",\"sighash\":\"%s\"",sighash);
        if ( comments[0] != 0 )
            sprintf(bidjsonstr+strlen(bidjsonstr),",\"comments\":\"%s\"}",comments);
        json = cJSON_Parse(bidjsonstr);
        if ( json == 0 )
            printf("error parsing bidjsonstr.(%s)\n",bidjsonstr);
        else free_json(json);
    }
    if ( NXTaddr[0] != 0 && listingid[0] != 0 && (validsubatomic != 0 || amount > 0) )
    {
        if ( lp != 0 )
        {
            if ( lp->status == NXTORRENT_ACTIVE )
            {
                lp->buyer64bits = calc_nxt64bits(NXTaddr);
                expand_nxt64bits(seller,lp->seller64bits);
                retstr = AM_NXTorrent(NXTORRENT_MAKEOFFER,0,seller,lp->listing64bits,bidjsonstr);
                if ( is_valid_NXTtxid(retstr) != 0 )
                    lp->status = NXTORRENT_OFFERED;
                return(standard_AMretstr(lp->status,retstr));
            }
            else
            {
                sprintf(errstr,"{\"error\":\"cant make offer\",\"reason\":\"%s\"}",NXTorrent_status_str(lp->status));
                retstr = clonestr(errstr);
            }
        }
    }
    return(NXTorrent_retjson(-1,"nolisting",retstr,1));
}

char *acceptoffer_func(char *sender,int32_t valid,cJSON **objs,int32_t numobjs)
{
    char NXTaddr[64],buyer[64],instructions[1024],listingid[64],txbytes[1024],sighash[1024],buf[512],*retstr = 0;
    struct NXTorrent_listing *lp;
    copy_cJSON(NXTaddr,objs[0]);
    copy_cJSON(listingid,objs[1]);
    copy_cJSON(buyer,objs[2]);
    copy_cJSON(instructions,objs[3]);
    if ( NXTaddr[0] != 0 && listingid[0] != 0 && buyer[0] != 0 )
    {
        lp = get_listingid(listingid);
        if ( lp != 0 )
        {
            lp->buyer64bits = calc_nxt64bits(buyer);
            if ( lp->coinid == NXT_COINID && lp->amount > 0 )
                calc_raw_NXTtx(txbytes,sighash,lp->assetidbits,lp->amount,lp->buyer64bits);
            else calc_raw_NXTtx(txbytes,sighash,0,SATOSHIDEN,lp->buyer64bits);
            sprintf(buf,"{\"accept\":\"%s\",\"buyer\":\"%s\",\"txbytes\":\"%s\",\"sighash\":\"%s\",\"instructions\":\"%s\"}",listingid,buyer,txbytes,sighash,instructions);
            retstr = AM_NXTorrent(NXTORRENT_ACCEPT,0,buyer,lp->listing64bits,buf);
            if ( is_valid_NXTtxid(retstr) != 0 )
                lp->status = NXTORRENT_ACCEPTED;
            return(standard_AMretstr(lp->status,retstr));
        }
    }
    return(NXTorrent_retjson(-1,"nolisting",retstr,1));
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
            return(standard_AMretstr(lp->status,retstr));
        }
    }
    return(NXTorrent_retjson(-1,"nolisting",retstr,1));
}

char *NXTorrent_json_commands(struct NXTorrent_info *gp,cJSON *argjson,char *sender,int32_t valid)
{
    static char *changeurl[] = { (char *)changeurl_func, "changeurl", "", "URL", 0 };
    static char *listings[] = { (char *)listings_func, "listings", "", "NXT", "filters", 0 };
    static char *list[] = { (char *)list_func, "list", "", "NXT", "title", "price", "category1", "category2", "category3", "tag1", "tag2", "tag3", "URI", "description", "duration", "NXTsubatomic", 0 };
    static char *status[] = { (char *)status_func, "status", "", "NXT", "listingid", 0 };
    static char *cancel[] = { (char *)cancel_func, "cancel", "", "NXT", "listingid", 0 };
    static char *makeoffer[] = { (char *)makeoffer_func, "makeoffer", "", "NXT", "listingid", "bid", "comments", 0 };
    static char *acceptoffer[] = { (char *)acceptoffer_func, "acceptoffer", "", "NXT", "listingid", "buyer", "instructions", 0 };
    static char *feedback[] = { (char *)feedback_func, "feedback", "", "NXT", "listingid", "rating", "aboutbuyer", "aboutseller", 0 };
    static char **commands[] = { changeurl, listings, list, status, cancel, makeoffer, acceptoffer, feedback };
    int32_t i,j;
    cJSON *obj,*nxtobj,*objs[16];
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
        //printf("needvalid.(%c) sender.(%s) valid.%d %d of %d: cmd.(%s) vs command.(%s)\n",cmdinfo[2][0],sender,valid,i,(int32_t)(sizeof(commands)/sizeof(*commands)),cmdinfo[1],command);
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
       
        obj = gen_NXTaccts_json(0);
        if ( obj != 0 )
            cJSON_AddItemToObject(json,"NXTaccts",obj);
        retstr = cJSON_Print(json);
        free_json(json);
        return(retstr);
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
    uint64_t AMtxidbits = 0;
    struct json_AM *ap;
    char *sender,*receiver;
    sender = parms->sender; receiver = parms->receiver; ap = parms->AMptr;
    if ( (argjson = parse_json_AM(ap)) != 0 )
    {
        if ( parms->txid != 0 )
            AMtxidbits = calc_nxt64bits(parms->txid);
        printf("process_NXTorrent_AM got jsontxt.(%s) %s\n",ap->jsonstr,parms->txid);
        update_NXTorrent_state(dp,ap->funcid,ap->timestamp,argjson,ap->H.nxt64bits,sender,receiver,AMtxidbits);
        free_json(argjson);
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
    static int64_t nexttime;
    struct feedback_info *fb = 0;
    struct search_tag *tag = 0;
    struct NXTorrent_info *gp = handlerdata;
    struct NXTorrent_listing *lp = 0;
    if ( Global_NXTorrent != 0 )
    {
        if ( microseconds() < nexttime )
        {
            Global_NXTorrent->timestamp = issue_getTime();
            nexttime += 1000000;
        }
    }
    if ( parms->txid == 0 )     // indicates non-transaction event
    {
        if ( parms->mode == NXTPROTOCOL_WEBJSON )
            return(NXTorrent_jsonhandler(parms->argjson));
        else if ( parms->mode == NXTPROTOCOL_NEWBLOCK )
        {
            //printf("NXTorrent new RTblock %d time %I64d microseconds %ld\n",mp->RTflag,time(0),(long long)microseconds());
        }
        else if ( parms->mode == NXTPROTOCOL_IDLETIME )
        {
            //Global_NXTorrent->timestamp = issue_getTime();
            //printf("NXTorrent new idletime %d time %I64d microseconds %ld \n",mp->RTflag,time(0),(long long)microseconds());
        }
        else if ( parms->mode == NXTPROTOCOL_INIT )
        {
            printf("NXTorrent NXThandler_info init %d\n",mp->RTflag);
            gp = Global_NXTorrent = calloc(1,sizeof(*Global_NXTorrent));
            strcpy(gp->NXTADDR,mp->NXTADDR);
            strcpy(gp->NXTACCTSECRET,mp->NXTACCTSECRET);
            gp->listings = hashtable_create("listings",HASHTABLES_STARTSIZE,sizeof(*lp),((long)&lp->listingid[0] - (long)lp),sizeof(lp->listingid),((long)&lp->modified - (long)lp));
            gp->buyers = hashtable_create("buyers",HASHTABLES_STARTSIZE,sizeof(*fb),((long)&fb->NXTaddr[0] - (long)fb),sizeof(fb->NXTaddr),((long)&fb->modified - (long)fb));
            gp->sellers = hashtable_create("sellers",HASHTABLES_STARTSIZE,sizeof(*fb),((long)&fb->NXTaddr[0] - (long)fb),sizeof(fb->NXTaddr),((long)&fb->modified - (long)fb));
            gp->categories = hashtable_create("categories",HASHTABLES_STARTSIZE,sizeof(*tag),((long)&tag->key[0] - (long)tag),sizeof(tag->key),((long)&tag->modified - (long)tag));
            gp->tags = hashtable_create("tags",HASHTABLES_STARTSIZE,sizeof(*tag),((long)&tag->key[0] - (long)tag),sizeof(tag->key),((long)&tag->modified - (long)tag));
            portable_mutex_init(&gp->category_mutex);
            portable_mutex_init(&gp->tag_mutex);
            portable_mutex_init(&gp->buyer_mutex);
            portable_mutex_init(&gp->seller_mutex);
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
