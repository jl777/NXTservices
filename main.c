//
//  main.c
//  Created by jl777, April 5th, 2014
//  MIT License
//
// 1st implementation of NXTprotocol
// reference protocol handlers for:
//
// multicoin multiserver fragmented multisig gateway, beta release
// nodecoin pool servers and nodeminer
// NXTcoins "create a coin in a minute" dev kit
// NXT network health monitor
// NXT mixer and NXT bridges for supported coins: BTC, LTC, PPC, DOGE, DRK, ANC, USD, CNY
// NXT cash support for step Z
// BTC and USD markets for all crypto
// programmable NXT tradebot using C!
// Atomic exchange of all coin assets with each other and with NXT
// fiat gateways using ripple
// BTC exchange, USD exchange, CNY exchange
// dividends for assets

// Todo:
// add trade changes to asset inventory
// publish data stream in excess AM space, use as variable capacity channel

char dispstr[65536];

#include "libwebsocketsglue.h"
//#include "multigateway.h"
//#include "nodecoin.h"
//#include "NXTmixer.h"


int32_t set_passwords(struct NXThandler_info *mp,int32_t argc, const char * argv[])
{
    if ( argc > 1 )
    {
        safecopy(mp->NXTACCTSECRET,"a7joF4LdyzPLYDio3uUdJQvhYRQZtARep",sizeof(mp->NXTACCTSECRET));
    }
    else
    {
#ifndef MAINNET
#ifdef __APPLE__
        //safecopy(mp->COINSECRET,"QUJScyTaNYWeSyXUeRo83XdWQN7qgCDGg6ipxi9oib3ArLriEZov");
        safecopy(mp->NXTACCTSECRET,"BD7KOuK5P78C2QzqDaVAlZIkkl1mV0",sizeof(mp->NXTACCTSECRET));
        safecopy(mp->NXTADDR,"11445347041779652448",sizeof(mp->NXTADDR));
        //safecopy(mp->NXTACCTSECRET,"a7joF4LdyzPLYDio3uUdJQvhYRQZtARep",sizeof(mp->NXTACCTSECRET));
        //safecopy(mp->NXTADDR,"18232225178877143084",sizeof(mp->NXTADDR));
        //issue_getBalance("11445347041779652448");
#else
        gen_randomacct(33,mp->NXTADDR,mp->NXTACCTSECRET,"randvals");
        //printf("usage: %s `./dogecoind dumpprivkey DOGEADDR` | argc.%d make sure daemon is running\n",argv[0],argc);
        // return(-1);
#endif
#endif
    }
    issue_startForging(mp->NXTACCTSECRET);
    return(0);
}

