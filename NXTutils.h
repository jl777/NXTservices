
//  Created by jl777
//  MIT License
//

#ifndef gateway_NXTutils_h
#define gateway_NXTutils_h


char *load_file(char *fname,char **bufp,int64_t  *lenp,int64_t  *allocsizep)
{
    FILE *fp;
    int64_t  filesize,buflen = *allocsizep;
    char *buf = *bufp;
    *lenp = 0;
    if ( (fp= fopen(fname,"rb")) != 0 )
    {
        fseek(fp,0,SEEK_END);
        filesize = ftell(fp);
        if ( filesize == 0 )
        {
            fclose(fp);
            *lenp = 0;
            return(0);
        }
        if ( filesize > buflen-1 )
        {
            *allocsizep = filesize+1;
            *bufp = buf = realloc(buf,(long)*allocsizep);
            //buflen = filesize+1;
        }
        rewind(fp);
        if ( buf == 0 )
            printf("Null buf ???\n");
        else
        {
            if ( fread(buf,1,(long)filesize,fp) != (unsigned long)filesize )
                printf("error reading filesize.%ld\n",(long)filesize);
            buf[filesize] = 0;
        }
        fclose(fp);
        
        *lenp = filesize;
    }
    return(buf);
}

#ifndef WIN32
char *_issue_cmd_to_buffer(char *prog,char *arg1,char *arg2,char *arg3)
{
    char buffer[4096],cmd[512];
    int32_t fd[2];
    unsigned long len;
	static pthread_mutex_t mutex;
 	pthread_mutex_lock(&mutex);
    if ( pipe(fd) != 0 )
        printf("_issue_cmd_to_buffer error doing pipe(fd)\n");
    pid_t pid = fork();
    if ( pid == 0 )
    {
        dup2(fd[1],STDOUT_FILENO);
        close(fd[0]);
        sprintf(cmd,"%s %s %s %s",prog,arg1,arg2,arg3);
#ifdef DEBUG_MODE
        if ( strcmp(arg1,"getinfo") != 0 && strcmp(arg1,"getrawtransaction") != 0 &&
            strcmp(arg1,"decoderawtransaction") != 0 && strcmp(arg1,"validateaddress") != 0 &&
            strcmp(arg1,"getaccountaddress") != 0 )
            fprintf(stderr,"ISSUE.(%s)\n",cmd);
#endif
        if ( system(cmd) != 0 )
            printf("error issuing system(%s)\n",cmd);
        exit(0);
    }
    else
    {
        dup2(fd[0],STDIN_FILENO);
        close(fd[1]);
        len = 0;
        buffer[0] = 0;
        while ( fgets(buffer+len,(int)(sizeof(buffer)-len),stdin) != 0 )
        {
            //printf("%s",buffer+len);
            len += strlen(buffer+len);
        }
        waitpid(pid,NULL,0);
        while ( len > 2 && (buffer[len-1] == '\r' || buffer[len-1] == '\n' || buffer[len-1] == '\t' || buffer[len-1] == ' ') )
            len--;
        buffer[len] = 0;
        while ( fgetc(stdin) != EOF )
            ;
        pthread_mutex_unlock(&mutex);
        if ( len > 0 )
            return(clonestr(buffer));
    }
    return(0);
}

char *oldget_ipaddr()
{
    static char *ipaddr;
    char *devs[] = { "eth0", "em1" };
    int32_t iter;
    char *retstr,*tmp;
    if ( ipaddr != 0 )
        return(ipaddr);
    for (iter=0; iter<(int)((sizeof(devs))/(sizeof(*devs))); iter++)
    {
        printf("iter.%d\n",iter);
        retstr = _issue_cmd_to_buffer("ifconfig",devs[iter],"| grep \"inet addr\" | awk '{print $2}'","");
        if ( retstr != 0 && strncmp(retstr,"addr:",strlen("addr:")) == 0 )
        {
            tmp = clonestr(retstr + strlen("addr:"));
            myfree(retstr,"1");
            if ( ipaddr != 0 )
                myfree(ipaddr,"12");
            ipaddr = tmp;
            return(ipaddr);
        } else ipaddr = 0;
    }
    ipaddr = clonestr("127.0.0.1");
    return(ipaddr);
}
#endif

char *get_ipaddr()
{
    static char *_ipaddr;
    char *ipsrcs[] = { "http://ip-api.com/json", "http://ip.jsontest.com/?showMyIP", "http://www.telize.com/jsonip", "http://www.trackip.net/ip?json"};
    int32_t i,match = 1;
    cJSON *json,*obj;
    char *jsonstr,ipaddr[512],str[512];
    if ( _ipaddr != 0 )
        return(_ipaddr);
    ipaddr[0] = 0;
    for (i=0; i<(int)(sizeof(ipsrcs)/sizeof(*ipsrcs)); i++)
    {
        jsonstr = issue_curl(ipsrcs[i]);
        if ( jsonstr != 0 )
        {
            json = cJSON_Parse(jsonstr);
            if ( json != 0 )
            {
                obj = cJSON_GetObjectItem(json,(i==0)?"query":"ip");
                copy_cJSON(str,obj);
                printf("(%s) ",str);
                if ( strcmp(str,ipaddr) != 0 )
                    strcpy(ipaddr,str);
                else match++;
                free_json(json);
            }
            free(jsonstr);
        }
    }
    if ( match != i )
        printf("-> (%s) ipaddr matches.%d vs queries.%d\n",ipaddr,match,i);
    if ( ipaddr[0] != 0 )
        _ipaddr = clonestr(ipaddr);
    return(_ipaddr);
}

char *choose_poolserver(char *NXTaddr)
{
    static int32_t lastind = -1;
    uint64_t hashval;
    //return(SERVER_NAMEA);
    while ( 1 )
    {
        if ( lastind == -1 )
        {
            hashval = calc_decimalhash(NXTaddr);
            lastind = (hashval % Numguardians);
        }
        else lastind = ((lastind+1) % Numguardians);
        if ( Guardian_names[lastind][0] != 0 )
            break;
    }
    return(Guardian_names[lastind]);
}

