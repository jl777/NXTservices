//
//  libwebsocketsglue.h
//  Created by jl777, April 5th, 2014
//  MIT License
//


#ifndef gateway_dispstr_h
#define gateway_dispstr_h

char dispstr[65536];

#include "NXTprotocol.c"

//#include "multigateway.h"
//#include "InstantDEX.h"
//struct NXThandler_info *Global_mp;

#define NXTPROTOCOL_HTMLFILE "/Users/jl777/Documents/NXT/gateway/gateway/NXTprotocol.html"
#define STUB_SIG 0x99999999
struct stub_info { char privatdata[10000]; }; struct stub_info *Global_dp;

char *stub_jsonhandler(cJSON *argjson)
{
    char *jsonstr;
    if ( argjson != 0 )
    {
        jsonstr = cJSON_Print(argjson);
        printf("stub_jsonhandler.(%s)\n",jsonstr);
        return(jsonstr);
    }
    return(0);
}

void process_stub_AM(struct stub_info *dp,struct NXT_protocol_parms *parms)
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
            printf("process_stub_AM got jsontxt.(%s)\n",jsontxt);
            argjson = cJSON_Parse(jsontxt);
            if ( argjson != 0 )
            {
                free_json(argjson);
            }
        }
    }
}

void process_stub_typematch(struct stub_info *dp,struct NXT_protocol_parms *parms)
{
    char NXTaddr[64],*sender,*receiver,*txid;
    sender = parms->sender; receiver = parms->receiver; txid = parms->txid;
    safecopy(NXTaddr,sender,sizeof(NXTaddr));
    printf("got txid.(%s) type.%d subtype.%d sender.(%s) -> (%s)\n",txid,parms->type,parms->subtype,sender,receiver);
}

void *stub_handler(struct NXThandler_info *mp,struct NXT_protocol_parms *parms,void *handlerdata)
{
    struct stub_info *dp = handlerdata;
    if ( parms->txid == 0 )     // indicates non-transaction event
    {
        if ( parms->mode == NXTPROTOCOL_WEBJSON )
            return(stub_jsonhandler(parms->argjson));
        else if ( parms->mode == NXTPROTOCOL_NEWBLOCK )
            printf("stub new RTblock %d time %ld microseconds %ld\n",mp->RTflag,time(0),(long)microseconds());
        else if ( parms->mode == NXTPROTOCOL_IDLETIME )
                 printf("stub new idletime %d time %ld microseconds %ld \n",mp->RTflag,time(0),(long)microseconds());
        else if ( parms->mode == NXTPROTOCOL_INIT )
        {
            printf("stub NXThandler_info init %d\n",mp->RTflag);
            dp = Global_dp = calloc(1,sizeof(*Global_dp));
        }
        return(dp);
    }
    else if ( parms->mode == NXTPROTOCOL_AMTXID )
        process_stub_AM(dp,parms);
    else if ( parms->mode == NXTPROTOCOL_TYPEMATCH )
        process_stub_typematch(dp,parms);
    return(dp);
}

void init_NXTprotocol()
{
    //static char *whitelist[] = { NXTACCTA, NXTACCTB, NXTACCTC, "" };
    struct NXThandler_info *mp = calloc(1,sizeof(*mp));    // seems safest place to have main data structure
    Global_mp = mp;
    init_NXTAPI();
    memset(mp,0,sizeof(*mp));
    safecopy(mp->ipaddr,get_ipaddr(),sizeof(mp->ipaddr));
#ifdef __APPLE__
    //safecopy(mp->ipaddr,"181.47.159.125",sizeof(mp->ipaddr));
#endif
    mp->upollseconds = 100000 * 0;
    mp->pollseconds = POLL_SECONDS;
    gen_randomacct(33,mp->NXTADDR,mp->NXTACCTSECRET,"randvals");
    //set_passwords(mp,0,0);
    safecopy(mp->NXTAPISERVER,NXTSERVER,sizeof(mp->NXTAPISERVER));

    //register_NXT_handler("InstantDEX",mp,2,-1,iDEXhandler,INSTANTDEX_SIG,1,0,0);
    //register_NXT_handler("multigateway",mp,2,-1,multigateway_handler,GATEWAY_SIG,1,0,whitelist);
    register_NXT_handler("stub",mp,-1,-1,stub_handler,STUB_SIG,1,0,0);//whitelist);
    mp->firsttimestamp = issue_getTime();
    start_NXTloops(mp,"14006431524478461177");//"12980781150532356708");
}


#endif
