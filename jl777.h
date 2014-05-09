
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
#include <sys/wait.h>
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
#include "libuv/uv.h"
#else
#include "../libuv/include/uv.h"
#endif
#include <curl/curl.h>
#include <curl/easy.h>

#else
#include "utils/curl/curl.h"
#include "utils/curl/easy.h"

#include <windows.h>
//#include "utils/pthread.h"
#include "../libuv/include/uv.h"
#include "libwebsockets/win32port/win32helpers/gettimeofday.h"
#define STDIN_FILENO 0
void sleep(int);
void usleep(int);
#endif

//#define OLDWAY
#ifdef OLDWAY
#define portable_udp_t int32_t
#define portable_tcp_t int32_t
#else
#define portable_udp_t uv_udp_t
#define portable_tcp_t uv_tcp_t
#endif

//#define portable_mutex_t pthread_mutex_t
#define portable_mutex_t uv_mutex_t
#define portable_thread_t uv_thread_t

static int32_t portable_mutex_init(portable_mutex_t *mutex)
{
    return(uv_mutex_init(mutex));
    //pthread_mutex_init(mutex,NULL);
}

static void portable_mutex_lock(portable_mutex_t *mutex)
{
    //printf("lock.%p\n",mutex);
    uv_mutex_lock(mutex);
    // pthread_mutex_lock(mutex);
}

static void portable_mutex_unlock(portable_mutex_t *mutex)
{
   // printf("unlock.%p\n",mutex);
    uv_mutex_unlock(mutex);
    //pthread_mutex_unlock(mutex);
}

static portable_thread_t *portable_thread_create(void *funcp,void *argp)
{
    portable_thread_t *ptr;
    ptr = malloc(sizeof(portable_thread_t));
    if ( uv_thread_create(ptr,funcp,argp) != 0 )
        //if ( pthread_create(ptr,NULL,funcp,argp) != 0 )
    {
        free(ptr);
        return(0);
    } else return(ptr);
}

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
#define NODECOIN_POOLSERVER_ADDR "11445347041779652448"
#define NXTCOINSCO_PORT 8777
#define NXTPROTOCOL_WEBJSON 7777
#define NXT_PUNCH_PORT 6777
#define NXTSYNC_PORT 5777

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
    int32_t capacity,size,in,out,initflag;
	portable_mutex_t mutex;
	//pthread_cond_t cond_full;
	//pthread_cond_t cond_empty;
} queue_t;
//#define QUEUE_INITIALIZER(buffer) { buffer, sizeof(buffer) / sizeof(buffer[0]), 0, 0, 0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER }

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
    portable_mutex_t hash_mutex;
    void *handlerdata;
    char *origblockidstr,lastblock[256],blockidstr[256];
    queue_t hashtable_queue[2];
    struct hashtable **NXTaccts_tablep,**NXTassets_tablep,**NXTasset_txids_tablep,**NXTguid_tablep,**NXTsyncaddrs_tablep;
    cJSON *accountjson;
    unsigned char session_pubkey[crypto_box_PUBLICKEYBYTES],session_privkey[crypto_box_SECRETKEYBYTES];
    char pubkeystr[crypto_box_PUBLICKEYBYTES*2+1];
    void *pm;
    uint64_t nxt64bits;
    uv_tty_t *stdoutput;
    portable_tcp_t Punch_tcp;
    uv_connect_t Punch_connect;
    int32_t isudpserver,istcpserver,corresponding,myind,NXTsync_sock,deadmantime;
    char ipaddr[64],NXTAPISERVER[128],NXTADDR[64],NXTACCTSECRET[256],dispname[128],groupname[128];
    char Punch_servername[128],Punch_connect_id[512],otherNXTaddr[MAX_NXTADDR_LEN];
};
struct NXT_acct *get_NXTacct(int32_t *createdp,struct NXThandler_info *mp,char *NXTaddr);
extern struct NXThandler_info *Global_mp;

