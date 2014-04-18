//
//  libwebsocketsglue.h
//  Created by jl777, April  2014
//  MIT License
//


#ifndef gateway_dispstr_h
#define gateway_dispstr_h

int URL_changed;
char dispstr[65536];
char testforms[1024*1024];
char NXTPROTOCOL_HTMLFILE[512] = { "/Users/jl777/gateway/NXTprotocol.html" };

#include "NXTprotocol.c"

char *changeurl_func(char *sender,int32_t valid,cJSON **objs,int32_t numobjs)
{
    extern char NXTPROTOCOL_HTMLFILE[512];
    char URL[64],*retstr = 0;
    copy_cJSON(URL,objs[0]);
    if ( URL[0] != 0 )
    {
        URL_changed = 1;
        strcpy(NXTPROTOCOL_HTMLFILE,URL);
        testforms[0] = 0;
        retstr = clonestr(URL);
    }
    return(retstr);
}

#include "InstantDEX.h"
#include "multigateway.h"
#include "NXTorrent.h"
#include "NXTsubatomic.h"
#include "nodecoin.h"
#include "NXTmixer.h"
#include "html.h"

#define STUB_SIG 0x99999999
struct stub_info { char privatdata[10000]; };
struct stub_info *Global_stub;

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
            dp = Global_stub = calloc(1,sizeof(*Global_stub));
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
    static char *whitelist[] = { NXTISSUERACCT, NXTACCTA, NXTACCTB, NXTACCTC, "" };

    struct NXThandler_info *mp = calloc(1,sizeof(*mp));    // seems safest place to have main data structure
    Global_mp = mp;
    init_NXTAPI();
    memset(mp,0,sizeof(*mp));
    safecopy(mp->ipaddr,get_ipaddr(),sizeof(mp->ipaddr));
    //mp->upollseconds = 1000 * 1;
    mp->pollseconds = POLL_SECONDS;
    //set_passwords(mp,0,0);
    safecopy(mp->NXTAPISERVER,NXTSERVER,sizeof(mp->NXTAPISERVER));
    
    gen_randomacct(33,mp->NXTADDR,mp->NXTACCTSECRET,"randvals");
    printf(">>>>>>>>>>>>>>> %s NXT.(%s)\n",mp->ipaddr,mp->NXTADDR);
    register_NXT_handler("NXTorrent",mp,NXTPROTOCOL_ILLEGALTYPE,NXTPROTOCOL_ILLEGALTYPE,NXTorrent_handler,NXTORRENT_SIG,1,0,0);
    //register_NXT_handler("multigateway",mp,2,-1,multigateway_handler,GATEWAY_SIG,1,0,whitelist);
    //register_NXT_handler("InstantDEX",mp,NXTPROTOCOL_ILLEGALTYPE,NXTPROTOCOL_ILLEGALTYPE,iDEXhandler,INSTANTDEX_SIG,1,0,0);
    //register_NXT_handler("subatomic",mp,NXTPROTOCOL_ILLEGALTYPE,NXTPROTOCOL_ILLEGALTYPE,subatomic_handler,SUBATOMIC_SIG,1,0,0);
    //register_NXT_handler("stub",mp,-1,-1,stub_handler,STUB_SIG,1,0,0);//whitelist);
    mp->firsttimestamp = issue_getTime();
    start_NXTloops(mp,"2408234822244879292");//"4475075929652596255");//"14006431524478461177");//"12980781150532356708");
}


#endif
