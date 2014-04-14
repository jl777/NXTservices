//
//  multidefines.h
//  Created by jl777, March/April 2014
//  MIT License
//

#ifndef gateway_multidefines_h
#define gateway_multidefines_h

#define NUM_GATEWAYS 3
#define SERVER_NAMEA "209.126.71.170"
#define SERVER_NAMEB "209.126.73.156"
#define SERVER_NAMEC "209.126.73.158"
static char *Server_names[NUM_GATEWAYS+1] = { SERVER_NAMEA, SERVER_NAMEB, SERVER_NAMEC, "" };

#ifdef MAINNET
#define NXTISSUERACCT "10154506025773104943"
#define NXTACCTA "10154506025773104943"
#define NXTACCTB "10154506025773104943"
#define NXTACCTC "10154506025773104943"
#else
#define NXTACCTA NXTISSUERACCT  "423766016895692955"
#define NXTACCTB NXTISSUERACCT  "12240549928875772593"
#define NXTACCTC NXTISSUERACCT  "8279528579993996036"
#endif

#define GATEWAY_SIG 0xabcd1246
#define DEPOSIT_FREQUENCY 3
#define MAX_COINTXID_LEN 68
#define MAX_COINADDR_LEN 64
#define ILLEGAL_COIN "ERR"
#define ILLEGAL_COINASSET "0"
#define NUM_BITCOIND_RETRIES 3


#define GET_COINDEPOSIT_ADDRESS 'g'
//#define SET_COINWITHDRAW_ADDRESS 'w'

#define BIND_DEPOSIT_ADDRESS 'b'
#define DEPOSIT_CONFIRMED 'd'
//#define WITHDRAW_REQUEST 'w'
#define MONEY_SENT 'm'

// netstat -a # then localhost:<ports>
// sudo netstat -lnp | grep ':22555 '

#define BTC_COINID 1
#define BTC_CONF "../.bitcoin/bitcoin.conf"
#define BTC_TXFEE .0001           // don't forget to match txfee with coin
#define BTC_MIN_CONFIRMS 3
#define BTC_PORT "127.0.0.1:8332"
#define BTC_FIRST_BLOCKHEIGHT 295300
#define BTC_MARKER "17outUgtsnLkguDuXm14tcQ7dMbdD8KZGK"

#define LTC_COINID 2
#define LTC_CONF "../.litecoin/litecoin.conf"
#define LTC_TXFEE .001           // don't forget to match txfee with coin
#define LTC_MIN_CONFIRMS 3
#define LTC_PORT "127.0.0.1:9332"
#define LTC_FIRST_BLOCKHEIGHT 547689
#define LTC_MARKER "Le9hFCEGKDKp7qYpzfWyEFAq58kSQsjAqX"

#define CGB_COINID 3
#define CGB_CONF "../.cgbcoin/cgbcoin.conf"
#define CGB_TXFEE .0001           // don't forget to match txfee with coin
#define CGB_MIN_CONFIRMS 3
#define CGB_PORT "127.0.0.1:9902"
#define CGB_FIRST_BLOCKHEIGHT 104411
#define CGB_MARKER "PTqkPVfNkenMF92ZP8wfMQgQJc9DWZmwpB"

#define DOGE_COINID 4
#define DOGE_CONF "../.dogecoin/dogecoin.conf"
#define DOGE_TXFEE 1.0000           // don't forget to match txfee with coin
#define DOGE_MIN_CONFIRMS 3
#define DOGE_PORT "127.0.0.1:22555"
#define DOGE_FIRST_BLOCKHEIGHT 175713   //162555
#define DOGE_MARKER "D72Xdw5cVyuX9JLivLxG3V9awpvj7WvsMi"

#define DRK_COINID 5
#define DRK_CONF "../.darkcoin/darkcoin.conf"
#define DRK_TXFEE .01             // don't forget to match txfee with coin
#define DRK_MIN_CONFIRMS 3
#define DRK_PORT "127.0.0.1:9998"
#define DRK_FIRST_BLOCKHEIGHT 49594
#define DRK_MARKER "XiiSWYGYozVKg3jyDLfJSF2xbieX15bNU8"

#define ANC_COINID 6
#define ANC_COINASSET ""
#define ANC_CONF "../.anoncoin/anoncoin.conf"
#define ANC_TXFEE .0001             // don't forget to match txfee with coin
#define ANC_MIN_CONFIRMS 3
#define ANC_PORT "127.0.0.1:9998"
#define ANC_FIRST_BLOCKHEIGHT 0
#define ANC_MARKER "D72Xdw5cVyuX9JLivLxG3V9awpvj7WvsMi"

#define USD_COINID 7
#define CNY_COINID 8



#endif