#define NXTPROTOCOL_INIT -1
#define NXTPROTOCOL_IDLETIME 0
#define NXTPROTOCOL_NEWBLOCK 1
#define NXTPROTOCOL_AMTXID 2
#define NXTPROTOCOL_TYPEMATCH 3
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

int Enable_tcp_dispatch;
extern uv_loop_t *UV_loop;
queue_t UDPsend_queue;
struct UDP_queue { struct sockaddr addr; portable_udp_t *usockp; int32_t len; unsigned char *buf; };
int64_t microseconds();

void on_read(uv_udp_t *req,ssize_t nread,const uv_buf_t *buf,const struct sockaddr *addr,unsigned int flags);

void alloc_buffer(uv_handle_t *handle,size_t suggested_size,uv_buf_t *buf)
{
    buf->base = malloc(suggested_size);
    buf->len = (int32_t)suggested_size;
}

int32_t queue_UDPsend(void *tag,int32_t taglen,void *buf,int32_t len)
{
    struct UDP_queue *qp;
    qp = calloc(1,sizeof(*qp));
    qp->usockp = tag;
    qp->len = taglen;
    if ( buf != 0 && len > 0 )
    {
        qp->buf = malloc(len);
        memcpy(qp->buf,buf,len);
    }
    queue_enqueue(&UDPsend_queue,qp);
    return(taglen);
}

void portable_set_illegaludp(portable_udp_t *usockp)
{
#ifdef OLDWAY
    *usockp = -1;
#else
#ifdef WIN32
    usockp->socket = -1;
#else
    usockp->io_watcher.fd = -1;
#endif
#endif
}

int32_t portable_isillegal_udp(portable_udp_t *usockp)
{
#ifdef OLDWAY
    return(*usockp < 0);
#else
#ifdef WIN32
    return((int32_t)usockp->socket < 0);
#else
    return((int32_t)usockp->io_watcher.fd < 0);
#endif
#endif
}

void default_closehandler(uv_handle_t *handle)
{
    printf("default_closehandler\n");
    *(int32_t *)handle->data = 1;
}

int32_t portable_udp_close(portable_udp_t *usockp)
{
#ifdef OLDWAY
    return(close(*usockp));
#else
#ifdef WIN32
    //closesocket(usockp->socket);
    //usockp->socket = -1;
    return(queue_UDPsend(usockp,0,usockp,sizeof(*usockp)));
    //while ( completed == 0 )
    //    usleep(100000);
    printf("udp close completed isillegal.%d socket.%d\n",portable_isillegal_udp(usockp),usockp->socket);
#else
#ifdef __APPLE__
    //int32_t completed = 0;
    //printf("portable_udp close\n");
    //usockp->data = &completed;
    uv_close((uv_handle_t *)usockp,0);
    //while ( completed == 0 )
    //    usleep(100000);
    //memset(usockp,0,sizeof(*usockp));
    //usockp->io_watcher.fd = -1;
    printf("udp close completed isillegal.%d socket.%d\n",portable_isillegal_udp(usockp),usockp->io_watcher.fd);
#else
    uv_close((uv_handle_t *)usockp,0);
    //return(queue_UDPsend(usockp,0,usockp,sizeof(*usockp)));
#endif
#endif
    return(0);
#endif
}

int32_t portable_tcp_close(portable_tcp_t *tsockp)
{
#ifdef OLDWAY
    close(*tsockp);
    printf("close tcp.%d\n",*tsockp);
    *tsockp = -1;
    return(0);
#else
#ifdef WIN32
    //int32_t completed = 0;
    //tsockp->data = &completed;
    uv_close((uv_handle_t *)tsockp,0);
    //return(queue_UDPsend(tsockp,0,tsockp,sizeof(*tsockp)));
    //while ( completed == 0 )
    //    usleep(100);
    printf("tcp close completed\n");
#else
    uv_close((uv_handle_t *)tsockp,0);
#endif
    return(0);
#endif
}