union NXTtype extract_NXTfield(char *origoutput,char *cmd,char *field,int32_t type)
{
    char *jsonstr,*output,NXTaddr[MAX_NXTADDR_LEN];
    cJSON *json,*obj;
    union NXTtype retval;
    retval.nxt64bits = 0;
    if ( origoutput == 0 )
        output = NXTaddr;
    else output = origoutput;
    jsonstr = issue_curl(cmd);
    if ( jsonstr != 0 )
    {
        //if ( field != 0 && strcmp(field,"transactionId") == 0 )
        //    printf("jsonstr.(%s)\n",jsonstr);
        json = cJSON_Parse(jsonstr);
        if ( json == 0 ) printf("Error before: (%s) -> [%s]\n",jsonstr,cJSON_GetErrorPtr());
        else
        {
            if ( field == 0 )
            {
                if ( origoutput == 0 )
                    retval.json = json;
                else
                {
                    copy_cJSON(origoutput,json);
                    retval.str = origoutput;
                    free_json(json);
                }
            }
            else
            {
                obj = cJSON_GetObjectItem(json,field);
                if ( obj != 0 )
                {
                    copy_cJSON(output,obj);
                    //if ( strcmp(field,"transactionId") == 0 )
                    //    printf("obj.(%s) type.%d\n",output,type);
                    switch ( type )
                    {
                        case sizeof(double):
                            retval.dval = atof(output);
                            break;
                        case sizeof(uint32_t):
                            retval.uval = atoi(output);
                            break;
                        case -(int)sizeof(int32_t):
                            retval.val = atoi(output);
                            break;
                        case -(int)sizeof(int64_t):
                            retval.lval = atol(output);
                            break;
                        case 64:
                            retval.nxt64bits = calc_nxt64bits(output);
                            //if ( strcmp(field,"transactionId") == 0 )
                            //    printf("transactionId.%s\n",nxt64str(retval.nxt64bits));
                           break;
                        case 0:
                            if ( origoutput != 0 )
                                retval.str = origoutput;
                            else retval.str = 0;
                            break;
                        default: printf("extract_NXTfield: warning unknown type.%d\n",type);
                    }
                }
                free_json(json);
            }
        }
        myfree(jsonstr,"33");
    }
    return(retval);
}

int32_t issue_getTime()
{
    char cmd[4096];
    union NXTtype ret;
    sprintf(cmd,"%s=getTime",NXTSERVER);
    ret = extract_NXTfield(0,cmd,"time",sizeof(int32_t));
    return(ret.val);
}

uint64_t issue_getAccountId(char *password)
{
    char cmd[4096];
    union NXTtype ret;
    sprintf(cmd,"%s=getAccountId&secretPhrase=%s",NXTSERVER,password);
    //printf("(%s) ->(%s)\n",cmd,issue_curl(cmd));
    ret = extract_NXTfield(0,cmd,"accountId",64);
    return(ret.nxt64bits);
}

int32_t issue_startForging(char *secret)
{
    char cmd[4096];
    union NXTtype ret;
    sprintf(cmd,"%s=startForging&secretPhrase=%s",NXTSERVER,secret);
    ret = extract_NXTfield(0,cmd,"deadline",sizeof(int32_t));
    return(ret.val);
}

char *issue_broadcastTransaction(char *txbytes)
{
    char cmd[4096];
    sprintf(cmd,"%s=broadcastTransaction&secretPhrase=%s&transactionBytes=%s",NXTSERVER,Global_mp->NXTACCTSECRET,txbytes);
    return(issue_curl(cmd));
}


char *issue_signTransaction(char *txbytes)
{
    char cmd[4096];
    sprintf(cmd,"%s=signTransaction&secretPhrase=%s&unsignedTransactionBytes=%s",NXTSERVER,Global_mp->NXTACCTSECRET,txbytes);
    return(issue_curl(cmd));
}

uint64_t issue_transferAsset(char *secret,char *recipient,char *asset,int64_t quantity,int64_t feeNQT,int32_t deadline,char *comment)
{
    char cmd[4096],*str,*jsontxt;
    uint64_t txid = 0;
    cJSON *json;
    sprintf(cmd,"%s=transferAsset&secretPhrase=%s&recipient=%s&asset=%s&quantityQNT=%ld&feeNQT=%ld&deadline=%d",NXTSERVER,secret,recipient,asset,(long)quantity,(long)feeNQT,deadline);
    if ( comment != 0 )  
        strcat(cmd,"&comment="),strcat(cmd,comment);
    jsontxt = issue_curl(cmd);
    if ( jsontxt != 0 )
    {
     printf(" issuing asset.(%s) -> %s\n",cmd,jsontxt);
        //if ( field != 0 && strcmp(field,"transactionId") == 0 )
        //    printf("jsonstr.(%s)\n",jsonstr);
        json = cJSON_Parse(jsontxt);
        if ( json != 0 )
        {
            txid = get_satoshi_obj(json,"transaction");
            if ( txid == 0 )
            {
                str = cJSON_Print(json);
                printf("ERROR WITH ASSET TRANSFER.(%s) -> \n%s\n",cmd,str);
                free(str);
            }
            free_json(json);
        } else printf("error issuing asset.(%s) -> %s\n",cmd,jsontxt);
        free(jsontxt);
    }
    return(txid);
}

