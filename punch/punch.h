/* Definitions
 
 Copyright (C) 2012  William Morris
 
 This program is free software: you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software Foundation, either version 3 of the License, or (at your option)
 any later version.
 
 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 
 wrm at basyl.co.uk
 */

#ifndef __PUNCH__H
#define __PUNCH__H
#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <assert.h>

#define EMERGENCY_PUNCH_SERVER        "209.126.75.158"  //"home.contextshift.co.uk"
#define NXT_PUNCH_PORT        6777
#define SYNC_MAXUNREPORTED 32
#define SYNC_FRAGSIZE 1024
#define NXTSUBATOMIC_SHAREDMEMSIZE (SYNC_FRAGSIZE * SYNC_MAXUNREPORTED/2)

#define PING_INTERVAL           1          // seconds
#define CONNECTION_TIMEOUT     (120 * 1000) // ms

#define UDP_MESSAGE_MAX 1400
#define TCP_MESSAGE_MAX 1400
#define ID_SIZE         32
#define NAME_SIZE       16
#define NXTADDR_SIZE    24
#define STATUS_SIZE     64
#define IDENTITY_SIZE   256
#define INTRO_SIZE     (TCP_MESSAGE_MAX - 16)   //((NAME_SIZE*3) + NXTADDR_SIZE + 4)

#define PUNCH_INITIATE  "initiate"
#define PUNCH_CONFIRM   "confirm"
#define PUNCH_DENY      "deny"

#ifndef __STDC_VERSION__
#define  __STDC_VERSION__ 0
#endif

#if __STDC_VERSION__ < 199901L
# if __GNUC__ >= 2
#  define __func__ __FUNCTION__
# else
#  define __func__ "<unknown>"
# endif
#endif


/* Suppress spurious 'will never be executed' warnings on OSX/gcc 4.2.1 */
#define memclear(s)                             \
do {                                        \
char *t = (s);                              \
memset(t, 0, sizeof (s));                   \
} while (0)

#define maxfd(a,b) ((a) > (b) ? (a) : (b))

// Group details.
typedef struct group
{
    char           name[NAME_SIZE+1];
    //char           pass[NAME_SIZE+1];
    int            n_members;
    struct group  *next;
    struct group  *prev;
} group_t;

// Client connection details.
typedef struct client
{
    struct sockaddr_in  addr;
    char                id[ID_SIZE+1];
    int                 sock;
    group_t            *group;
    
    char                user[NAME_SIZE+1];
    char                NXTaddr[NXTADDR_SIZE+1];
    char                status[STATUS_SIZE+1];
    unsigned char pubkey[crypto_box_PUBLICKEYBYTES];
    char pubkeystr[crypto_box_PUBLICKEYBYTES*2+1];
    
    struct client      *next;
    struct client      *prev;
} client_t;

// Group member details.
typedef struct member
{
    char name[NAME_SIZE + 1];
    char NXTaddr[NXTADDR_SIZE + 1];
    char status[STATUS_SIZE + 1];
    unsigned char pubkey[crypto_box_PUBLICKEYBYTES];
    char pubkeystr[crypto_box_PUBLICKEYBYTES*2+1];
    
    struct sockaddr_in addr;
    int fd;
    int have_address;
    
    struct timeval comm_time;
    struct timeval ping_time;
    uint32_t ping_nr,ipbits;
    uint16_t port;
    double avelag,ping_rtt;              // return trip time of pings
    
    struct member *next;
    struct member *prev;
    unsigned char sharedmem[];
} member_t;
struct member *member_list;
member_t *member_find_NXTaddr(const char *NXTaddr);

char *stgsep(char **stringp, const char *delim);
char *stgncpy(char *s1, const char *s2, size_t n);
char *stgstrip(char *s, const char *cset);
char *print_host_address(const struct sockaddr_in *addr);
char *print_address(const struct sockaddr_in *addr);
int is_printable(const char *s);
double elapsed_ms(struct timeval *then);

#define slog_error(s)  slog(": error in %s (%s): %s\n",       \
__func__, (s), strerror(errno))
#define slog_info(s)   slog(": %s\n", (s))
#define slog_addr(s,a) slog(": %s %s\n", (s), print_host_address(a))


static const char *punch_whitespace = " \t\n\r\v\f";

#include "punch-util.c"
#include "NXTsync.h"
#include "NXTmembers.h"
#include "punch-client.c"
#include "punch.c"

#endif
