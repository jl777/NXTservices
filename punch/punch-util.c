/* Title: Utility functions
 Latest version: http://basyl.co.uk/code/punch/util.c.html */

/* Copyright (C) 2012  William Morris
 
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

#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>


/* Function: stgsep
 
 Derived from strsep() but modifies the output value in stringp.  Locates,
 in the string referenced by *stringp, the first occurrence of any character
 in the string delim (or the terminating \0 character) and replaces it with
 a \0.  The location of the next character after the delimiter character (or
 the terminating \0, if the end of the string was reached) is stored in
 *stringp (strsep stores NULL in this case).  The original value of *stringp
 is returned.
 
 An 'empty' field (i.e., a character in the string delim occurs as the first
 character of *stringp) can be detected by comparing the location referenced
 by the returned pointer to \0.
 */
char *
stgsep(char **stringp, const char *delim)
{
    char *s = strsep(stringp, delim);
    if (*stringp == NULL)
        *stringp = s + strlen(s);
    return s;
}


/* Function: stgncpy
 
 Derived from strncpy() but guarantees the destination string is terminated.
 Copies at most n-1 characters from s2 into s1.  If s2 is less than n
 characters long, the remainder of s1 is filled with \0 characters.
 Otherwise, s1 *is* terminated.
 
 The source and destination strings should not overlap, as the behavior is
 undefined.
 
 Returns s1.
 */
char *
stgncpy(char *s1, const char *s2, size_t n)
{
    strncpy(s1, s2, n);
    s1[n-1] = '\0';
    return s1;
}


/* Function: stgstrip
 
 Strip whitespace from each end of a string.  Actualy it strips the
 characters in the character set cset from each end of the string.
 */
char *
stgstrip(char *s, const char *cset)
{
    char *start = s + strspn(s, cset);
    size_t len = strlen(start);
    char *end = start + len;
    
    while ((end > start) && strchr(cset, end[-1]))
        --end;
    
    len = end - start;
    if (s != start)
        memmove(s, start, len);
    s[len] = '\0';
    return s;
}


/* Function: print_host_address
 
 Resolve the given internet address to a host name and print the address,
 port number and host to a string.  The address is printed in dotted format
 (eg "text 192.168.0.1:6008 (unknown)").
 */
char *
print_host_address(const struct sockaddr_in *addr)
{
    static char buf[128];
    char *hostname = "unknown";
    struct hostent *hp = gethostbyaddr(&addr->sin_addr,
                                       sizeof addr->sin_addr, AF_INET);
    if (hp)
        hostname = hp->h_name;
    
    snprintf(buf, sizeof buf,
             "%s:%d (%s)",
             inet_ntoa(addr->sin_addr), ntohs(addr->sin_port), hostname);
    return buf;
}




/* Function: print_address
 
 Print the given internet address and port number to a string.  The address
 is printed in dotted format (eg "text 192.168.0.1:6008").
 */
char *
print_address(const struct sockaddr_in *addr)
{
    static char buf[128];
    
    snprintf(buf, sizeof buf,
             "%s:%d",
             inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
    return buf;
}



/* Function: is_printable
 
 Return non-zero if all characters in the given string are printable.
 */
int
is_printable(const char *s)
{
    for (; *s; ++s)
        if (!isprint(*s))
            return 0;
    return 1;
}


/* Function: elapsed_ms
 
 Subtract the given time value from current time to give and elapsed time in
 milliseconds.  The supplied time must be in the past.
 */
long
elapsed_ms(struct timeval *then)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    
    return
    ((now.tv_sec  - then->tv_sec)  * 1000) +
    ((now.tv_usec - then->tv_usec) / 1000);
}