cJSON *issue_getAccountInfo(int64_t *amountp,char *name,char *username,char *NXTaddr,char *groupname)
{
    union NXTtype retval;
    cJSON *obj,*json = 0;
    char buf[2048];
    *amountp = 0;
    sprintf(buf,"%s=getAccount&account=%s",NXTSERVER,NXTaddr);
    retval = extract_NXTfield(0,buf,0,0);
    if ( retval.json != 0 )
    {
        printf("%s\n",cJSON_Print(retval.json));
        obj = cJSON_GetObjectItem(retval.json,"balanceNQT");
        copy_cJSON(buf,obj);
        *amountp = atol(buf);
        obj = cJSON_GetObjectItem(retval.json,"name");
        copy_cJSON(name,obj);
        obj = cJSON_GetObjectItem(retval.json,"description");
        copy_cJSON(buf,obj);
        replace_backslashquotes(buf);
        json = cJSON_Parse(buf);
        if ( json != 0 )
        {
            //printf("%s\n",cJSON_Print(json));
            obj = cJSON_GetObjectItem(json,"username");
            copy_cJSON(username,obj);
            obj = cJSON_GetObjectItem(json,"group");
            copy_cJSON(groupname,obj);
            printf("name.%s username.%s groupname.%s\n%s\n",name,username,cJSON_Print(json),groupname);
        }
        free_json(retval.json);
    }
    return(json);
}

int64_t get_coin_quantity(int64_t *unconfirmedp,int32_t coinid,char *NXTaddr)
{
    char *assetid_str(int32_t coinid);
    char cmd[4096],assetid[512],*assetstr;
    union NXTtype retval;
    int32_t i,n,iter;
    cJSON *array,*item,*obj;
    int64_t quantity,qty;
    quantity = *unconfirmedp = 0;
    sprintf(cmd,"%s=getAccount&account=%s",NXTSERVER,NXTaddr);
    retval = extract_NXTfield(0,cmd,0,0);
    if ( retval.json != 0 )
    {
        assetstr = assetid_str(coinid);
        for (iter=0; iter<2; iter++)
        {
            qty = 0;
            array = cJSON_GetObjectItem(retval.json,iter==0?"assetBalances":"unconfirmedAssetBalances");
            if ( is_cJSON_Array(array) != 0 )
            {
                n = cJSON_GetArraySize(array);
                for (i=0; i<n; i++)
                {
                    item = cJSON_GetArrayItem(array,i);
                    obj = cJSON_GetObjectItem(item,"asset");
                    copy_cJSON(assetid,obj);
                    //printf("i.%d of %d: %s(%s)\n",i,n,assetid,cJSON_Print(item));
                    if ( strcmp(assetid,assetstr) == 0 )
                    {
                        qty = get_cJSON_int(item,iter==0?"balanceQNT":"unconfirmedBalanceQNT");
                        break;
                    }
                }
            }
            if ( iter == 0 )
                quantity = qty;
            else *unconfirmedp = qty;
        }
    }
    return(quantity);
}

char *issue_getTransaction(char *txidstr)
{
    char cmd[4096],*jsonstr,*retstr = 0;
    sprintf(cmd,"%s=getTransaction&transaction=%s",NXTSERVER,txidstr);
    jsonstr = issue_curl(cmd);
    printf("getTransaction.%s %s\n",txidstr,jsonstr);
    if ( jsonstr != 0 )
    {
        //retstr = parse_NXTresults(0,"sender","",results_processor,jsonstr,strlen(jsonstr));
        free(jsonstr);
    } else printf("error getting txid.%s\n",txidstr);
    return(retstr);
}

char *issue_calculateFullHash(char *unsignedtxbytes,char *sighash)
{
    char cmd[4096],buf[512];
    union NXTtype ret;
    sprintf(cmd,"%s=calculateFullHash&unsignedTransactionBytes=%s&signatureHash=%s",NXTSERVER,unsignedtxbytes,sighash);
    ret = extract_NXTfield(buf,cmd,"fullHash",0);
    //printf("calculated.(%s)\n",ret.str);
    return(ret.str);
}

char *issue_parseTransaction(char *txbytes)
{
    char cmd[4096],*retstr = 0;
    sprintf(cmd,"%s=parseTransaction&transactionBytes=%s",NXTSERVER,txbytes);
    retstr = issue_curl(cmd);
    //printf("issue_parseTransaction.%s %s\n",txbytes,retstr);
    if ( retstr != 0 )
    {
        //retstr = parse_NXTresults(0,"sender","",results_processor,jsonstr,strlen(jsonstr));
        //free(jsonstr);
    } else printf("error getting txbytes.%s\n",txbytes);
    return(retstr);
}

int32_t get_NXTtxid_confirmations(char *txid)
{
    char cmd[4096];
    union NXTtype ret;
#ifdef DEBUG_MODE
    if ( strcmp(txid,"123456") == 0 )
        return(1000);
#endif
    sprintf(cmd,"%s=getTransaction&transaction=%s",NXTSERVER,txid);
    ret = extract_NXTfield(0,cmd,"confirmations",sizeof(int32_t));
    return(ret.val);
}

uint64_t issue_getBalance(char *NXTaddr)
{
    char cmd[4096];
    union NXTtype ret;
    sprintf(cmd,"%s=getBalance&account=%s",NXTSERVER,NXTaddr);
    ret = extract_NXTfield(0,cmd,"balanceNQT",64);
    return(ret.nxt64bits);
}

int32_t issue_decodeToken(char sender[MAX_NXTADDR_LEN],int32_t *validp,char *key,char encoded[NXT_TOKEN_LEN])
{
    char cmd[4096],token[NXT_TOKEN_LEN+1];
    cJSON *nxtobj,*validobj;
    union NXTtype retval;
    *validp = -1;
    sender[0] = 0;
    memcpy(token,encoded,NXT_TOKEN_LEN);
    token[NXT_TOKEN_LEN] = 0;
    sprintf(cmd,"%s=decodeToken&website=%s&token=%s",NXTSERVER,key,token);
    //printf("cmd.(%s)\n",cmd);
    retval = extract_NXTfield(0,cmd,0,0);
    if ( retval.json != 0 )
    {
        validobj = cJSON_GetObjectItem(retval.json,"valid");
        if ( validobj != 0 )
            *validp = ((validobj->type&0xff) == cJSON_True) ? 1 : 0;
        nxtobj = cJSON_GetObjectItem(retval.json,"account");
        copy_cJSON(sender,nxtobj);
        free_json(retval.json);
        if ( sender[0] != 0 )
            return((int32_t)strlen(sender));
        else return(0);
    }
    return(-1);
}

