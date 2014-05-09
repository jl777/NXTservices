//
//  NXThealth.h
//  Created by jl777, May 2014
//  MIT License
//

#ifndef gateway_NXThealth_h
#define gateway_NXThealth_h

/*
marcus03
I remembered that CfB and BCNext were monitoring the NXT network when we were under DDOS and asked him how they did it. Here's his answer:
"We had nodes in different datacenters. All these nodes were connected to Nxt network (to different peers!) and had direct connections to each other. Once a node received a block or an unconfirmed transaction it sent this to other nodes (not Nxt peers). The others measured time between the signal and time when they received the same block/transaction from Nxt peers. The delay is the main indicator that shows time of network convergence. During DDoS attacks average convergence time becomes much higher. Forks r also easily detected.
We also measure load on other peers by sending transactions to one of them from one node and receiving on another one. U can easily extended this approach and add extra functionality.
We used tool written by BCNext and I can't share it without his explicit permission, which is hard to get coz I'm not in touch with him anymore."
*/

#define NXTCOINSCO_SIG 0x74924664

struct NXThealth_info
{
    uint64_t lastblock;
    int64_t lastblocktime;
    uint32_t peers[20];
    char state[500];
};

struct NXTcoinsco_info
{
    int64_t nodecoins_avail;
    portable_udp_t usock;
    struct sockaddr pooladdr;
    struct NXThealth_info health;
} *Global_NXTcoinsco;


void calc_NXThealth_info(struct NXThealth_info *health)
{
   /* {
    "time": 14213073,
    "numberOfTransactions": 165562,
    "version": "1.0.0",
   "numberOfUnlockedAccounts": 0,
    "cumulativeDifficulty": "2370717994042371",
    "numberOfAccounts": 25250
       "lastBlock": "16984484179601871765",
        "numberOfAliases": 61399,
        "lastBlockchainFeeder": "209.126.73.168",
        "numberOfPeers": 41,
        "numberOfBlocks": 90846,
        //"totalMemory": 1592262656,
        //"freeMemory": 643891040,
        //"maxMemory": 3817865216,
        //"totalEffectiveBalanceNXT": 945816059,
         //"numberOfOrders": 4651,
        //"numberOfVotes": 0,
        //"numberOfTrades": 1797,
        //"availableProcessors": 8,
        //"numberOfAssets": 729,
        //"numberOfPolls": 2,
    }*/
    // get peers
    health->lastblocktime = microseconds();
    cJSON *json;
    char i,cmd[4096],*jsonstr;
    memset(health,0,sizeof(*health));   // save some fields for prev
    for (i=0; i<2; i++)
    {
        if ( i == 0 )
            sprintf(cmd,"%s=getState",NXTSERVER);
        else sprintf(cmd,"%s=getPeers",NXTSERVER);
        jsonstr = issue_curl(cmd);
        if ( jsonstr != 0 )
        {
            if ( (json= cJSON_Parse(jsonstr)) != 0 )
            {
                if ( i == 1 ) // do peers
                {
                    
                }
                else
                {
                    
                }
                free_json(json);
            }
            free(jsonstr);
        }
    }
}

int64_t get_nodecoins_avail()
{
    if ( Global_NXTcoinsco != 0 )
        return(Global_NXTcoinsco->nodecoins_avail);
    else return(0);
}

void update_NXTcoinsco_state(struct NXTcoinsco_info *gp,int32_t funcid,int32_t timestamp,cJSON *argjson,uint64_t nxt64bits,char *sender,char *receiver)
{
    
}

char *cashout_nodecoins_func(char *sender,int32_t valid,cJSON **objs,int32_t numobjs)
{
    portable_tcp_t tcp;
    char *retstr = 0;
    char signedtx[1024],cashout_txid[128];
    struct NXT_tx *stx,U;
    set_NXTtx(&U,0,MIN_NQTFEE,calc_nxt64bits(NODECOIN_POOLSERVER_ADDR));
    stx = sign_NXT_tx(signedtx,&U,0,1.);
    if ( stx !=0 )
        free(stx);
    if ( portable_tcp_send(&tcp,signedtx,(int32_t)strlen(signedtx)+1) < 0 )
        retstr = clonestr("{\"error\":\"couldnt contact nodecoin poolserver\"}");
    else if ( portable_tcp_read(&tcp,cashout_txid,(int32_t)sizeof(cashout_txid),1000000) <= 0 )
        retstr = clonestr("{\"error\":\"couldnt get cashout txid\"}");
    return(retstr);
}

