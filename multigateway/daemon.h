//
//  daemon.h
//  Created by jl777 April 2014
//  MIT License
//


#ifndef gateway_daemon_h
#define gateway_daemon_h


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
    if ( Global_gp->gatewayid < 0 )
    {
        switch ( coinid )
        {
            case BTC_COINID: strcpy(userpass,"xxx:yyy"); return("node2.mynxtcoin.org:8332"); break;
            case LTC_COINID: strcpy(userpass,"xxx:yyy"); return("node2.mynxtcoin.org:9332"); break;
            case CGB_COINID: strcpy(userpass,"xxx:yyy"); return("node2.mynxtcoin.org:8395"); break;
            case DOGE_COINID: strcpy(userpass,"yyy:xxx"); return("node2.mynxtcoin.org:22555"); break;
            case DRK_COINID: strcpy(userpass,"xxx:yyy"); return("node2.mynxtcoin.org:9998"); break;
        }
        return(0);
    }
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
    } else printf("extract_userpass cant open.(%s)\n",fname);
    return(serverport);
}

struct daemon_info *create_daemon_info(int32_t coinid,char *name,int32_t minconfirms,double txfee,int32_t pollseconds,char *asset,char *userpass_fname,char *serverport,int32_t blockheight,char *marker)
{
    struct gateway_info *gp = Global_gp;
    struct replicated_coininfo *lp = calloc(1,sizeof(*lp));
    struct daemon_info *cp = calloc(1,sizeof(*cp));
    FILE *fp;
    char userpass[512],walletbackup[512];
    lp->coinid = coinid;
    safecopy(lp->name,name,sizeof(lp->name)); gp->name[coinid] = lp->name;
    if ( (fp= fopen("backups/tmp","rb")) == 0 )
    {
        system("mkdir backups");
        fp = fopen("backups/tmp","wb");
        fclose(fp);
    }
    else fclose(fp);
    lp->pollseconds = pollseconds;
    safecopy(lp->assetid,asset,sizeof(lp->assetid)); gp->assetid[coinid] = lp->assetid;
    gp->internalmarker[coinid] = clonestr(marker);
    serverport = extract_userpass(cp,coinid,serverport,userpass,userpass_fname);
    gp->serverport[coinid] = clonestr(serverport);
    gp->userpass[coinid] = clonestr(userpass);
    gp->blockheight[coinid] = blockheight;
    gp->min_confirms[coinid] = minconfirms;
    cp->numretries = NUM_BITCOIND_RETRIES;
    lp->markeramount = (.00001 * SATOSHIDEN);
    lp->txfee = (uint64_t)(txfee  * SATOSHIDEN) + lp->markeramount;
    printf("%s userpass.(%s) -> (%s)\n",coinid_str(coinid),gp->userpass[coinid],gp->serverport[coinid]);
    if ( gp->gatewayid >= 0 )
    {
        sprintf(walletbackup,"backups/%s",name);
        gp->walletbackup[coinid] = clonestr(walletbackup);
    }
    gp->replicants[coinid] = lp;
    return(cp);
}

struct daemon_info *init_daemon_info(struct gateway_info *gp,char *name,char *serverA,char *serverB,char *serverC)
{
    struct daemon_info *cp = 0;
    // get this from JSON input file!
    if ( strcmp(name,"BTC") == 0 )
        cp = create_daemon_info(BTC_COINID,"BTC",BTC_MIN_CONFIRMS,BTC_TXFEE,60,BTC_COINASSET,BTC_CONF,BTC_PORT,BTC_FIRST_BLOCKHEIGHT,BTC_MARKER);
    else if ( strcmp(name,"LTC") == 0 )
        cp = create_daemon_info(LTC_COINID,"LTC",LTC_MIN_CONFIRMS,LTC_TXFEE,60,LTC_COINASSET,LTC_CONF,LTC_PORT,LTC_FIRST_BLOCKHEIGHT,LTC_MARKER);
    else if ( strcmp(name,"CGB") == 0 )
        cp = create_daemon_info(CGB_COINID,"CGB",CGB_MIN_CONFIRMS,CGB_TXFEE,60,CGB_COINASSET,CGB_CONF,CGB_PORT,CGB_FIRST_BLOCKHEIGHT,CGB_MARKER);
    else if ( strcmp(name,"DOGE") == 0 )
    {
        cp = create_daemon_info(DOGE_COINID,"DOGE",DOGE_MIN_CONFIRMS,DOGE_TXFEE,60,DOGE_COINASSET,DOGE_CONF,DOGE_PORT,DOGE_FIRST_BLOCKHEIGHT,DOGE_MARKER);
        //cp->initlist = malloc(sizeof(*cp->initlist));
        //cp->initlist[cp->numinitlist++] = 157499;
    }
    else if ( strcmp(name,"DRK") == 0 )
        cp = create_daemon_info(DRK_COINID,"DRK",DRK_MIN_CONFIRMS,DRK_TXFEE,60,DRK_COINASSET,DRK_CONF,DRK_PORT,DRK_FIRST_BLOCKHEIGHT,DRK_MARKER);
    else if ( strcmp(name,"ANC") == 0 )
        cp = create_daemon_info(ANC_COINID,"ANC",ANC_MIN_CONFIRMS,ANC_TXFEE,60,ANC_COINASSET,ANC_CONF,ANC_PORT,ANC_FIRST_BLOCKHEIGHT,ANC_MARKER);
    else printf("unsupported coin.%s\n",name);
    return(cp);
}


#endif