int32_t issue_generateToken(char encoded[NXT_TOKEN_LEN],char *key,char *secret)
{
    char cmd[4096],token[NXT_TOKEN_LEN+1];
    cJSON *tokenobj;
    union NXTtype retval;
    encoded[0] = 0;
    sprintf(cmd,"%s=generateToken&website=%s&secretPhrase=%s",NXTSERVER,key,secret);
    // printf("cmd.(%s)\n",cmd);
    retval = extract_NXTfield(0,cmd,0,0);
    if ( retval.json != 0 )
    {
        //printf("token.(%s)\n",cJSON_Print(retval.json));
        tokenobj = cJSON_GetObjectItem(retval.json,"token");
        memset(token,0,sizeof(token));
        copy_cJSON(token,tokenobj);
        free_json(retval.json);
        if ( token[0] != 0 )
        {
            memcpy(encoded,token,NXT_TOKEN_LEN);
            return(0);
        }
    }
    return(-1);
}

char *submit_AM(char *recipient,struct NXT_AMhdr *ap,char *reftxid)
{
    //union NXTtype retval;
    int32_t len,deadline = 10;
    cJSON *json,*txjson;
    char hexbytes[4096],cmd[5120],txid[MAX_NXTADDR_LEN],*jsonstr,*retstr = 0;
    len = ap->size;//(int)sizeof(*ap);
    if ( len > 1000 || len < 1 )
    {
        printf("issue_sendMessage illegal len %d\n",len);
        return(0);
    }
    // jl777: here is where the entire message could be signed;
   // printf("in submit_AM\n");
    memset(hexbytes,0,sizeof(hexbytes));
    init_hexbytes(hexbytes,(void *)ap,len);
    sprintf(cmd,"%s=sendMessage&secretPhrase=%s&recipient=%s&message=%s&deadline=%u%s&feeNQT=%ld",Global_mp->NXTAPISERVER,Global_mp->NXTACCTSECRET,recipient,hexbytes,deadline,reftxid!=0?reftxid:"",MIN_NQTFEE);
    //printf("submit_AM.(%s)\n",cmd);
    jsonstr = issue_curl(cmd);
    //printf("back from issue_curl\n");
    if ( jsonstr != 0 )
    {
        json = cJSON_Parse(jsonstr);
        if ( json == 0 ) printf("Error before: (%s) -> [%s]\n",jsonstr,cJSON_GetErrorPtr());
        else
        {
            txjson = cJSON_GetObjectItem(json,"transaction");
            copy_cJSON(txid,txjson);
            if ( txid[0] != 0 )
            {
                retstr = clonestr(txid);
                printf("AMtxid.%s\n",txid);
            }
            free_json(json);
        }
    }
    if ( (retstr == 0 || retstr[0] == 0) && jsonstr != 0 )
        printf("submitAM.(%s) -> (%s)\n",cmd,jsonstr);
    return(retstr);
}

int32_t set_json_AM(struct json_AM *ap,int32_t sig,int32_t funcid,char *nxtaddr,int32_t timestamp,char *jsonstr,int32_t compressjson)
{
    int32_t jsonflag;
    long len = 0;
    char *teststr;
    struct compressed_json *jsn;
    compressjson = 0; // some cases dont decompress on the other side, even though decode tests fine :(
    if ( jsonstr == 0 )
        jsonflag = 0;
    else jsonflag = 1 + (compressjson != 0);
    if ( jsonflag == 2 )
    {
        if ( (jsn= encode_json(jsonstr)) != 0 )
            len = sizeof(*ap) + jsn->complen;
        else
        {
            printf("set_json_AM: error encoding.(%s)\n",jsonstr);
            return(-1);
        }
    }
    if ( jsonflag != 0 )
    {
        if ( len == 0 )
        {
            len = sizeof(*ap) + strlen(jsonstr) + 1;
            jsonflag = 1;
        }
    } else len = sizeof(*ap);
    memset(ap,0,len);
    ap->H.sig = sig;
    if ( nxtaddr != 0 )
    {
        ap->H.nxt64bits = calc_nxt64bits(nxtaddr);
        //printf("set_json_AM: NXT.%s -> %s %lx\n",nxtaddr,nxt64str(ap->H.nxt64bits),(long)ap->H.nxt64bits);
    }
    ap->funcid = funcid;
    //ap->gatewayid = gp->gatewayid;
    ap->timestamp = timestamp;
    ap->jsonflag = jsonflag;
    if ( jsonflag == 1 )
        strcpy(ap->jsonstr,jsonstr);//,999 - ((long)ap->jsonstr - (long)ap));
    else if ( jsonflag > 1 )
    {
        memcpy(&ap->jsn,jsn,sizeof(*jsn)+jsn->complen);
        free(jsn);
        teststr = decode_json(&ap->jsn,jsonflag-2);
        if ( teststr != 0 )
        {
            stripstr(teststr,(int64_t)ap->jsn.origlen);
            if ( strcmp(teststr,jsonstr) != 0 )
                printf("JSONcodec error (%s) != (%s)\n",teststr,jsonstr);
            else printf("decoded.(%s) %d %d %d starting\n",teststr,ap->jsn.complen,ap->jsn.origlen,ap->jsn.sublen);
            free(teststr);
        }
    }
    //printf("AM len.%ld vs %ld (%s)\n",len,strlen(ap->jsonstr),ap->jsonstr);
    ap->H.size = (int32_t)len;
    return(0);
}

void set_next_NXTblock(char *nextblock,char *blockidstr)
{
    union NXTtype retval;
    char cmd[4096];
    int32_t errcode;
    cJSON *nextjson;
    nextblock[0] = 0;
    sprintf(cmd,"%s=getBlock&block=%s",NXTSERVER,blockidstr);
    // printf("cmd.(%s)\n",cmd);
    retval = extract_NXTfield(0,cmd,0,0);
    if ( retval.json != 0 )
    {
        //printf("%s\n",cJSON_Print(retval.json));
        errcode = (int32_t)get_cJSON_int(retval.json,"errorCode");
        if ( errcode == 0 )
        {
            nextjson = cJSON_GetObjectItem(retval.json,"nextBlock");
            if ( nextjson != 0 )
                copy_cJSON(nextblock,nextjson);
        }
        free_json(retval.json);
    }
}

