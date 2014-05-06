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

//#include "nodecoin.h"
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

void init_NXTprotocol(int _argc,char **_argv)
{
    static char *argv[16]; int argc = 0;
    static char *whitelist[] = { NXTISSUERACCT, NXTACCTA, NXTACCTB, NXTACCTC, "" };
    
    struct NXThandler_info *mp = calloc(1,sizeof(*mp));    // seems safest place to have main data structure
    Global_mp = mp;
    
    strcpy(MY_IPADDR,get_ipaddr());
    init_NXTAPI();
    memset(mp,0,sizeof(*mp));

    safecopy(mp->ipaddr,MY_IPADDR,sizeof(mp->ipaddr));
    mp->upollseconds = 333333 * 0;
    mp->pollseconds = POLL_SECONDS;
    safecopy(mp->NXTAPISERVER,NXTSERVER,sizeof(mp->NXTAPISERVER));
    gen_randomacct(33,mp->NXTADDR,mp->NXTACCTSECRET,"randvals");
    crypto_box_keypair(Global_mp->session_pubkey,Global_mp->session_privkey);
    init_hexbytes(Global_mp->pubkeystr,Global_mp->session_pubkey,sizeof(Global_mp->session_pubkey));
    mp->accountjson = issue_getAccountInfo(&Global_mp->acctbalance,mp->dispname,PC_USERNAME,mp->NXTADDR,mp->groupname);
    printf("argv.0 %s, argv.1 %s (%s) (%s) (%s) (%s) [%s]\n",_argv[0],_argv[1],mp->dispname,PC_USERNAME,mp->NXTADDR,mp->groupname,mp->NXTACCTSECRET);
 
    //printf("%s\n",issue_signTransaction("0000be25c6009a0225c5fed2690701cf06f267e7c227b1a3c0dfa9c6fc3cdb593b3af6f16d65302f9b30f378f284e10500e1f5050000000000e1f505000000000000000000000000000000000000000000000000000000000000000000000000")); getchar();

    mp->myind = -1;
    mp->nxt64bits = calc_nxt64bits(mp->NXTADDR);
    argv[argc++] = "punch";
    stripwhite(mp->dispname,strlen(mp->dispname));
    argv[argc++] = "-u", argv[argc++] = mp->dispname;
    argv[argc++] = "-n", argv[argc++] = mp->NXTADDR;
    if ( mp->groupname[0] != 0 )
        argv[argc++] = "-g", argv[argc++] = mp->groupname;
    init_NXThashtables(mp);
    if ( pthread_create(malloc(sizeof(pthread_t)),NULL,process_hashtablequeues,mp) != 0 )
        printf("ERROR hist process_hashtablequeues\n");
    if ( _argc < 2 || strcmp(_argv[1],"punch") != 0 )
    {
        char cmd[1024]; 
        if ( _argc >= 2 && (strcmp(_argv[1],"server") == 0 || strcmp(mp->ipaddr,EMERGENCY_PUNCH_SERVER) == 0) )
        {
#ifndef WIN32
           punch_server_main(_argc-1,_argv+1);
            printf("hole punch server done\n"); exit(0);
#endif
        }
        else if ( 0 )
        {
            launch_app_in_new_terminal(_argv[0],argc,argv);
            sprintf(cmd,"say -v Vicki hello %s %s",mp->dispname,mp->groupname[0]==0?"please input groupname":"");
#ifdef __APPLE__
            //system(cmd);
#endif
        }
    }
    else if ( 0 )
    {
#ifndef WIN32
        if ( pthread_create(malloc(sizeof(pthread_t)),NULL,punch_server_glue,argv) != 0 )
            printf("ERROR punch_server_glue\n");
        punch_client_main(argc,argv);
        printf("chat child done\n"); while(1) sleep(1);
#endif
    }
    if ( pthread_create(malloc(sizeof(pthread_t)),NULL,run_NXTsync,EMERGENCY_PUNCH_SERVER) != 0 )
        printf("ERROR run_NXTsync\n");
   // if ( pthread_create(malloc(sizeof(pthread_t)),NULL,process_syncmem_queue_loop,EMERGENCY_PUNCH_SERVER) != 0 )
    //    printf("ERROR process_syncmem_queue_loop\n");
   // sleep(60);
    sleep(3);
    printf(">>>>>>>>>>>>>>> %s: %s %s NXT.(%s)\n",mp->dispname,PC_USERNAME,mp->ipaddr,mp->NXTADDR);
    register_NXT_handler("NXTorrent",mp,NXTPROTOCOL_ILLEGALTYPE,NXTPROTOCOL_ILLEGALTYPE,NXTorrent_handler,NXTORRENT_SIG,1,0,0);
    SUPPRESS_MULTIGATEWAY = 1;
    register_NXT_handler("subatomic",mp,NXTPROTOCOL_ILLEGALTYPE,NXTPROTOCOL_ILLEGALTYPE,subatomic_handler,SUBATOMIC_SIG,1,0,0);
    register_NXT_handler("multigateway",mp,2,-1,multigateway_handler,GATEWAY_SIG,1,0,whitelist);
    //register_NXT_handler("InstantDEX",mp,NXTPROTOCOL_ILLEGALTYPE,NXTPROTOCOL_ILLEGALTYPE,iDEXhandler,INSTANTDEX_SIG,1,0,0);
    //register_NXT_handler("stub",mp,-1,-1,stub_handler,STUB_SIG,1,0,0);//whitelist);
    mp->firsttimestamp = issue_getTime();
    
    //start_NXTloops(mp,"16787696303645624065"); has subatomic and atomic trades setup
    start_NXTloops(mp,"1238805504845503558");
}


#endif
