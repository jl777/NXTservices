

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
            *bufp = buf = realloc(buf,*allocsizep);
            //buflen = filesize+1;
        }
        rewind(fp);
        if ( buf == 0 )
            printf("Null buf ???\n");
        else
        {
            fread(buf,1,filesize,fp);
            buf[filesize] = 0;
        }
        fclose(fp);
        
        *lenp = filesize;
    }
    return(buf);
}

char *_issue_cmd_to_buffer(char *prog,char *arg1,char *arg2,char *arg3)
{
    char buffer[4096],cmd[512];
    int32_t fd[2];
    unsigned long len;
	static pthread_mutex_t mutex;
 	pthread_mutex_lock(&mutex);
    pipe(fd);
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
        system(cmd);
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

char *get_ipaddr()
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
                        case -(int)sizeof(long):
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
                retstr = clonestr(txid);
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
        printf("NXT.%s -> %s %lx\n",nxtaddr,nxt64str(ap->H.nxt64bits),(long)ap->H.nxt64bits);
    }
    ap->funcid = funcid;
    //ap->gatewayid = gp->gatewayid;
    ap->timestamp = timestamp;
    ap->jsonflag = jsonflag;
    if ( jsonflag == 1 )
        strcpy(ap->jsonstr,jsonstr);//,999 - ((long)ap->jsonstr - (long)ap));
    else if ( jsonflag == 2 )
    {
        memcpy(&ap->jsn,jsn,sizeof(*jsn)+jsn->complen);
        free(jsn);
        teststr = decode_json(&ap->jsn);
        if ( teststr != 0 )
        {
            stripstr(teststr,(int64_t)ap->jsn.origlen);
            if ( strcmp(teststr,jsonstr) != 0 )
                printf("JSONcodec error (%s) != (%s)\n",teststr,jsonstr);
            else printf("decoded.(%s) %d %d %d starting with %x\n",teststr,ap->jsn.complen,ap->jsn.origlen,ap->jsn.sublen,*(int32_t *)ap->jsn.encoded);
            free(teststr);
        }
    }
    //printf("AM len.%ld vs %ld\n",len,strlen(ap->jsonstr));
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
            system(buf);
            sleep(3);
        }
        fp = fopen(fname,"rb");
        if ( fp != 0 )
        {
            fread(bits,1,sizeof(bits),fp);
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
    //printf("NXTaccts hash %p\n",mp->NXTaccts_tablep);
    return(MTadd_hashtable(createdp,mp->NXTaccts_tablep,NXTaddr));
}

uint32_t calc_ipbits(char *ipaddr)
{
    int32_t a,b,c,d;
    sscanf(ipaddr,"%d.%d.%d.%d",&a,&b,&c,&d);
    if ( a < 0 || a > 0xff || b < 0 || b > 0xff || c < 0 || c > 0xff || d < 0 || d > 0xff )
    {
        printf("malformed ipaddr?.(%s) -> %d %d %d %d\n",ipaddr,a,b,c,d);
    }
    return((a<<24) | (b<<16) | (c<<8) | d);
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
    bzero(&(server_addr.sin_zero),8);
    return(server_addr);
}

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
#endif


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
