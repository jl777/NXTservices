//
//  NXTsync.h
//  Created by jl777, April 26-27 2014
//  MIT License
//


#ifndef gateway_NXTsync_h
#define gateway_NXTsync_h

#define SYNC_MAPPEDFILE 0x80
#define SYNC_RECVFILE 1
#define SYNC_SENDFILE 2
#define SYNC_SYNCFILE 3
#define SYNC_IS_FILE(sfp) (sfp->mappedflag != 0)
#define SYNC_IS_SHAREDMEM(sfp) (sfp->mappedflag == 0)
#define SYNC_IS_SEND(sfp) ((sfp)->syncflag & SYNC_SENDFILE)
#define SYNC_IS_RECV(sfp) ((sfp)->syncflag & SYNC_RECVFILE)

#define SYNC_DEFAULT_DELAY 25.
#define SYNC_MAXPENDING_PACKETS 64

struct sync_file
{
    char name[512],syncname[8];
    void *ptr,*refptr;
    member_t *pm;
    uint64_t xferred,filesize,remaining,lastcontact,started,finished,lastsend;
    uint32_t totalcrc,unreported[SYNC_MAXUNREPORTED][2];
    int32_t sendi,incr,numincr,acki,mappedflag,syncflag,reporti,lastrecvi,numtoreport,changepending;
    uint32_t crcs[]; // even are local crc, odd are remote crc
};

struct ping_packet
{
    char zero,pad;
    uint16_t port;
    uint32_t ipbits,srvipbits,ping_nr,totalcrc;
    uint64_t nxt64bits,other64bits,srv64bits;
    char syncname[8];
    uint32_t unreported[];
};

struct syncmem_queued_packet { uint64_t nxt64bits; int32_t n; char buf[]; };
queue_t syncmem_queue;

uint64_t *Active_NXTaddrs;
uint32_t *Punch_servers,*IPaddrs;
uint16_t *IPports;
int Num_punch_servers,Num_ipaddrs;

struct pending_sync_files { struct sync_file **sfps; int32_t num,max; pthread_mutex_t mutex; };
struct pending_sync_files RECVS,SENDS;