int32_t set_current_NXTblock(char *blockidstr)
{
    int32_t numunlocked = 0;
    union NXTtype retval;
    cJSON *blockjson,*unlocked;//*nextjson,
    char cmd[256],unlockedstr[1024];
    sprintf(cmd,"%s=getState",NXTSERVER);
    retval = extract_NXTfield(0,cmd,0,0);
    if ( retval.json != 0 )
    {
        blockjson = cJSON_GetObjectItem(retval.json,"lastBlock");
        //nextjson = cJSON_GetObjectItem(retval.json,"nextBlock");
        unlocked = cJSON_GetObjectItem(retval.json,"numberOfUnlockedAccounts");
        copy_cJSON(blockidstr,blockjson);
        //copy_cJSON(nextblock,nextjson);
        copy_cJSON(unlockedstr,unlocked);
        numunlocked = atoi(unlockedstr);
        free_json(retval.json);
    }
    return(numunlocked);
}

int32_t gen_randomacct(uint32_t randchars,char *NXTaddr,char *NXTsecret,char *randfilename)
{
    uint32_t i,j,x,iter,bitwidth = 6;
    FILE *fp;
    char buf[512],fname[512];
    unsigned char bits[33];
    NXTaddr[0] = 0;
    randchars /= 8;
    if ( randchars > (int)sizeof(bits) )
        randchars = (int)sizeof(bits);
    if ( randchars < 3 )
        randchars = 3;
    for (iter=0; iter<=8; iter++)
    {
        sprintf(fname,"%s.%d",randfilename,iter);
        fp = fopen(fname,"rb");
        if ( fp == 0 )
        {
            sprintf(buf,"dd if=/dev/random count=%d bs=1 > %s",randchars*8,fname);
            printf("cmd.(%s)\n",buf);
            if ( system(buf) != 0 )
                printf("error issuing system(%s)\n",buf);
            sleep(3);
        }
        fp = fopen(fname,"rb");
        if ( fp != 0 )
        {
            if ( fread(bits,1,sizeof(bits),fp) == 0 )
                printf("gen_random_acct: error reading bits\n");
            for (i=0; i+bitwidth<(sizeof(bits)*8) && i/bitwidth<randchars; i+=bitwidth)
            {
                for (j=x=0; j<6; j++)
                {
                    if ( GETBIT(bits,i*bitwidth+j) != 0 )
                        x |= (1 << j);
                }
                //printf("i.%d j.%d x.%d %c\n",i,j,x,1+' '+x);
                NXTsecret[randchars*iter + i/bitwidth] = safechar64(x);
            }
            NXTsecret[randchars*iter + i/bitwidth] = 0;
            fclose(fp);
        }
    }
    expand_nxt64bits(NXTaddr,issue_getAccountId(NXTsecret));
    printf("NXT.%s NXTsecret.(%s)\n",NXTaddr,NXTsecret);
    return(0);
}

int32_t init_NXTAPI()
{
    cJSON *json,*obj;
    int32_t timestamp;
    char cmd[4096],dest[1024],*jsonstr;
    init_jsoncodec(0);
    sprintf(cmd,"%s=getTime",NXTSERVER);
    while ( 1 )
    {
        while ( (jsonstr= issue_curl(cmd)) == 0 )
        {
            printf("error communicating to NXT network\n");
            sleep(3);
        }
        timestamp = 0;
        json = cJSON_Parse(jsonstr);
        if ( json == 0 ) printf("Error before: (%s) -> [%s]\n",jsonstr,cJSON_GetErrorPtr());
        else
        {
            obj = cJSON_GetObjectItem(json,"time");
            if ( obj != 0 )
            {
                copy_cJSON(dest,obj);
                timestamp = atoi(dest);
            }
        }
        if ( json != 0 )
            free_json(json);
        if ( timestamp > 0 )
        {
            printf("init_NXTAPI timestamp.%d\n",timestamp);
            free(jsonstr);
            return(timestamp);
        }
        printf("no time found (%s)\n",jsonstr);
        free(jsonstr);
        sleep(3);
    }
}

struct NXT_acct *get_NXTacct(int32_t *createdp,struct NXThandler_info *mp,char *NXTaddr)
{
    struct NXT_acct *np;
    //printf("NXTaccts hash %p\n",mp->NXTaccts_tablep);
    //printf("get_NXTacct.(%s)\n",NXTaddr);
    np = MTadd_hashtable(createdp,mp->NXTaccts_tablep,NXTaddr);
    if ( *createdp != 0 )
        np->Usock = -1;
    return(np);
}