char *NXTcoinsco_json_commands(struct NXTcoinsco_info *gp,cJSON *argjson,char *sender,int32_t valid)
{
    static char *cashout_nodecoins[] = { (char *)cashout_nodecoins_func, "cashout_nodecoins", "", "NXT", 0 };
    static char **commands[] = { cashout_nodecoins };
    int32_t i,j;
    cJSON *obj,*nxtobj,*objs[16];
    char NXTaddr[64],command[4096],**cmdinfo;
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

char *NXTcoinsco_jsonhandler(cJSON *argjson)
{
    struct NXTcoinsco_info *gp = Global_NXTcoinsco;
    long len;
    int32_t valid;
    cJSON *parmsobj,*tokenobj,*secondobj;
    char sender[64],*parmstxt,encoded[NXT_TOKEN_LEN],*retstr = 0;
    sender[0] = 0;
    valid = -1;
    printf("NXTcoinsco_jsonhandler argjson.%p\n",argjson);
    if ( argjson == 0 )
        return(0);
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
    if ( sender[0] == 0 )
        strcpy(sender,Global_mp->NXTADDR);
    retstr = NXTcoinsco_json_commands(gp,argjson,sender,valid);
    return(retstr);
}

void process_NXTcoinsco_AM(struct NXTcoinsco_info *gp,struct NXT_protocol_parms *parms)
{
    cJSON *argjson;
    //char *jsontxt;
    struct json_AM *ap;
    char *sender,*receiver;
    sender = parms->sender; receiver = parms->receiver; ap = parms->AMptr; //txid = parms->txid;
    if ( (argjson = parse_json_AM(ap)) != 0 && (strcmp(Global_mp->NXTADDR,sender) == 0 || strcmp(Global_mp->NXTADDR,receiver) == 0) )
    {
        printf("process_NXTcoinsco_AM got jsontxt.(%s)\n",ap->jsonstr);
        update_NXTcoinsco_state(gp,ap->funcid,ap->timestamp,argjson,ap->H.nxt64bits,sender,receiver);
        free_json(argjson);
    }
}

void process_NXTcoinsco_typematch(struct NXTcoinsco_info *dp,struct NXT_protocol_parms *parms)
{
    char NXTaddr[64],*sender,*receiver,*txid;
    sender = parms->sender; receiver = parms->receiver; txid = parms->txid;
    safecopy(NXTaddr,sender,sizeof(NXTaddr));
    printf("got txid.(%s) type.%d subtype.%d sender.(%s) -> (%s)\n",txid,parms->type,parms->subtype,sender,receiver);
}

struct NXTcoinsco_info *NXTcoinsco_init()
{
    Global_NXTcoinsco = calloc(1,sizeof(struct NXTcoinsco_info));
    portable_udpsocket(&Global_NXTcoinsco->usock,NXTCOINSCO_PORT,1);
    (void)server_address(POOLSERVER,NXTCOINSCO_PORT,(struct sockaddr_in *)&Global_NXTcoinsco->pooladdr);
    return(Global_NXTcoinsco);
}

void NXTcoinsco_idletime(struct NXTcoinsco_info *gp)
{
}

void NXTcoinsco_newblock(struct NXTcoinsco_info *gp)
{
    calc_NXThealth_info(&gp->health);
    portable_udpsend(&gp->usock,&gp->health,sizeof(gp->health),&gp->pooladdr);
}

void *NXTcoinsco_handler(struct NXThandler_info *mp,struct NXT_protocol_parms *parms,void *handlerdata)
{
    struct NXTcoinsco_info *gp = handlerdata;
    if ( parms->txid == 0 )     // indicates non-transaction event
    {
        if ( parms->mode == NXTPROTOCOL_WEBJSON )
            return(NXTcoinsco_jsonhandler(parms->argjson));
        else if ( parms->mode == NXTPROTOCOL_NEWBLOCK )
        {
            //printf("subatomic new RTblock %d time %lld microseconds %ld\n",mp->RTflag,time(0),microseconds());
            NXTcoinsco_newblock(gp);
        }
        else if ( parms->mode == NXTPROTOCOL_IDLETIME )
        {
            NXTcoinsco_idletime(gp);
        }
        else if ( parms->mode == NXTPROTOCOL_INIT )
        {
            printf("NXTcoinsco NXThandler_info init %d\n",mp->RTflag);
            gp = Global_NXTcoinsco = NXTcoinsco_init();
        }
        return(gp);
    }
    else if ( parms->mode == NXTPROTOCOL_AMTXID )
        process_NXTcoinsco_AM(gp,parms);
    else if ( parms->mode == NXTPROTOCOL_TYPEMATCH )
        process_NXTcoinsco_typematch(gp,parms);
    return(gp);
}

#endif