int32_t main(int32_t argc, const char * argv[])
{
    struct NXThandler_info *mp;    // seems safest place to have main data structure
#ifdef __APPLE__
    //init_NXTprotocol();
    //while ( 1 )
    //    sleep(1);
#endif
    mp = calloc(1,sizeof(*mp));
    Global_mp = mp;
    memset(mp,0,sizeof(*mp));
    init_jsoncodec(0);
    init_NXTAPI();
    safecopy(mp->ipaddr,get_ipaddr(),sizeof(mp->ipaddr));
#ifdef __APPLE__
    safecopy(mp->ipaddr,"181.47.159.125",sizeof(mp->ipaddr));
#endif
    mp->pollseconds = POLL_SECONDS;
    //mp->upollseconds = 100000;
    //#else
    //   mp->pollseconds = POLL_SECONDS;
    //#endif
    set_passwords(mp,argc,argv);
    safecopy(mp->NXTAPISERVER,NXTSERVER,sizeof(mp->NXTAPISERVER));
//register_NXT_handler(mp,2,-1,iDEXhandler,INSTANTDEX_SIG,1,0,0);
//goto skip;
#ifdef notnow
    struct gateway_info *gp;
    int32_t ispool,gatewayid;
    static char *whitelist[] = { NXTACCTA, NXTACCTB, NXTACCTC, "" };
    static char *assetlist[] = { DOGE_COINASSET, BTC_COINASSET, LTC_COINASSET, CGB_COINASSET, DRK_COINASSET, NODECOIN, "" };
    if ( strcmp(mp->ipaddr,MIXER_ADDR) == 0 )
        register_NXT_handler("NXTmixer",mp,NXTPROTOCOL_ILLEGALTYPE,NXTPROTOCOL_ILLEGALTYPE,mixerhandler,MIXER_SIG,1,0,0);
    gatewayid = is_gateway(mp->ipaddr);
    if ( gatewayid >= 0 )
    {
        printf("(%s) is gateway server\n",mp->ipaddr);
        gp = register_NXT_handler("multigateway",mp,2,-1,multigateway_handler,GATEWAY_SIG,1,0,whitelist);
        printf("(%s) is gateway server.%d\n",mp->ipaddr,gp->gatewayid);
    }
    else
    {
        if ( (ispool= is_nodecoin_server(mp->ipaddr)) > 0 )
        {
            printf("ispool.%d\n",ispool);
            register_NXT_handler("nodecoin",mp,2,-1,nodecoinhandler,NODECOIN_SIG,2,assetlist,0);
        }
        else
        {
#ifdef __APPLE__
            char *coins = "{\"ipaddr\":\"127.0.0.1\",\"BTC\":\"1AbKhsLqJLHExzQiLo6tt9oB5c5NKhq6ei\", \"LTC\":\"LRQ3ZyrZDhvKFkcbmC6cLutvJfPbxnYUep\", \"PPC\":\"PRv6K7Y7Kb6mXVZCRfPs1kYT1fcfq5eWLq\", \"DOGE\":\"DNyEii2mnvVL1BM1kTZkmKDcf8SFyqAX4Z\", \"DRK\":\"Xj4KK1vzBebvGrg8c5h4rr5C8kzXDojruK\", \"Ripple\":\"0\"}";
             AM_get_coindeposit_address(GATEWAY_SIG,mp->timestamp,"11445347041779652448",coins);
            register_NXT_handler("multigateway",mp,2,-1,multigateway_handler,GATEWAY_SIG,1,0,whitelist);
            //register_NXT_handler("InstantDEX",mp,2,-1,iDEXhandler,INSTANTDEX_SIG,1,0,0);
            /*AM_get_coindeposit_address(GATEWAY_SIG,0,"957183466007778257","{\"ipaddr\":\"127.0.0.1\",\"DOGE\":\"D9pkBsuGNh5ZLuDtc9uRuLJWSYbA9duUrq\"}");
             AM_get_coindeposit_address(GATEWAY_SIG,mp->timestamp,"7191893623143661625","{\"ipaddr\":\"127.0.0.1\",\"DOGE\":\"D5ikWozCC4sv8Vmdbh1YToaCPx5bBWzjmE\"}");
             AM_get_coindeposit_address(GATEWAY_SIG,mp->timestamp,"5728597073699734894","{\"ipaddr\":\"127.0.0.1\",\"BTC\":\"1LFuynLsAnwkSt3SPJkPAtEqpcbpP3t5ko\", \"DOGE\":\"D5Q6giAETxY5on9r4Jf82pc1RpY8qW87kr\", \"DRK\":\"XupGgkpoVdVLVhXQaAzFQncW5ZT6XMwio7\" }");
             AM_get_coindeposit_address(GATEWAY_SIG,mp->timestamp,"14525674777319574677","{\"ipaddr\":\"127.0.0.1\",\"BTC\":\"17PBvkuowHQ3tfdpX8fEzCopiLrH31VYtB\"}");
             AM_get_coindeposit_address(GATEWAY_SIG,mp->timestamp,"10960793819445211117","{\"ipaddr\":\"127.0.0.1\",\"BTC\":\"1DKPYdeqPyL4hyHbPbBzNUbGy682s6HpxN\"}");
             AM_get_coindeposit_address(GATEWAY_SIG,mp->timestamp,"13641213266085296507","{\"ipaddr\":\"127.0.0.1\",\"BTC\":\"1LYS6T4tyVG8APJZsxWpbnxkjAzBJDij39\"}");
             AM_get_coindeposit_address(GATEWAY_SIG,mp->timestamp,"14386024746077933238","{\"ipaddr\":\"127.0.0.1\",\"DOGE\":\"D9Tw8dVTNGKzii1cXom7ymw5HTrSGr2h5R\"}");
             AM_get_coindeposit_address(GATEWAY_SIG,mp->timestamp,"16248676195570366253","{\"ipaddr\":\"127.0.0.1\",\"BTC\":\"17Yc5N7gMYaU7waKag3XTt86vNnB3fdQAx\", \"DRK\":\"XbpU37SWogD4GatkpKZ6iYzAFT4oc82nCS\" }");
             AM_get_coindeposit_address(GATEWAY_SIG,mp->timestamp,"1562462127635514638","{\"ipaddr\":\"127.0.0.1\",\"BTC\":\"12zxBHsFsjzH7y9d6sRfPFy2MgX3gr2wFX\"}");*/
#endif
        }
    }
#endif
skip:
    mp->firsttimestamp = issue_getTime();
    start_NXTloops(mp,"14006431524478461177");//"12980781150532356708");
    while ( 1 )
        sleep(1);
}