int32_t validate_token(cJSON **argjsonp,char *pubkey,char *tokenizedtxt,char *NXTaddr,char *name,int32_t strictflag)
{
    cJSON *acctjson,*array,*firstitem=0,*tokenobj,*obj;
    int64_t amount,timeval,diff = 0;
    int32_t valid,retcode = -13;
    char buf[1024],NXTname[1024],username[1024],groupname[1024],sender[MAX_NXTADDR_LEN],encoded[1024],*firstjsontxt = 0;
    acctjson = issue_getAccountInfo(&amount,NXTname,username,NXTaddr,groupname);
    if ( argjsonp != 0 )
        *argjsonp = 0;
    if ( pubkey != 0 )
        pubkey[0] = 0;
    if ( acctjson != 0 )
        free_json(acctjson);
    if ( strcmp(name,NXTname) != 0 )
        return(-1);
    array = cJSON_Parse(tokenizedtxt);
    if ( array == 0 )
        return(-2);
    if ( is_cJSON_Array(array) != 0 && cJSON_GetArraySize(array) == 2 )
    {
        firstitem = cJSON_GetArrayItem(array,0);
        if ( pubkey != 0 )
        {
            obj = cJSON_GetObjectItem(firstitem,"pubkey");
            copy_cJSON(pubkey,obj);
        }
        if ( argjsonp != 0 )
        {
            *argjsonp = cJSON_GetObjectItem(firstitem,"xfer");
            if ( 0 && *argjsonp != 0 )
                printf("%p ARGJSON.(%s)\n",*argjsonp,cJSON_Print(*argjsonp));
        }
        obj = cJSON_GetObjectItem(firstitem,"NXT"); copy_cJSON(buf,obj);
        if ( strcmp(buf,NXTaddr) != 0 )
            retcode = -3;
        else
        {
            obj = cJSON_GetObjectItem(firstitem,"name"); copy_cJSON(buf,obj);
            if ( strcmp(buf,name) != 0 )
                retcode = -4;
            else
            {
                if ( strictflag != 0 )
                {
                    timeval = get_cJSON_int(firstitem,"time");
                    diff = timeval - time(NULL);
                    if ( diff < 0 )
                        diff = -diff;
                    if ( diff > strictflag )
                    {
                        printf("time diff %ld too big %ld vs %ld\n",(long)diff,(long)timeval,(long)time(NULL));
                        retcode = -5;
                    }
                }
                if ( retcode != -5 )
                {
                    firstjsontxt = cJSON_Print(firstitem); stripwhite(firstjsontxt,strlen(firstjsontxt));
                    tokenobj = cJSON_GetArrayItem(array,1);
                    obj = cJSON_GetObjectItem(tokenobj,"token");
                    copy_cJSON(encoded,obj);
                    memset(sender,0,sizeof(sender));
                    valid = -1;
                    if ( issue_decodeToken(sender,&valid,firstjsontxt,encoded) > 0 )
                    {
                        if ( strcmp(sender,NXTaddr) == 0 )
                        {
                            printf("signed by valid NXT.%s valid.%d diff.%ld\n",sender,valid,(long)diff);
                            retcode = valid;
                        }
                    }
                    if ( retcode < 0 )
                        printf("err: signed by valid NXT.%s valid.%d diff.%ld\n",sender,valid,(long)diff);
                    free(firstjsontxt);
                }
            }
        }
    }
    if ( retcode >= 0 && firstitem != 0 && argjsonp != 0 && *argjsonp != 0 )
        *argjsonp = cJSON_DetachItemFromObject(firstitem,"xfer");
    free_json(array);
    return(retcode);
}

char *tokenize_json(cJSON *argjson)
{
    char *str,token[NXT_TOKEN_LEN+1];
    cJSON *array;
    //printf("SECRET.(%s)\n",Global_mp->NXTACCTSECRET);
    array = cJSON_CreateArray();
    cJSON_AddItemToArray(array,argjson);

    str = cJSON_Print(argjson);
    stripwhite(str,strlen(str));
    issue_generateToken(token,str,Global_mp->NXTACCTSECRET);
    token[NXT_TOKEN_LEN] = 0;
    free(str);
    argjson = cJSON_CreateObject();
    cJSON_AddItemToObject(argjson,"token",cJSON_CreateString(token));
    cJSON_AddItemToArray(array,argjson);
    
    str = cJSON_Print(array);
    stripwhite(str,strlen(str));
    free_json(array);
    return(str);
}

int gen_tokenjson(char *jsonstr,char *user,char *NXTaddr,long nonce,cJSON *argjson)
{
    char *str,pubkey[1024];
    cJSON *json;
    json = cJSON_CreateObject();
    cJSON_AddItemToObject(json,"name",cJSON_CreateString(user));
    cJSON_AddItemToObject(json,"NXT",cJSON_CreateString(NXTaddr));
    init_hexbytes(pubkey,Global_mp->session_pubkey,sizeof(Global_mp->session_pubkey));
    cJSON_AddItemToObject(json,"pubkey",cJSON_CreateString(pubkey));
    cJSON_AddItemToObject(json,"time",cJSON_CreateNumber(nonce));
    if ( argjson != 0 )
        cJSON_AddItemToObject(json,"xfer",argjson);
    str = tokenize_json(json);
    strcpy(jsonstr,str);
    free(str);
    stripwhite(jsonstr,strlen(jsonstr));
    return((int)strlen(jsonstr));
}

uint32_t calc_ipbits(char *ipaddr)
{
    int32_t a,b,c,d;
    sscanf(ipaddr,"%d.%d.%d.%d",&a,&b,&c,&d);
    if ( a < 0 || a > 0xff || b < 0 || b > 0xff || c < 0 || c > 0xff || d < 0 || d > 0xff )
    {
        printf("malformed ipaddr?.(%s) -> %d %d %d %d\n",ipaddr,a,b,c,d);
    }
    return((d<<24) | (c<<16) | (b<<8) | a);
}

void expand_ipbits(char *ipaddr,uint32_t ipbits)
{
    sprintf(ipaddr,"%d.%d.%d.%d",(ipbits>>24)&0xff,(ipbits>>16)&0xff,(ipbits>>8)&0xff,(ipbits&0xff));
}

char *ipbits_str(uint32_t ipbits)
{
    static char ipaddr[32];
    expand_ipbits(ipaddr,ipbits);
    return(ipaddr);
}

char *ipbits_str2(uint32_t ipbits)
{
    static char ipaddr[32];
    expand_ipbits(ipaddr,ipbits);
    return(ipaddr);
}

struct sockaddr_in conv_ipbits(uint32_t ipbits,int32_t port)
{
    char ipaddr[32];
    struct hostent *host;
    struct sockaddr_in server_addr;
    expand_ipbits(ipaddr,ipbits);
    host = (struct hostent *)gethostbyname(ipaddr);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr = *((struct in_addr *)host->h_addr);
    memset(&(server_addr.sin_zero),0,8);
    return(server_addr);
}