int32_t portable_udpsocket(portable_udp_t *usockp,int32_t port,int32_t bidirectional)
{
#ifdef OLDWAY
    *usockp = socket(AF_INET,SOCK_DGRAM,0);
    printf("make udp socket %d\n",*usockp);
    return(0);
#else
    //struct sockaddr_in recv_addr;
    int32_t retval;
    retval = uv_udp_init(UV_loop,usockp);
    //uv_ip4_addr("0.0.0.0",port,&recv_addr);
    //uv_udp_bind(usockp,(struct sockaddr *)&recv_addr,0);
    uv_udp_recv_start(usockp,alloc_buffer,on_read);
    if ( bidirectional != 0 )
        uv_udp_set_broadcast(usockp,1);
    //printf("portable_udpsocket retval.%d fd.%d\n",retval,usockp->io_watcher.fd);
    //*usockp = usock.io_watcher.fd;
    return(retval);
#endif
}

int32_t portable_udpsend(portable_udp_t *usockp,void *buf,int32_t len,void *addr)
{
#ifdef OLDWAY
    printf("send %d bytes to socket.%d\n",len,*usockp);
    return((int32_t)sendto(*usockp,buf,len,0,(struct sockaddr *)addr,sizeof(struct sockaddr)));
#else
    //return(queue_UDPsend(usockp,len,usockp,sizeof(*usockp)));
#ifdef WIN32
    printf("send %d bytes to socket.%d\n",len,usockp->socket);
    return((int32_t)sendto(usockp->socket,buf,len,0,(struct sockaddr *)addr,sizeof(struct sockaddr)));
    //uv_udp_send_t *send_req;
   // uv_buf_t msg = uv_buf_init((void *)buf,len);
    //send_req = malloc(sizeof(*send_req));
    //printf("portable send.%p len.%d req.%p @ %lld\n",buf,len,&send_req,(long long)microseconds());
    //uv_udp_send(send_req,usockp,&msg,1,addr,0);
    //return(len);
    //return(queue_UDPsend(usockp,len,usockp,sizeof(*usockp)));
#else
    printf("send %d bytes to socket.%d\n",len,usockp->io_watcher.fd);
    return((int32_t)sendto(usockp->io_watcher.fd,buf,len,0,(struct sockaddr *)addr,sizeof(struct sockaddr)));
#endif
#endif
}

void generic_wcallback(uv_write_t *wreq,int status)
{
    printf("got wcallback %p\n",wreq->data);
    if ( status != 0 )
        printf("error status.%d in callback func data.%p\n",status,wreq->data);
    if ( wreq->data != 0 )
        *(int32_t *)wreq->data = (status == 0) ? 1 : -1;
    free(wreq);
}

int32_t TCP_bytes_avail; unsigned char TCP_bytes[65536];
void tcp_gotbytes(uv_stream_t *handle,ssize_t nread,const uv_buf_t *buf)
{
    long len;
    //uv_shutdown_t* req;
    if ( nread < 0 )
    {
        // Error or EOF
        //ASSERT(nread == UV_EOF);
        //req = (uv_shutdown_t *)malloc(sizeof(*req));
        //uv_shutdown(req,handle,after_shutdown);
        printf("tcp_gotbytes got error.%ld\n",nread);
    }
    else if ( nread == 0 )
    {
        // Everything OK, but nothing read. 
    }
    else
    {
        void tcp_io(char *line,long n,char *servername,char *connect_id);
        printf(">>>>>>>>>>>>>>>>>>> got %ld bytes at %p.(%s)\n",nread,buf->base,buf->base);
        len = nread < (ssize_t)sizeof(TCP_bytes) ? nread : sizeof(TCP_bytes);
        if ( Enable_tcp_dispatch != 0 )
            tcp_io(buf->base,nread,Global_mp->Punch_servername,Global_mp->Punch_connect_id);
        memcpy(TCP_bytes,buf->base,len);
        TCP_bytes_avail = (int32_t)len;
    }
    if ( buf->base != 0 )
        free(buf->base);
}

