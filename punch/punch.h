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

#define RVZ_HOST        "209.126.71.170"  //"home.contextshift.co.uk"
#define RVZ_PORT        60218

#define UDP_MESSAGE_MAX 256
#define TCP_MESSAGE_MAX 256
#define ID_SIZE         32
#define NAME_SIZE       16
#define EMAIL_SIZE      64
#define STATUS_SIZE     64
#define IDENTITY_SIZE   (NAME_SIZE + EMAIL_SIZE + 2)
#define INTRO_SIZE     ((NAME_SIZE*3) + EMAIL_SIZE + 4)

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

char *stgsep(char **stringp, const char *delim);

char *stgncpy(char *s1, const char *s2, size_t n);

char *stgstrip(char *s, const char *cset);

char *print_host_address(const struct sockaddr_in *addr);

char *print_address(const struct sockaddr_in *addr);

int is_printable(const char *s);

long elapsed_ms(struct timeval *then);

#endif