//#ifndef WIN32
double milliseconds(void)
{
    static struct timeval timeval,first_timeval;
    gettimeofday(&timeval,0);
    if ( first_timeval.tv_sec == 0 )
    {
        first_timeval = timeval;
        return(0);
    }
    return((timeval.tv_sec - first_timeval.tv_sec) * 1000. + (timeval.tv_usec - first_timeval.tv_usec)/1000.);
}
//#endif

int64_t microseconds(void)
{
    static struct timeval timeval;
    gettimeofday(&timeval,0);
    return(timeval.tv_sec*1000000 + timeval.tv_usec);
}

#define SMALLVAL .000000000000001

double _dxblend(double *destp,double val,double decay)
{
	double oldval;
	if ( (oldval = *destp) != 0. )
		return((oldval * decay) + ((1. - decay) * val));
	else
		return(val);
}

double dxblend(double *destp,double val,double decay)
{
	double newval,slope;
	if ( isnan(*destp) != 0 )
		*destp = 0.;
	if ( isnan(val) != 0 )
		return(0.);
	if ( *destp == 0 )
	{
		*destp = val;
		return(0);
	}
	newval = _dxblend(destp,val,decay);
	if ( newval < SMALLVAL && newval > -SMALLVAL )
	{
		// non-zero marker for actual values close to or even equal to zero
		if ( newval < 0. )
			newval = -SMALLVAL;
		else
			newval = SMALLVAL;
	}
	if ( *destp != 0. && newval != 0. )
	{
		//slope = ((newval / *destp) - 1.);
		slope = (newval - *destp);
	}
	else
		slope = 0.;
	*destp = newval;
	return(slope);
}

int _increasing_unsignedint(const void *a,const void *b)
{
#define uint_a (((unsigned int *)a)[0])
#define uint_b (((unsigned int *)b)[0])
	if ( uint_b > uint_a )
		return(-1);
	else if ( uint_b < uint_a )
		return(1);
	return(0);
#undef uint_a
#undef uint_b
}

static int _increasing_float(const void *a,const void *b)
{
#define float_a (*(float *)a)
#define float_b (*(float *)b)
	if ( float_b > float_a )
		return(-1);
	else if ( float_b < float_a )
		return(1);
	return(0);
#undef float_a
#undef float_b
}

int _decreasing_unsignedint64(const void *a,const void *b)
{
#define uint_a (((uint64_t *)a)[0])
#define uint_b (((uint64_t *)b)[0])
	if ( uint_b > uint_a )
		return(1);
	else if ( uint_b < uint_a )
		return(-1);
	return(0);
#undef uint_a
#undef uint_b
}

int _decreasing_signedint64(const void *a,const void *b)
{
#define int_a (((int64_t *)a)[0])
#define int_b (((int64_t *)b)[0])
	if ( int_b > int_a )
		return(1);
	else if ( int_b < int_a )
		return(-1);
	return(0);
#undef int_a
#undef int_b
}


static int _decreasing_double(const void *a,const void *b)
{
#define double_a (*(double *)a)
#define double_b (*(double *)b)
	if ( double_b > double_a )
		return(1);
	else if ( double_b < double_a )
		return(-1);
	return(0);
#undef double_a
#undef double_b
}

static int _increasing_double(const void *a,const void *b)
{
#define double_a (*(double *)a)
#define double_b (*(double *)b)
	if ( double_b > double_a )
		return(-1);
	else if ( double_b < double_a )
		return(1);
	return(0);
#undef double_a
#undef double_b
}

int32_t revsortds(double *buf,uint32_t num,int32_t size)
{
	qsort(buf,num,size,_decreasing_double);
	return(0);
}

int32_t sortds(double *buf,uint32_t num,int32_t size)
{
	qsort(buf,num,size,_increasing_double);
	return(0);
}

int32_t bitweight(uint64_t x)
{
    int32_t wt,i;
	wt = 0;
	for (i=0; i<64; i++)
	{
		if ( (x & 1) != 0 )
			wt++;
		x >>= 1;
		if ( x == 0 )
			break;
	}
	return(wt);
}

void add_nxt64bits_json(cJSON *json,char *field,uint64_t nxt64bits)
{
    cJSON *obj;
    char numstr[64];
    if ( nxt64bits != 0 )
    {
        expand_nxt64bits(numstr,nxt64bits);
        obj = cJSON_CreateString(numstr);
        cJSON_AddItemToObject(json,field,obj);
    }
}

void launch_app_in_new_terminal(char *appname,int argc,char **argv)
{
#ifdef __APPLE__
    FILE *fp;
    int i;
    char cmd[2048];
    if ( (fp= fopen("/tmp/launchit","w")) != 0 )
    {
        fprintf(fp,"osascript <<END\n");
        fprintf(fp,"tell application \"Terminal\"\n");
        fprintf(fp,"do script \"cd \\\"`pwd`\\\";$1;exit\"\n");
        fprintf(fp,"end tell\n");
        fprintf(fp,"END\n");
        fclose(fp);
        system("chmod +x /tmp/launchit");
        sprintf(cmd,"/tmp/launchit \"%s ",appname);
        for (i=0; argv[i]!=0; i++)
            sprintf(cmd+strlen(cmd),"%s ",argv[i]);
        strcat(cmd,"\"");
        printf("cmd.(%s)\n",cmd);
        system(cmd);
    }
#else
    void *punch_client_glue(void *argv);
    if ( pthread_create(malloc(sizeof(pthread_t)),NULL,punch_client_glue,argv) != 0 )
        printf("ERROR punch_client_glue\n");
#endif
}

int search_uint32_ts(int32_t *ints,int32_t val)
{
    int i;
    for (i=0; ints[i]>=0; i++)
        if ( val == ints[i] )
            return(0);
    return(-1);
}