void on_connect(uv_connect_t *connect,int status)
{
    uv_write_t *wreq;
    printf("on_connect.%d\n",status);
    if ( status < 0 )
    {
        printf("got on_connect error status.%d\n",status);
        return;
    }
    if ( uv_read_start(connect->handle,alloc_buffer,tcp_gotbytes) != 0 )
        printf("uv_read_start failed\n");
    else
    {
        if ( connect->data != 0 )
        {
            wreq = calloc(1,sizeof(*wreq));
            wreq->data = (void *)((long)connect->data + sizeof(uv_buf_t));
            if ( uv_write(wreq,connect->handle,connect->data,1,generic_wcallback) != 0 )
                printf("uv_write failed.(%s).%ld\n",((uv_buf_t *)connect->data)->base,((uv_buf_t *)connect->data)->len);
        }
    }
}

int32_t portable_tcp_connect(uv_connect_t *connect,portable_tcp_t *tsockp,struct sockaddr_in *addr,char *greetmsg,int64_t utimeout)
{
#ifdef OLDWAY
    int server_connect_tcp(struct sockaddr_in *addr,socklen_t addr_len);
    *tsockp = server_connect_tcp(addr,sizeof(*addr));
    return(*tsockp > 0);
#else
    long len;
    int64_t starttime;
    int32_t *completionflagp = 0;
    uv_buf_t *buf;
    uv_tcp_init(UV_loop,tsockp);
    if ( greetmsg != 0 && greetmsg[0] != 0 )
    {
        len = strlen(greetmsg);
        buf = calloc(1,sizeof(*buf) + strlen(greetmsg) + 1 + sizeof(int32_t));
        completionflagp = (void *)((long)buf + sizeof(*buf));
        buf->base = (void *)((long)buf + sizeof(*buf) + sizeof(int32_t));
        buf->len = len;
        memcpy(buf->base,greetmsg,len);
        connect->data = buf;
        printf("connect->data %p\n",connect->data);
    } else connect->data = 0;
    if ( uv_tcp_connect(connect,tsockp,(struct sockaddr *)addr,on_connect) != 0 )
    {
        printf("error making tcp connection\n");
        if ( connect->data != 0 )
        {
            free(connect->data);
            connect->data = 0;
        }
        return(-1);
    }
    if ( connect->data != 0 )
    {
        starttime = microseconds();
        printf("wait for completion\n");
        while ( microseconds() < (starttime + utimeout) && *completionflagp == 0 )
            usleep(100);
        printf("completed send of.(%s) completion.%d elapsed.%lld\n",greetmsg,*completionflagp,(long long)(microseconds() - starttime));
        free(connect->data);
        connect->data = 0;
    }
    return(0);
#endif
}

int32_t portable_tcp_read(portable_tcp_t *tcp,void *buf,int32_t len,int64_t utimeout) // blocking
{
#ifdef OLDWAY
    return((int32_t)read(*tcp,buf,len));
#else
    int64_t starttime = microseconds();
    while ( microseconds() < (starttime + utimeout) )
    {
        if ( TCP_bytes_avail > 0 )
        {
            if ( TCP_bytes_avail < len )
                len = TCP_bytes_avail;
            memcpy(buf,TCP_bytes,len);
            TCP_bytes_avail = 0;
            printf("returning.(%s) TCP_bytes.%d\n",TCP_bytes,len);
            return(len);
        }
    }
    return(-1);
#endif
}

