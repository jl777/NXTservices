//
//  libwebsocketsglue.h
//  Created by jl777, April 5th, 2014
//  MIT License
//


#ifndef gateway_dispstr_h
#define gateway_dispstr_h

int URL_changed;
char dispstr[65536];
char testforms[1024*1024],PC_USERNAME[512],MY_IPADDR[512];
char NXTPROTOCOL_HTMLFILE[512] = { "/tmp/NXTprotocol.html" };

#include "NXTprotocol.c"
uv_loop_t *UV_loop;

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

#include "InstantDEX/InstantDEX.h"
#include "multigateway/multigateway.h"
#include "NXTorrent.h"
#include "NXTsubatomic.h"
#include "NXTcoinsco.h"
//#include "NXTmixer.h"
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
   // char *jsontxt;
    struct json_AM *ap;
    char NXTaddr[64],*sender,*receiver;
    sender = parms->sender; receiver = parms->receiver; ap = parms->AMptr; //txid = parms->txid;
    expand_nxt64bits(NXTaddr,ap->H.nxt64bits);
    if ( strcmp(NXTaddr,sender) != 0 )
    {
        printf("unexpected NXTaddr %s != sender.%s when receiver.%s\n",NXTaddr,sender,receiver);
        return;
    }
    if ( (argjson = parse_json_AM(ap)) != 0 )
    {
        printf("process_stub_AM got jsontxt.(%s)\n",ap->jsonstr);
        free_json(argjson);
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
            printf("stub new RTblock %d time %ld microseconds %lld\n",mp->RTflag,time(0),(long long)microseconds());
        else if ( parms->mode == NXTPROTOCOL_IDLETIME )
                 printf("stub new idletime %d time %ld microseconds %lld \n",mp->RTflag,time(0),(long long)microseconds());
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

void run_NXTprotocol(void *arg)
{
    struct NXThandler_info *mp = arg;
   // static char *argv[16];
    //static int argc = 0;
    static char *whitelist[] = { NXTISSUERACCT, NXTACCTA, NXTACCTB, NXTACCTC, "" };
    
    init_NXTAPI();
    safecopy(mp->ipaddr,MY_IPADDR,sizeof(mp->ipaddr));
    mp->upollseconds = 333333 * 0;
    mp->pollseconds = POLL_SECONDS;
    safecopy(mp->NXTAPISERVER,NXTSERVER,sizeof(mp->NXTAPISERVER));
    gen_randomacct(33,mp->NXTADDR,mp->NXTACCTSECRET,"randvals");
    crypto_box_keypair(Global_mp->session_pubkey,Global_mp->session_privkey);
    init_hexbytes(Global_mp->pubkeystr,Global_mp->session_pubkey,sizeof(Global_mp->session_pubkey));
    mp->accountjson = issue_getAccountInfo(&Global_mp->acctbalance,mp->dispname,PC_USERNAME,mp->NXTADDR,mp->groupname);
    printf("(%s) (%s) (%s) (%s) [%s]\n",mp->dispname,PC_USERNAME,mp->NXTADDR,mp->groupname,mp->NXTACCTSECRET);
    mp->myind = -1;
    mp->nxt64bits = calc_nxt64bits(mp->NXTADDR);
    /*argv[argc++] = "punch";
    stripwhite(mp->dispname,strlen(mp->dispname));
    argv[argc++] = "-u", argv[argc++] = mp->dispname;
    argv[argc++] = "-n", argv[argc++] = mp->NXTADDR;
    if ( mp->groupname[0] != 0 )
        argv[argc++] = "-g", argv[argc++] = mp->groupname;*/
    init_NXThashtables(mp);
    if ( portable_thread_create(process_hashtablequeues,mp) == 0 )
        printf("ERROR hist process_hashtablequeues\n");
    if ( portable_thread_create(run_NXTsync,EMERGENCY_PUNCH_SERVER) == 0 )
        printf("ERROR run_NXTsync\n");
    sleep(3);
    printf(">>>>>>>>>>>>>>> %s: %s %s NXT.(%s)\n",mp->dispname,PC_USERNAME,mp->ipaddr,mp->NXTADDR);
    if ( 1 )
    {
        register_NXT_handler("NXTcoinsco",mp,NXTPROTOCOL_ILLEGALTYPE,NXTPROTOCOL_ILLEGALTYPE,NXTcoinsco_handler,NXTCOINSCO_SIG,1,0,0);
        register_NXT_handler("NXTorrent",mp,NXTPROTOCOL_ILLEGALTYPE,NXTPROTOCOL_ILLEGALTYPE,NXTorrent_handler,NXTORRENT_SIG,1,0,0);
        SUPPRESS_MULTIGATEWAY = 1;
        register_NXT_handler("subatomic",mp,NXTPROTOCOL_ILLEGALTYPE,NXTPROTOCOL_ILLEGALTYPE,subatomic_handler,SUBATOMIC_SIG,1,0,0);
        register_NXT_handler("multigateway",mp,2,-1,multigateway_handler,GATEWAY_SIG,1,0,whitelist);
    }
    //register_NXT_handler("InstantDEX",mp,NXTPROTOCOL_ILLEGALTYPE,NXTPROTOCOL_ILLEGALTYPE,iDEXhandler,INSTANTDEX_SIG,1,0,0);
    //register_NXT_handler("stub",mp,-1,-1,stub_handler,STUB_SIG,1,0,0);//whitelist);
    mp->firsttimestamp = issue_getTime();
    
    //start_NXTloops(mp,"16787696303645624065"); has subatomic and atomic trades setup
    //start_NXTloops(mp,"1238805504845503558");
    static struct NXThandler_info histM;
    char *histstart = 0;//"16787696303645624065";
    //mp->RTmp = mp;
    //printf("start_NXTloops\n");
    portable_mutex_init(&mp->hash_mutex);
    portable_mutex_init(&mp->hashtable_queue[0].mutex);
    portable_mutex_init(&mp->hashtable_queue[1].mutex);
    if ( histstart != 0 && histstart[0] != 0 )
    {
        histM = *mp;
        histM.origblockidstr = histstart;
        histM.histmode = 1;
        init_NXThashtables(&histM);
        if ( portable_thread_create(NXTloop,&histM) == 0 )
            printf("ERROR start_Histloop\n");
    } else Historical_done = 1;
    NXTloop(mp);
    printf("start_NXTloops done\n");
    while ( 1 ) sleep(60);
}

void run_UVloop(void *arg)
{
    uv_idle_t idler;
    uv_idle_init(UV_loop,&idler);
    uv_idle_start(&idler,NXTprotocol_idler);
    uv_run(UV_loop,UV_RUN_DEFAULT);
    printf("end of uv_run\n");
}

void init_NXTprotocol(int _argc,char **_argv)
{
    struct NXThandler_info *mp = calloc(1,sizeof(*mp));    // seems safest place to have main data structure
    Global_mp = mp;
    strcpy(MY_IPADDR,get_ipaddr()), strcpy(mp->ipaddr,MY_IPADDR);
    UV_loop = uv_default_loop();
    if ( _argc >= 2 && (strcmp(_argv[1],"server") == 0 || strcmp(mp->ipaddr,EMERGENCY_PUNCH_SERVER) == 0) )
    {
#ifndef WIN32
        punch_server_main(_argc-1,_argv+1);
        printf("hole punch server done\n");
        exit(0);
#endif
    }
    else
    {
        if ( portable_thread_create(run_NXTprotocol,mp) == 0 )
            printf("ERROR hist process_hashtablequeues\n");
    }
    if ( portable_thread_create(run_UVloop,mp) == 0 )
        printf("ERROR hist process_hashtablequeues\n");
 }


#endif