#ifndef WIN32
void *map_file(char *fname,uint64_t *filesizep,int32_t enablewrite)
{
	void *mmap64(void *addr,size_t len,int prot,int flags,int fildes,off_t off);
	int fd,rwflags,flags = MAP_FILE|MAP_SHARED;
	uint64_t filesize;
    void *ptr = 0;
	*filesizep = 0;
	if ( enablewrite != 0 )
		fd = open(fname,O_RDWR);
	else
  		fd = open(fname,O_RDONLY);
	if ( fd < 0 )
	{
		//printf("map_file: error opening enablewrite.%d %s\n",enablewrite,fname);
        return(0);
	}
    if ( *filesizep == 0 )
        filesize = (uint64_t)lseek(fd,0,SEEK_END);
    else
        filesize = *filesizep;
	rwflags = PROT_READ;
	if ( enablewrite != 0 )
		rwflags |= PROT_WRITE;
#ifdef __APPLE__
	ptr = mmap(0,filesize,rwflags,flags,fd,0);
#else
	ptr = mmap64(0,filesize,rwflags,flags,fd,0);
#endif
	close(fd);
    if ( ptr == 0 || ptr == MAP_FAILED )
	{
		printf("map_file.write%d: mapping %s failed? mp %p\n",enablewrite,fname,ptr);
		return(0);
	}
	*filesizep = filesize;
	return(ptr);
}

int32_t release_map_file(void *ptr,uint64_t filesize)
{
	int32_t retval;
    if ( ptr == 0 )
	{
		printf("release_map_file: null ptr\n");
		return(-1);
	}
	retval = munmap(ptr,filesize);
	if ( retval != 0 )
		printf("release_map_file: munmap error %p %ld: err %d\n",ptr,(long)filesize,retval);
	//else
	//	printf("released %p %ld\n",ptr,filesize);
	return(retval);
}
#else
void usleep(int utimeout)
{
    utimeout /= 1000;
    if ( utimeout == 0 )
        utimeout = 1;
    Sleep(utimeout);
}

void sleep(int seconds)
{
    Sleep(seconds * 1000);
}

void *map_file(char *fname,uint64_t *filesizep,int32_t enablewrite)
{
    FILE *fp;
    void *ptr = 0;
    *filesizep = 0;
    if ( (fp= fopen(fname,"rb")) != 0 )
    {
        fseek(fp,0,SEEK_END);
        *filesizep = ftell(fp);
        rewind(fp);
        fread(ptr,1,(long)*filesizep,fp);
        fclose(fp);
    }
    return(ptr);
}

int32_t release_map_file(void *ptr,uint64_t filesize)
{
    free(ptr);
    return(0);
}
#endif

uint32_t calc_file_crc(uint64_t *filesizep,char *fname)
{
    void *ptr;
    uint32_t totalcrc = 0;
#ifdef WIN32
    FILE *fp;
    *filesizep = 0;
    if ( (fp= fopen(fname,"rb")) != 0 )
    {
        fseek(fp,0,SEEK_END);
        *filesizep = ftell(fp);
        rewind(fp);
        ptr = malloc((long)*filesizep);
        fread(ptr,1,(long)*filesizep,fp);
        fclose(fp);
        totalcrc = _crc32(0,ptr,(long)*filesizep);
    }
#else
    uint32_t enablewrite = 0;
    ptr = map_file(fname,filesizep,enablewrite);
    if ( ptr != 0 )
    {
        totalcrc = _crc32(0,ptr,(long)*filesizep);
        release_map_file(ptr,(long)*filesizep);
    }
#endif
    return(totalcrc);
}

cJSON *parse_json_AM(struct json_AM *ap)
{
    char *jsontxt;
    if ( ap->jsonflag != 0 )
    {
        jsontxt = (ap->jsonflag == 1) ? ap->jsonstr : decode_json(&ap->jsn,ap->jsonflag);
        if ( jsontxt != 0 )
        {
            if ( jsontxt[0] == '"' && jsontxt[strlen(jsontxt)-1] == '"' )
                replace_backslashquotes(jsontxt);
            return(cJSON_Parse(jsontxt));
        }
    }
    return(0);
}

int32_t is_valid_NXTtxid(char *txid)
{
    long i,len;
    if ( txid == 0 )
        return(0);
    len = strlen(txid);
    if ( len < 6 )
        return(0);
    for (i=0; i<len; i++)
        if ( txid[i] < '0' || txid[i] > '9' )
            break;
    if ( i != len )
        return(0);
    return(1);
}

double calc_dpreds(double dpreds[6])
{
    double sum,net,both;
	sum = (dpreds[0] + dpreds[2]);
	net = (dpreds[1] - dpreds[3]);
	both = MAX(1,(dpreds[1] + dpreds[3]));
	//return(sum/both);
	if ( net*sum > 0 )
		return((net * fabs(sum))/(both * both));
	else return(0.);
}

double calc_dpreds_ave(double dpreds[6])
{
    double sum,both;
	sum = (dpreds[0] + dpreds[2]);
	both = MAX(1,(dpreds[1] + dpreds[3]));
	return(sum/both);
}

double calc_dpreds_abs(double dpreds[6])
{
    double both;
	both = (dpreds[1] + dpreds[3]);
	if ( both >= 1. )
		return((dpreds[0] - dpreds[2]) / both);
	else return(0);
}

double calc_dpreds_metric(double dpreds[6])
{
    double both;
	both = (dpreds[1] + dpreds[3]);
	if ( both >= 1. )
		return((dpreds[1] - dpreds[3]) / both);
	else return(0);
}

void update_dpreds(double dpreds[6],register double pred)
{
	if ( pred > SMALLVAL ) dpreds[0] += pred, dpreds[1] += 1.;
	else if ( pred < -SMALLVAL ) dpreds[2] += pred, dpreds[3] += 1.;
    if ( pred != 0. )
    {
        if ( dpreds[4] == 0. || pred < dpreds[4] )
            dpreds[4] = pred;
        if ( dpreds[5] == 0. || pred > dpreds[5] )
            dpreds[5] = pred;
    }
    //printf("[%f %f %f %f] ",dpreds.x,dpreds.y,dpreds.z,dpreds.w);
}

#endif
