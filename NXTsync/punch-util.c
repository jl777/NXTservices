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
#ifndef punchutils
#define punchutils


static int prompt_again = 1;
static int bell = 1;
static int print_ping = 1;
static int EXIT_FLAG;
static char *log_file = NULL;

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
    struct hostent *hp = gethostbyaddr(&addr->sin_addr,sizeof addr->sin_addr,AF_INET);
    if ( hp != 0 )
        hostname = hp->h_name;
    snprintf(buf, sizeof buf,"%s:%d (%s)",inet_ntoa(addr->sin_addr), ntohs(addr->sin_port), hostname);
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
int is_printable(const char *s)
{
    if ( s == 0 || *s == 0 )
        return(0);
    for (; *s; ++s)
        if (!isprint(*s))
            return 0;
    return 1;
}


/* Function: elapsed_ms
 
 Subtract the given time value from current time to give and elapsed time in
 milliseconds.  The supplied time must be in the past.
 */
double
elapsed_ms(struct timeval *then)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    
    return
    ((now.tv_sec  - then->tv_sec)  * 1000) +
    ((now.tv_usec - then->tv_usec) / 1000);
}

// Function: fprint
// Print like fprintf but also set global 'prompt_again' to show that a command-line prompt is needed.
static int fprint(FILE *stream, const char *format, ...)
{
    int n;
    va_list ap;
    va_start(ap, format);
    n = vfprintf(stream, format, ap);
    va_end(ap);
    prompt_again = 1;
    return n;
}

// Function: print_help
// Print usage information
static void print_help(void)
{
    fprint(stdout,
           "Help:\n"
           "      '?' to list online group members\n"
           "      '/member' to connect to 'member'\n"
           "      '/close member' to close a conversation with wrm\n"
           "      '/close' to close all conversations\n"
           // "      '/bell' to toggle the bell\n"
           "      '/rtt' to toggle printing of round trip times\n"
           "      '/status busy' sets the user status to 'busy'\n"
           "      '/help' for this information\n"
           "      '/exit' to exit group\n"
           );
    fprint(stdout,
           " Once corresponding, any text typed is sent to all correspondents.\n"
           " If corresponding with several users, prefix text with '@name '\n"
           " to send to just one\n");
}

// Function: user_arg
// Prompt the user for input, read, strip and return the response.
static char *user_arg(const char *message, char *line, size_t size)
{
    fprint(stdout, "%s : ", message);
    fflush(stdout);
    memset(line, 0, size);
    if ( fgets(line ,(int)size - 1,stdin) == NULL )
    {
        if ( feof(stdin) == 0 )
            perror("fgets");
        else
            fprint(stdout, "exit\n");
        EXIT_FLAG = 1;
        return("exit");
    }
    return(stgstrip(line, punch_whitespace));
}

// Function: get_options
// Get user and group name from the command line options or, if missing (and if interactive), read from the user.
int get_options(int argc,char **argv,char *user,char *group,char *NXTaddr)
{
    int c;
    while( argv != 0 && (c= getopt(argc, argv, "u:g:n:s:t:bp")) != EOF )
        switch (c)
    {
        case 'u': stgncpy(user,  optarg, 128); break;
        case 'g': stgncpy(group, optarg, 128); break;
        case 'n': stgncpy(NXTaddr, optarg, 128); break;
            //case 'p': pass_prompt = 1; break;
        //case 's': rvz_server = optarg; break;
        //case 't': rvz_port = (unsigned short) strtoul(optarg,0,0); break;
        case 'b': bell = 0; break;
        default:
            fprint(stderr,
                   "options: "
                   "[-u user] [-g group] [-n NXTaddr] "
                   "[-s server] [-t port] [-pb]\n"
                   "b=no bell, p=prompt for password\n");
            exit(1);
    }
    
    if ( isatty(fileno(stdin)) != 0 )
    {
        while ( user[0] == 0 )
            user_arg("user name", user, 128);
        while ( group[0] == 0 )
            user_arg("group name", group, 128);
        if ( NXTaddr[0] == 0 )
            user_arg("NXT address", NXTaddr, 128);
        if ( strlen(user) < 3 )
            strcat(user,"__");
        if ( strlen(group) < 3 )
            strcat(group,"__");
        if ( strlen(NXTaddr) < 3 )
            return(-1);
    }
    else
    {
        fprint(stderr, "missing credentials\n");
        return(-1);
    }
    return(0);
}

// Function: slog
// Log the supplied text with current time to the log file or stderr.
static void slog(const char *format, ...)
{
    static int log_fail;
    va_list ap;
    FILE *log = stderr;
    FILE *f = NULL;
    char buf[120];
    int n;
    time_t now = time(NULL);
    char *stamp = ctime(&now);
    va_start(ap, format);
    n = vsnprintf(buf, sizeof buf, format, ap);
    va_end(ap);
    if ( n >= (int)sizeof(buf) )
        n = (int)sizeof(buf) - 1;
    if ( log_file != 0 && log_fail == 0 )
    {
        if ( (f= fopen(log_file,"a+")) != NULL)
            log = f;
        else
        {
            log_fail = 1;
            perror(log_file);
        }
    }
    (void)fwrite(stamp, 24, 1, log);
    (void)fwrite(buf, n, 1, log);
    if ( f != 0 )
        fclose(f);
}

#endif
