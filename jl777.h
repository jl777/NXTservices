
//  Created by jl777
//  MIT License
//

#ifndef gateway_jl777_h
#define gateway_jl777_h

#define DEBUG_MODE

// system includes
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <memory.h>
#include <math.h>
//#include <time.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <float.h>
//#include <limits.h>
#include <zlib.h>

#ifndef WIN32
//#include <getopt.h>
//#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
//#include <sys/socket.h>
//#include <sys/wait.h>
//#include <sys/time.h>
//#include <pthread.h>

#include <sys/mman.h>   // only for support of map_file and release_map_file in NXTutils.h
#include <fcntl.h>

#define ENABLE_DIRENT
#ifdef ENABLE_DIRENT
#include <sys/stat.h>   // only NXTmembers.h
#include <dirent.h>     //only NXTmembers.h
#endif

#ifdef __APPLE__
//#include "libuv/uv.h"
#else
#include "../libuv/include/uv.h"
#endif
#include <curl/curl.h>
#include <curl/easy.h>

#else
#include "curl/curl.h"
#include "curl/easy.h"

#include <windows.h>
//#include "utils/pthread.h"
#include "../libuv/include/uv.h"
#include "libwebsockets/win32port/win32helpers/gettimeofday.h"
#define STDIN_FILENO 0
void sleep(int);
void usleep(int);
#endif

// includes that include actual code
#include "nacl/crypto_box.h"
#include "nacl/randombytes.h"
void *jl777malloc(size_t allocsize) { void *ptr = malloc(allocsize); if ( ptr == 0 ) { printf("malloc(%ld) failed\n",allocsize); while ( 1 ) sleep(60); } return(ptr); }
void *jl777calloc(size_t num,size_t allocsize) { void *ptr = calloc(num,allocsize); if ( ptr == 0 ) { printf("calloc(%ld,%ld) failed\n",num,allocsize); while ( 1 ) sleep(60); } return(ptr); }
#define malloc jl777malloc
#define calloc jl777calloc

#include "guardians.h"
#include "utils/cJSON.h"
#include "utils/jl777str.h"
#include "utils/cJSON.c"
#include "utils/bitcoind_RPC.c"
#include "utils/jsoncodec.h"
#include "libtom/crypt_argchk.c"
#include "libtom/sha256.c"
#include "libtom/rmd160.c"

#define BTC_COINASSET "10235881003283394223"
#define LTC_COINASSET "18237314836568240804"
#define CGB_COINASSET "17262323417894903558"
#define DOGE_COINASSET "9035587034804564125"
#define DRK_COINASSET "13015637232441376361"
#define NODECOIN "9096665501521699628"
#define USD_COINASSET "4562283093369359331"
#define CNY_COINASSET "13983943517283353302"

#define NXTISSUERACCT "18232225178877143084"
#define GENESISACCT "1739068987193023818"
#define NODECOIN_SIG 0x63968736
#define POOLSERVER "209.126.73.160"
#define NXTACCTA "423766016895692955"
#define SERVER_NAMEA "209.126.71.170"

#define USD "3759130218572630531"
#define CNY "17293030412654616962"
#define DEFAULT_NXT_DEADLINE 720

#ifdef __APPLE__
#define NXTSERVER "http://127.0.0.1:6876/nxt?requestType"
//#define NXTSERVER "http://tn01.nxtsolaris.info:6876/nxt?requestType"
//#define NXTSERVER "http://209.126.73.160:6876/nxt?requestType"
//#define NXTSERVER "http://node10.mynxtcoin.org:6876/nxt?requestType"
#else
#ifdef MAINNET
#define NXTSERVER "http://127.0.0.1:7876/nxt?requestType"
#else
#define NXTSERVER "http://127.0.0.1:6876/nxt?requestType"
//#define NXTSERVER "http://node10.mynxtcoin.org:6876/nxt?requestType"
#endif
#endif

#define SERVER_PORT 3013
#define SERVER_PORTSTR "3013"

#define SATOSHIDEN 100000000L
#define MIN_NQTFEE SATOSHIDEN
#define MIN_NXTCONFIRMS 10 
#define NXT_TOKEN_LEN 160
#define MAX_NXT_STRLEN 24
#define MAX_NXTTXID_LEN MAX_NXT_STRLEN
#define MAX_NXTADDR_LEN MAX_NXT_STRLEN
#define POLL_SECONDS 1
#define NXT_ASSETLIST_INCR 100

typedef struct queue
{
	void **buffer;
    int32_t capacity;
	int32_t size;
	int32_t in;
	int32_t out;
	pthread_mutex_t mutex;
	//pthread_cond_t cond_full;
	//pthread_cond_t cond_empty;
} queue_t;

