//
//  daemon.h
//  Created by jl777 April 2014
//  MIT License
//


#ifndef gateway_daemon_h
#define gateway_daemon_h

void set_bitcoind_confname(char *fname,char *nameroot,char *PCusername)
{
    char name[128];
    strcpy(name,nameroot);
    name[0] = toupper(name[0]);
#ifdef __APPLE__
    sprintf(fname,"/Users/%s/Library/Application Support/%s/%s.conf",PCusername,name,nameroot);
#elif __linux__
    sprintf(fname,"/home/admin1/.%s/%s.conf",nameroot,nameroot);
#else
    //(XP) C:\Documents and Settings\username\Application Data\Bitcoin\bitcoin.conf
    //sprintf(fname,"C:\Users\username\AppData\Roaming\%s\%s.conf",name,nameroot);
    sprintf(fname,"C:\Users\username\AppData\Roaming\%s\%s.conf",PCusername,name,nameroot);
#endif
}

char *parse_conf_line(char *line,char *field)
{
    line += strlen(field);
    for (; *line!='='&&*line!=0; line++)
        break;
    if ( *line == 0 )
        return(0);
    if ( *line == '=' )
        line++;
    stripstr(line,strlen(line));
    printf("[%s]\n",line);
    return(clonestr(line));
}

char *extract_userpass(struct daemon_info *cp,int32_t coinid,char *serverport,char *userpass,char *fname)
{
    FILE *fp;
    char line[1024],*rpcuser,*rpcpassword,*str;
    userpass[0] = 0;
    if ( (fp= fopen(fname,"r")) != 0 )
    {
        printf("extract_userpass from (%s)\n",fname);
        rpcuser = rpcpassword = 0;
        while ( fgets(line,sizeof(line),fp) > 0 )
        {
            //printf("line.(%s) %p %p\n",line,strstr(line,"rpcuser"),strstr(line,"rpcpassword"));
            if ( (str= strstr(line,"rpcuser")) != 0 )
                rpcuser = parse_conf_line(str,"rpcuser");
            else if ( (str= strstr(line,"rpcpassword")) != 0 )
                rpcpassword = parse_conf_line(str,"rpcpassword");
        }
        if ( rpcuser != 0 && rpcpassword != 0 )
            sprintf(userpass,"%s:%s",rpcuser,rpcpassword);
        else userpass[0] = 0;
        printf("-> (%s):(%s) userpass.(%s)\n",rpcuser,rpcpassword,userpass);
        if ( rpcuser != 0 )
            free(rpcuser);
        if ( rpcpassword != 0 )
            free(rpcpassword);
    }
    else
    {
        printf("extract_userpass cant open.(%s)\n",fname);
        return(0);
    }
    return(serverport);
}

struct daemon_info *create_daemon_info(int32_t estblocktime,int32_t coinid,char *name,int32_t minconfirms,double txfee,int32_t pollseconds,char *asset,char *coin_nameroot,char *serverport,int32_t blockheight,char *marker)
{
    struct gateway_info *gp = Global_gp;
    struct replicated_coininfo *lp = calloc(1,sizeof(*lp));
    struct daemon_info *cp = calloc(1,sizeof(*cp));
    FILE *fp;
    char userpass[512],walletbackup[512],conf_fname[512];
    lp->coinid = coinid;
    lp->estblocktime = estblocktime;
    set_bitcoind_confname(conf_fname,coin_nameroot,PC_USERNAME);
    safecopy(lp->name,name,sizeof(lp->name)); gp->name[coinid] = lp->name;
    if ( (fp= fopen("backups/tmp","rb")) == 0 )
    {
        if ( system("mkdir backups") != 0 )
            printf("error making backup dir\n");
        fp = fopen("backups/tmp","wb");
        fclose(fp);
    }
    else fclose(fp);
    lp->pollseconds = pollseconds;
    safecopy(lp->assetid,asset,sizeof(lp->assetid)); gp->assetid[coinid] = lp->assetid;
    gp->internalmarker[coinid] = clonestr(marker);
    gp->blockheight[coinid] = blockheight;
    gp->min_confirms[coinid] = minconfirms;
    lp->markeramount = (.00001 * SATOSHIDEN);
    lp->txfee = (uint64_t)(txfee  * SATOSHIDEN) + lp->markeramount;
    if ( gp->gatewayid >= 0 )
    {
        sprintf(walletbackup,"backups/%s",name);
        gp->walletbackup[coinid] = clonestr(walletbackup);
    }
    gp->replicants[coinid] = lp;
    serverport = extract_userpass(cp,coinid,serverport,userpass,conf_fname);
    if ( serverport != 0 )
    {
        gp->serverport[coinid] = clonestr(serverport);
        gp->userpass[coinid] = clonestr(userpass);
        printf("%s userpass.(%s) -> (%s)\n",coinid_str(coinid),gp->userpass[coinid],gp->serverport[coinid]);
        cp->numretries = NUM_BITCOIND_RETRIES;
        cp->minconfirms = minconfirms;
        cp->estblocktime = estblocktime;
        cp->txfee = (uint64_t)(txfee  * SATOSHIDEN);
    }
    else
    {
        free(cp);
        cp = 0;
    }
    return(cp);
}

struct daemon_info *init_daemon_info(struct gateway_info *gp,char *name,char *serverA,char *serverB,char *serverC)
{
    struct daemon_info *cp = 0;
    // get this from JSON input file!
    if ( strcmp(name,"BTC") == 0 )
        cp = create_daemon_info(600,BTC_COINID,"BTC",BTC_MIN_CONFIRMS,BTC_TXFEE,60,BTC_COINASSET,BTC_CONF,BTC_PORT,BTC_FIRST_BLOCKHEIGHT,BTC_MARKER);
    else if ( strcmp(name,"LTC") == 0 )
        cp = create_daemon_info(150,LTC_COINID,"LTC",LTC_MIN_CONFIRMS,LTC_TXFEE,60,LTC_COINASSET,LTC_CONF,LTC_PORT,LTC_FIRST_BLOCKHEIGHT,LTC_MARKER);
    else if ( strcmp(name,"CGB") == 0 )
        cp = create_daemon_info(600,CGB_COINID,"CGB",CGB_MIN_CONFIRMS,CGB_TXFEE,60,CGB_COINASSET,CGB_CONF,CGB_PORT,CGB_FIRST_BLOCKHEIGHT,CGB_MARKER);
    else if ( strcmp(name,"DOGE") == 0 )
    {
        cp = create_daemon_info(150,DOGE_COINID,"DOGE",DOGE_MIN_CONFIRMS,DOGE_TXFEE,60,DOGE_COINASSET,DOGE_CONF,DOGE_PORT,DOGE_FIRST_BLOCKHEIGHT,DOGE_MARKER);
        //cp->initlist = malloc(sizeof(*cp->initlist));
        //cp->initlist[cp->numinitlist++] = 157499;
    }
    else if ( strcmp(name,"DRK") == 0 )
        cp = create_daemon_info(150,DRK_COINID,"DRK",DRK_MIN_CONFIRMS,DRK_TXFEE,60,DRK_COINASSET,DRK_CONF,DRK_PORT,DRK_FIRST_BLOCKHEIGHT,DRK_MARKER);
    else if ( strcmp(name,"ANC") == 0 )
        cp = create_daemon_info(600,ANC_COINID,"ANC",ANC_MIN_CONFIRMS,ANC_TXFEE,60,ANC_COINASSET,ANC_CONF,ANC_PORT,ANC_FIRST_BLOCKHEIGHT,ANC_MARKER);
    else printf("unsupported coin.%s\n",name);
    return(cp);
}


#endif