int32_t portable_tcp_send(portable_tcp_t *tsockp,void *buf,int32_t len)
{
    printf(">>>>>>>>>>>>>>> tcpsend.(%s)\n",(char *)buf);
#ifdef OLDWAY
    return((int32_t)send(*tsockp,buf,len,0));
#else
    uv_buf_t uvbuf;
    uv_write_t *wreq = calloc(1,sizeof(*wreq));
    uvbuf = uv_buf_init(buf,len);
    if ( uv_write(wreq,(uv_stream_t *)tsockp,&uvbuf,1,generic_wcallback) != 0 )
        printf("uv_write failed.(%s).%d\n",(char *)buf,len);
    return(0);
#endif
}


int32_t UDP_sending;
void UDPsend_done(uv_udp_send_t *req,int status)
{
    //printf("UDPsend_done.%d sendreq.%p status.%d @ %lld\n",UDP_sending,req,status,(long long)microseconds());
    free(req);
    UDP_sending--;
}

void process_UDPsend_queue()
{
    int32_t sock;
#ifndef OLDWAY
    static int64_t nexttime;
    uv_udp_send_t *send_req;
    struct UDP_queue *qp;
    if ( microseconds() > nexttime && UDP_sending == 0 && (qp= queue_dequeue(&UDPsend_queue)) != 0 )
    {
        if ( qp->len == 0 )
        {
            if ( queue_size(&UDPsend_queue) == 0 )
            {
                if ( qp->usockp != 0 )
                {
                    //printf("pending close udp\n");
                    //uv_close((uv_handle_t *)qp->buf, NULL);
                    int32_t completed = 0;
                    printf("portable_udp close\n");
                    qp->usockp->data = &completed;
                    uv_close((uv_handle_t *)qp->usockp,default_closehandler);
                    while ( completed == 0 )
                        usleep(100000);
#ifdef WIN32
                    qp->usockp->socket = -1;
#else
                    qp->usockp->io_watcher.fd = -1;
#endif
                }
                else
                {
                    printf("pending close tcp\n");
                    uv_close((uv_handle_t *)qp->buf, NULL);
                }
            }
            else
            {
                queue_enqueue(&UDPsend_queue,qp);
                return;
            }
        }
        else if ( portable_isillegal_udp(qp->usockp) == 0 )//qp->usockp->io_watcher.fd >= 0 )
        {
#ifdef WIN32
            sock = qp->usockp->socket;
#else
            sock = qp->usockp->io_watcher.fd;
#endif
            sendto(sock,qp->buf,qp->len,0,(struct sockaddr *)&qp->addr,sizeof(struct sockaddr));
            //uv_buf_t msg = uv_buf_init((void *)qp->buf,qp->len);
            //send_req = malloc(sizeof(*send_req));
            printf("portable send.%p len.%d req.%p @ %lld\n",qp->buf,qp->len,&send_req,(long long)microseconds());
            //uv_udp_send(send_req,qp->usockp,&msg,1,&qp->addr,UDPsend_done);
#ifdef __APPLE__
            double rate_per_us = .5; // mbps
#else
            double rate_per_us = .5; // mbps
#endif
            nexttime = microseconds() + (qp->len*10)/rate_per_us; // x10 to include overhead bits of low level network packet
            //UDP_sending++;
        }
        free(qp);
    }
#endif
}

void NXTprotocol_idler(uv_idle_t *handle)
{
    static int64_t nexttime;
    int32_t process_syncmem_queue();
    void call_handlers(struct NXThandler_info *mp,int32_t mode);
    static uint64_t counter;
    if ( 0 && counter == 0 )
    {
        uv_tty_t *init_stdinout(uv_read_cb read_cb);
        Global_mp->stdoutput = init_stdinout(0);
        printf("stdinout done\n");
    }
    if ( (counter++ % 1000) == 0 && microseconds() > nexttime )
    {
        call_handlers(Global_mp,NXTPROTOCOL_IDLETIME);
        nexttime = (microseconds() + 1000000);
    }
    while ( process_syncmem_queue() > 0 )
        ;
    process_UDPsend_queue();
}

#include "utils/NXTutils.h"
#include "utils/NXTsock.h"
#include "punch.h"

#endif