struct pingpong_queue
{
    char *name;
    queue_t pingpong[2],*destqueue,*errorqueue;
    int32_t (*action)();
};

union NXTtype { uint64_t nxt64bits; uint32_t uval; int32_t val; int64_t lval; double dval; char *str; cJSON *json; };

struct NXT_AMhdr
{
    uint32_t sig;
    int32_t size;
    uint64_t nxt64bits;
};

struct json_AM
{
    struct NXT_AMhdr H;
	uint32_t funcid,gatewayid,timestamp,jsonflag;
    union { unsigned char binarydata[sizeof(struct compressed_json)]; char jsonstr[sizeof(struct compressed_json)]; struct compressed_json jsn; };
};

struct NXT_str
{
    int64_t modified,nxt64bits;
    union { char txid[MAX_NXTTXID_LEN]; char NXTaddr[MAX_NXTADDR_LEN];  char assetid[MAX_NXT_STRLEN]; };
};

struct NXThandler_info
{
    double fractured_prob;  // probability NXT network is fractured, eg. major fork or attack in progress
    int32_t upollseconds,pollseconds,firsttimestamp,timestamp,deadman,RTflag,NXTheight,histmode,hashprocessing;
    int64_t acctbalance;
    pthread_mutex_t hash_mutex;
    void *handlerdata;
    char *origblockidstr,lastblock[256],blockidstr[256];
    queue_t hashtable_queue[2];
    struct hashtable **NXTaccts_tablep,**NXTassets_tablep,**NXTasset_txids_tablep,**NXTguid_tablep,**NXTsyncaddrs_tablep;
    cJSON *accountjson;
    unsigned char session_pubkey[crypto_box_PUBLICKEYBYTES],session_privkey[crypto_box_SECRETKEYBYTES];
    char pubkeystr[crypto_box_PUBLICKEYBYTES*2+1];
    void *pm;
    uint64_t nxt64bits;
    int32_t isudpserver,istcpserver,corresponding,myind,server_tcp,NXTsync_sock;
    char ipaddr[64],NXTAPISERVER[128],NXTADDR[64],NXTACCTSECRET[256],dispname[128],groupname[128];
    char servername[128],connect_id[512],otherNXTaddr[MAX_NXTADDR_LEN];
};
struct NXT_acct *get_NXTacct(int32_t *createdp,struct NXThandler_info *mp,char *NXTaddr);
extern struct NXThandler_info *Global_mp;

#define NXTPROTOCOL_INIT -1
#define NXTPROTOCOL_IDLETIME 0
#define NXTPROTOCOL_NEWBLOCK 1
#define NXTPROTOCOL_AMTXID 2
#define NXTPROTOCOL_TYPEMATCH 3
#define NXTPROTOCOL_WEBJSON 7777
#define NXTPROTOCOL_ILLEGALTYPE 666

struct NXT_protocol_parms
{
    cJSON *argjson;
    int32_t mode,type,subtype,priority,histflag;
    char *txid,*sender,*receiver;
    void *AMptr;
    char *assetid,*comment;
    int64_t assetoshis;
};

typedef void *(*NXT_handler)(struct NXThandler_info *mp,struct NXT_protocol_parms *parms,void *handlerdata);

struct NXT_protocol
{
    int32_t type,subtype,priority;
    uint32_t AMsigfilter;
    void *handlerdata;
    char **assetlist,**whitelist;
    NXT_handler NXT_handler;
    char name[64];
    char *retjsontxt;
    long retjsonsize;
};

struct NXT_protocol *NXThandlers[1000]; int Num_NXThandlers;

#define SETBIT(bits,bitoffset) (((unsigned char *)bits)[(bitoffset) >> 3] |= (1 << ((bitoffset) & 7)))
#define GETBIT(bits,bitoffset) (((unsigned char *)bits)[(bitoffset) >> 3] & (1 << ((bitoffset) & 7)))
#define CLEARBIT(bits,bitoffset) (((unsigned char *)bits)[(bitoffset) >> 3] &= ~(1 << ((bitoffset) & 7)))
#ifndef MIN
#define MIN(x,y) (((x)<=(y)) ? (x) : (y))
#endif
#ifndef MAX
#define MAX(x,y) (((x)>=(y)) ? (x) : (y))
#endif

char *bitcoind_RPC(char *debugstr,int32_t numretries,char *url,char *userpass,char *command,char *args);
#define issue_curl(cmdstr) bitcoind_RPC("NXT",NUM_BITCOIND_RETRIES,cmdstr,0,0,0)
#define fetch_URL(cmdstr) bitcoind_RPC("fetch",0,cmdstr,0,0,0)
void gen_testforms();
#include "NXTprotocol.h"

#include "utils/jl777hash.h"
#include "utils/NXTutils.h"
#include "utils/NXTsock.h"
#include "punch.h"

#endif
