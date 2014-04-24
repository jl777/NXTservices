/* Title: Client
 Latest version: http://basyl.co.uk/code/punch/punch-client.c.html */

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

#ifdef __linux__
#include <getopt.h>
#endif
#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "punch.h"

static const char *whitespace = " \t\n\r\v\f";

#define PING_INTERVAL           10          /* seconds */
#define CONNECTION_TIMEOUT     (120 * 1000) /* ms */

static int ping_interval = PING_INTERVAL;
static char *rvz_server = RVZ_HOST;
static unsigned short rvz_port = RVZ_PORT;

static int server_tcp;
static char user [NAME_SIZE + 1];
static char group[NAME_SIZE + 1];
static char email[EMAIL_SIZE + 1];
static char *pass = "";
static char connect_id[128];
static int corresponding;
static int bell = 1;
static int prompt_again = 1;
static int print_ping = 1;
int EXIT_FLAG;

/* Group member details. */
typedef struct member
{
    char name[NAME_SIZE + 1];
    char email[EMAIL_SIZE + 1];
    char status[STATUS_SIZE + 1];
    struct sockaddr_in addr;
    int fd;
    int have_address;
    
    struct timeval comm_time;
    struct timeval ping_time;
    long ping_nr;
    long ping_rtt;              /* return trip time of pings */
    
    struct member *next;
    struct member *prev;
    
} member_t;
struct member *member_list;


static int punch_cancel(member_t *pm);

/* Function: fprint
 
 Print like fprintf but also set global 'prompt_again' to show that a
 command-line prompt is needed.
 */
static int
fprint(FILE *stream, const char *format, ...)
{
    int n;
    va_list ap;
    va_start(ap, format);
    n = vfprintf(stream, format, ap);
    va_end(ap);
    prompt_again = 1;
    return n;
}


/* Function: correspond
 
 Convenience wrapper around sendto() for use with correspondent clients.
 */
static long
correspond(member_t *pm, const char *buf, size_t size)
{
    /*
     fprint(stdout, "sending '%s' to %s\n", buf, print_host_address(&pm->addr));
     */
    long status = sendto(pm->fd, buf, size, 0,
                        (struct sockaddr *) &pm->addr, sizeof pm->addr);
    if (status < 0) {
        perror("correspond: sendto");
        punch_cancel(pm);
    }
    return status;
}


/* Function: server_address
 
 Resolve the server address and complete an address structure.
 */
static int
server_address(const char *server, int port, struct sockaddr_in *addr)
{
    struct hostent *hp = gethostbyname(server);
    
    if (hp == NULL) {
        perror("gethostbyname");
        return -1;
    }
    
    addr->sin_port = htons(port);
    addr->sin_family = AF_INET;
    memcpy(&addr->sin_addr.s_addr, hp->h_addr, hp->h_length);
    return 0;
}


/* Function: server_connect_tcp
 
 Create a TCP socket and connect to the specified server TCP port.
 */
static int
server_connect_tcp(struct sockaddr_in *addr, socklen_t addr_len)
{
    int fd;
    
    if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        perror("socket");
    
    else if (connect(fd, (struct sockaddr *) addr, addr_len) < 0) {
        perror("connect");
        close(fd);
        fd = -1;
    }
    else fprint(stdout, "Server is at: %s\n", print_host_address(addr));
    
    return fd;
}


/*   Function: member_find
 
 Search the list for a member with the spcified name.
 */
static member_t *
member_find(const char *name)
{
    member_t *pm = member_list;
    
    while (pm) {
        if (!strcmp(pm->name, name))
            return pm;
        pm = pm->next;
    }
    return NULL;
}


/* Function: member_find_by_fd
 
 Search the list for a member with the spcified socket file descriptor.
 */
static member_t *
member_find_by_fd(int fd)
{
    member_t *pm = member_list;
    
    while (pm) {
        if ((pm->fd == fd) && (fd >= 0))
            return pm;
        pm = pm->next;
    }
    return NULL;
}


/* Function: member_create
 
 Create a new group member and link into the list
 */
static member_t *
member_create(const char *name, const char *mail, const char *status)
{
    member_t *pm = calloc(1, sizeof(member_t));
    if (!pm)
        perror("calloc");
    else {
        stgncpy(pm->name, name, sizeof pm->name);
        stgncpy(pm->email, mail, sizeof pm->email);
        stgncpy(pm->status, status, sizeof pm->status);
        pm->fd = -1;
        
        /* link to global list */
        pm->next = member_list;
        pm->prev = NULL;
        member_list = pm;
        if (pm->next)
            pm->next->prev = pm;
    }
    return pm;
}


/* Function: member_set_status
 
 Record the updated status reported by the server.
 */
static void
member_set_status(member_t *pm, const char *status)
{
    stgncpy(pm->status, status, sizeof pm->status);
}


/* Function: member_add
 
 Add a new group member if not already present.
 */
static void
member_add(char *s)
{
    member_t * pm;
    char *name   = stgsep(&s, " ");
    char *mail   = stgsep(&s, " ");
    char *status = stgsep(&s, "]");
    status += strspn(status, " ["); /* status points to space or '[' */
    
    pm = member_find(name);
    
    if (pm)
        member_set_status(pm, status);
    else
        member_create(name, mail, status);
}


/* Function: member_del
 
 Delete a member of the group and unlink from the global list
 */
static void
member_del(char *s)
{
    char *name = stgsep(&s, " ");
    
    member_t *pm = member_find(name);
    if (pm) {
        member_t *prev = pm->prev;
        member_t *next = pm->next;
        punch_cancel(pm);
        free(pm);
        if (prev)
            prev->next = next;
        else
            member_list = next;
        if (next)
            next->prev = prev;
    }
}


/* Function: list_members
 
 List all members of our group
 */
static void
members_list(void)
{
    member_t *pm = member_list;
    
    while (pm) {
        fprint(stdout, "\t%s %s [%s]\n", pm->name, pm->email, pm->status);
        pm = pm->next;
    }
}


/* Function: ping_all
 
 Send something to the all correspondent addresses.
 */
static void
ping_all(void)
{
    member_t *pm = member_list;
    
    while (pm) {
        if (pm->have_address) {
            char pingbuf[20] = "";
            int n = snprintf(pingbuf+1, sizeof pingbuf - 1, "%s %ld",
                             user, ++pm->ping_nr);
            
            gettimeofday(&pm->ping_time, NULL);
            correspond(pm, pingbuf, n+1);
        }
        pm = pm->next;
    }
}


/* Function: udp_io
 
 IO handler for UDP traffic.  Traffic is between clients through holes
 punched in firewall/NAT.  Traffic starts with a ping message.  Any
 other traffic is text to be displayed to the user.
 */
static void
udp_io(int fd)
{
    char buf[UDP_MESSAGE_MAX];
    long n;
    member_t *pm = member_find_by_fd(fd);
    
    if (!pm) {
        fprint(stderr, "Unknown UDP socket!\n");
       // exit(1);
        return;
    }
    
    memclear(buf);
    
    if ((n = read(fd, buf, sizeof buf - 1)) <= 0) {
        if (n < 0)
            perror("udp read");
        
        punch_cancel(pm);
    }
    else if (!buf[0]) {                        // a ping... 
        size_t userlen = strlen(user);
        fprint(stdout,"ping received: %s\n", buf+1); 
        if (strncmp(&buf[1], user, userlen)) { // ...from the other end 
            correspond(pm, buf, n);
            gettimeofday(&pm->comm_time, NULL);
        }
        else if (atoi(&buf[1+userlen]) == pm->ping_nr) {
            pm->ping_rtt = elapsed_ms(&pm->ping_time);
            if (print_ping)
                fprint(stdout,"Round trip [%s] %ldms\n", pm->name, pm->ping_rtt);
        }
    }
    else {
        const char *bellstr = bell ? "\a" : "";
        char cmd[1000];
        sprintf(cmd,"say -v Vicki %s %s",pm->name, buf);
        //system(cmd);
        fprint(stdout, "%s[%s] %s\n", bellstr, pm->name, buf);
        ping_interval = PING_INTERVAL;
        gettimeofday(&pm->comm_time, NULL);
    }
}



/* Function: punch
 
 Request a hole punch.  We create a UDP port for the connection and use this
 to send to the server the name and group of a correspondent to whom we will
 connect.  The server forwards a TCP punch request containing our external
 UDP port number to the correspondent, which uses this same function to
 punch the other way.  Once either end has the UDP address/port of the other
 it can start pinging that port.  See <Message Formats> for details (in
 Messages.txt).
 */
static int
punch(member_t *pm, const char *tag)
{
    struct sockaddr_in addr;
    char buf[128];
    
    int n = snprintf(buf, sizeof buf,
                     "%s %s/%s %s\n", connect_id, pm->name, group, tag);
    assert(n<(int) sizeof buf);
    
    (void) server_address(rvz_server, rvz_port, &addr);
    
    pm->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (pm->fd < 0)
        perror("UDP socket");
    
    else if (sendto(pm->fd, buf, n+1, 0,
                    (struct sockaddr *) &addr, sizeof addr) < 0) {
        perror("punch: sendto");
        punch_cancel(pm);
        return -1;
    }
    return 0;
}


/* Function: punch_cancel
 
 Close the socket associated with the punch request and clear the port to
 show that the connection is dead.
 */
static int
punch_cancel(member_t *pm)
{
    int correspondent_deleted = 0;
    if (pm->have_address) {
        
        if (sendto(pm->fd, "Bye\n", 5, 0,
                   (struct sockaddr *) &pm->addr, sizeof pm->addr) < 0)
            perror("punch_cancel: sendto");
        
        close(pm->fd);
        pm->fd = -1;
        --corresponding;
        pm->have_address = 0;
        correspondent_deleted = 1;
        fprint(stderr, "Connection to %s closed\n", pm->name);
    }
    return correspondent_deleted;
}


/* Function: get_correspondent_address
 
 Extract the correspondent name/group, IP address and UDP port number from
 the punch request.  See <TCP messages transmitted form server to client>
 for details (in Messages.txt). */
static member_t *
get_correspondent_address(char *request)
{
    member_t *pm = NULL;
    char *name = stgsep(&request, "/");
    char *grp  = stgsep(&request, " ");
    char *addr = stgsep(&request, ":");
    
    unsigned short port_nr =
    (unsigned short) strtoul(request, NULL, 0);
    
    if (!strcmp(grp, group) &&
        (port_nr != 0) &&
        ((pm = member_find(name)) != NULL)) {
        
        pm->addr.sin_port = htons(port_nr);
        pm->addr.sin_family = AF_INET;
        inet_aton(addr, &pm->addr.sin_addr);
        
        pm->have_address = 1;
        ++corresponding;
        fprint(stdout, "correspondent is %s/%s at %s/%d\n",
               name, grp, addr, port_nr);
    }
    else
        fprint(stdout, "correspondence failed or denied: %s/%s at %s/%d\n",
               name, grp, addr, port_nr);
    
    return pm;
}


/* Function: tcp_io
 
 Handler for traffic on the server TCP connection.
 - group member additions or deletions
 - punch requests
 */
static void
tcp_io(int tcp_socket)
{
    char buf[TCP_MESSAGE_MAX];
    char *line = buf;
    long n;
    
    memclear(buf);
    
    if ((n = read(tcp_socket, buf, sizeof buf - 1)) < 0) {
        perror("FATAL: tcp read");
        EXIT_FLAG = 1;
        return;
        while ( 1 ) sleep(1);
       // exit(1);
    }
    else if (!n) {
        fprint(stderr, "FATAL: server closed connection\n");
        EXIT_FLAG = 1;
        return;
        while ( 1 ) sleep(1);
        // exit(1);
    }
    
    while(line[0]) {            /* for each line of input */
        char *s = stgsep(&line, "\n\r");
        if (*s == '+')
            member_add(++s);
        
        else if (*s == '-')
            member_del(++s);
        
        else if (strchr("><!", *s)) {
            member_t *pm = get_correspondent_address(&s[1]);
            ping_interval = PING_INTERVAL;//1;
            
            if (!pm)            /* punch denied */
                fprint(stdout, "punch denied\n");
            
            else if (*s == '<')
                ping_all();
            
            else if (*s == '>') {
                punch(pm, PUNCH_CONFIRM);
                ping_all();
            }
        }
        else {
            fprint(stderr, "unknown tcp traffic : '%s'\n", s);
            return;
        }
    }
}


/* Function: prompt
 
 Print a command line prompt
 */
static void
prompt(void)
{
    if (!prompt_again)
        return;
    
    if (corresponding) {
        int n = 0;
        member_t *pm = member_list;
        fprintf(stdout, "[");
        while (pm) {
            if (pm->have_address) {
                char *s = n++ ? "," : "";
                
                fprintf(stdout, "%s%s", s, pm->name);
                ++n;
            }
            pm = pm->next;
        }
        fprintf(stdout, "] ");
    }
    
    fprintf(stdout, "%s/%s >> ", user, group);
    fflush(stdout);
    prompt_again = 0;
}


/* Function: print_help
 
 Print usage information
 */
static void
print_help(void)
{
    fprint(stdout,
           "Help:\n"
           "      '?' to list online group members\n"
           "      '/member' to connect to 'member'\n"
           "      '/close member' to close a conversation with wrm\n"
           "      '/close' to close all conversations\n"
           "      '/bell' to toggle the bell\n"
           "      '/rtt' to toggle printing of round trip times\n"
           "      '/status busy' sets the user status to 'busy'\n"
           "      '/help' for this information\n"
           "      '/exit' or ctrl-d to exit group\n"
           );
    fprint(stdout,
           " Once corresponding, any text typed is sent to all correspondents.\n"
           " If corresponding with several users, prefix text with '@name '\n"
           " to send to just one\n");
}


/* Function: close_all_connections
 
 Close all open connections.
 */
static void
close_all_connections(void)
{
    member_t *pm = member_list;
    fprint(stderr, "Closed: ");
    
    while (pm) {
        if (punch_cancel(pm))
            fprint(stderr, "'%s', ", pm->name);
        
        pm = pm->next;
    }
    fprint(stderr, "\n");
}


/* Function: close_connection
 
 Close connections named in the command.
 */
static void
close_connection(char *cmd)
{
    member_t *pm;
    stgsep(&cmd, " \t");
    
    if (!corresponding)
        fprint(stderr, "No correspondents\n");
    else if (!*cmd)
        close_all_connections();
    else
        while (*cmd) {
            char *name = stgsep(&cmd, ", \t");
            
            if ((pm = member_find(name)) != NULL) {
                if (!punch_cancel(pm))
                    fprint(stderr, "User '%s' not correspondent\n", name);
            }
        }
}


/* Function: set_status
 
 Report new status to he server.
 */
static void
send_status(char *cmd)
{
    char *s = stgstrip(cmd, whitespace);
    if (*s && is_printable(s)) {
        char buf[STATUS_SIZE + 1] = "=";
        stgncpy(&buf[1], s, sizeof buf-1);
        
        if (send(server_tcp, buf, strlen(buf)+1, 0) < 0)
            perror("send status");
    }
}



/* Function: timeout_connections
 
 Close any connections that have had no pings for 60 seconds.
 */
static void
timeout_connections(void)
{
    member_t *pm = member_list;
    
    while (pm) {
        if (pm->have_address &&
            ((elapsed_ms(&pm->ping_time) > CONNECTION_TIMEOUT) &&
             (elapsed_ms(&pm->comm_time) > CONNECTION_TIMEOUT))) {
                punch_cancel(pm);
                fprint(stderr, "Connection timed-out [%s]\n", pm->name);
            }
        pm = pm->next;
    }
}


/* Function: terminal_command
 
 Execute the user command. Commands are prefixed by '/' (stripped by
 <terminal_io>)
 */
static void
terminal_command(char *cmd)
{
    member_t *pm;
    
    if (!strcmp(cmd, "exit"))
    {
        EXIT_FLAG = 1;
        return;
    }
    else if (!strncmp(cmd, "close", 5))
        close_connection(cmd);
    
    else if (!strcmp(cmd, "rtt"))
    {
        print_ping ^= 1;
        printf("PING is %s\n",print_ping?"ON":"OFF");
    }
    
    else if (!strncmp(cmd, "status", 6))
        send_status(cmd + 6);
    
    else if (!strcmp(cmd, "bell")) {
        bell ^= 1;
        fprint(stdout, "Bell: %s\n", bell ? "on" : "off");
    }
    else if (!strcmp(cmd, user))
        fprint(stderr, "cannot punch a hole to yourself\n");
    
    else if ((pm = member_find(cmd)) != NULL)
        punch(pm, PUNCH_INITIATE);
    
    else
        print_help();
}


/* Function: send_one
 
 Send the supplied text to the specified (first word) correspondent client.
 */
static void
send_one(char *text)
{
    char *name = stgsep(&text, " \t:");
    member_t *pm = member_find(++name);
    
    if (!pm)
        fprint(stderr, "%s: not correspondent\n", name);
    
    else if (pm->have_address)
        correspond(pm, text, strlen(text) + 1);
}


/* Function: send_all
 
 Send the supplied text to all correspondent clients.
 */
static int
send_all(char *text)
{
    member_t *pm = member_list;
    int n = 0;
    size_t len = strlen(text) + 1;
    
    while (pm) {
        if (pm->have_address) {
            ++n;
            correspond(pm, text, len);
        }
        pm = pm->next;
    }
    if (!n)
        corresponding = 0;
    return n;
}



/* Function: terminal_io
 
 Handle activity on the command line.  Lines beginning with a '/' are
 commands.  A ? gets a list of members.  Text prefixed by @username is sent
 to that single correspondent user (if corresponding) otherwise all other
 text is sent to all correspondent clients.
 */
static void
terminal_io(void)
{
    char line[1024];
    long n;
    
    memclear(line);
    prompt_again = 1;
    
    if ((n = read(STDIN_FILENO, line, sizeof line - 1)) < 0) {
        perror("FATAL: read stdin");
        EXIT_FLAG = 1; return;
        //exit(1);
    }
    else if (n == 0) {          /* ctrl-d */
        //close_all_connections();
        fprint(stdout, "exit\n");
        fflush(stdin);
        EXIT_FLAG = 1; return;
       // exit(0);
    }
    
    n = strcspn(line, "\n\r");
    line[n] = '\0';
    
    if (!n)
        timeout_connections();
    
    else if ((n == 1) && (*line == '?'))
        members_list();
    
    else if (*line == '/')
        terminal_command(&line[1]);
    
    else if (corresponding > 0) {
        if (*line == '@')
            send_one(line);
        
        else send_all(line);
    }
    else
        print_help();
}


/* Function: get_fdset
 
 Fill an fdset for select() with the descriptors of all open socket and
 stdin.
 */
static int
get_fdset(int server_sock, fd_set *fdset)
{
    int max = 0;
    member_t *pm = member_list;
    
    FD_ZERO(fdset);
    FD_SET(server_sock, fdset);
    FD_SET(STDIN_FILENO, fdset);
    max = server_sock;
    
    while (pm) {
        if (pm->fd >= 0)
            FD_SET(pm->fd, fdset);
        max = maxfd(max, pm->fd);
        pm = pm->next;
    }
    return max;
}


/* Function: wait_for_input
 
 Wait for activity on the TCP server connection, on UDP connections to
 correspondent clients and on stdin.  Every minute ping correspondents to
 keep connections alive.
 */
static int
wait_for_input(int tcp)
{
    struct timeval timeout;
    fd_set fdset;
    int s;
    int n;
    int fd;
    int maxfd = get_fdset(tcp, &fdset);
    timeout.tv_sec  = corresponding ? ping_interval : 3600;
    timeout.tv_usec = 0;
    
    if ((s = select(maxfd+1, &fdset, NULL, NULL, &timeout)) < 0)
        perror("select");
    
    for (fd = 0, n = s; (n > 0) && (fd < FD_SETSIZE); ++fd) {
        
        if (!FD_ISSET(fd, &fdset))
            continue;
        --n;
        
        if (fd ==STDIN_FILENO)
            terminal_io();
        
        else if (fd == tcp)
            tcp_io(fd);
        
        else
            udp_io(fd);
    }
    if (!s)                     /* timeout */
        ping_all();
    return s;
}


/* Function: user_arg
 
 Prompt the user for input, read, strip and return the response.
 */
static char *
user_arg(const char *message, char *line, size_t size)
{
    fprint(stdout, "%s : ", message);
    fflush(stdout);
    memset(line, 0, size);
    
    if (fgets(line , (int)size - 1, stdin) == NULL) {
        if (!feof(stdin))
            perror("fgets");
        else
            fprint(stdout, "exit\n");
        EXIT_FLAG = 1;
        return("exit");
        //while ( 1 ) sleep(1);
        //exit(1);
    }
    return stgstrip(line, whitespace);
}


/* Function: get_options
 
 Get user and group name from the command line options or, if missing (and
 if interactive), read from the user.
 */
static char *
get_options(int argc, char ** argv, char *intro, size_t size)
{
    int c;
    int pass_prompt = 0;
    
    while(( c = getopt(argc, argv, "u:g:n:s:t:bp" )) != EOF )
        switch (c) {
            case 'u': stgncpy(user,  optarg, sizeof user); break;
            case 'g': stgncpy(group, optarg, sizeof group); break;
            case 'n': stgncpy(email, optarg, sizeof email); break;
            case 'p': pass_prompt = 1; break;
            case 's': rvz_server = optarg; break;
            case 't': rvz_port = (unsigned short) strtoul(optarg,0,0); break;
            case 'b': bell = 0; break;
            default:
                fprint(stderr,
                       "options: "
                       "[-u user] [-g group] [-n NXTaddr] "
                       "[-s server] [-t port] [-pb]\n"
                       "b=no bell, p=prompt for password\n");
                exit(1);
        }
    
    if (isatty(fileno(stdin))) {
        
        while (!user[0])
            user_arg("user name", user, sizeof user);
        
        while (!group[0])
            user_arg("group name", group, sizeof group);
        if ( strlen(group) < 3 )
            strcat(group,"__");
        if (pass_prompt)
            pass = getpass("password: ");
        
        if (!email[0])
            user_arg("NXT address", email, sizeof email);
        
        fprint(stdout, "Connecting as '%s' in group '%s'\n", user, group);
        
        c = snprintf(intro, size, "%s/%s/%s %s", user, group, pass, email);
        assert(c < (int) size);
    }
    else {
        fprint(stderr, "missing credentials\n");
        //exit(1);
        EXIT_FLAG = 1;
    }
    if (!is_printable(intro)) {
        fprint(stderr, "unprintable character[s] in credentials\n");
        EXIT_FLAG = 1;
      // exit(1);
    }
    
    return intro;
}



int punch_client_main(int argc, char ** argv)
{
    struct sockaddr_in addr;
    char ok[128];
    char intro[INTRO_SIZE + 1];
    
    get_options(argc, argv, intro, sizeof intro);

    memclear(connect_id);
    memclear(ok);
    
    if (server_address(rvz_server, rvz_port, &addr) < 0)
        fprint(stderr, "no address\n");
    else if ((server_tcp = server_connect_tcp(&addr, sizeof addr)) < 0)
        fprint(stderr, "no TCP connection\n");
    else if (read(server_tcp, connect_id, sizeof connect_id - 1) <= 0)
        perror("read connection id");
    else if (strncmp(connect_id, "id:", 3))
        fprint(stderr, "server rejected connection: %s\n", connect_id);
    
    else if (send(server_tcp, intro, strlen(intro)+1, 0) < 0)
        perror("send intro");
    else if (read(server_tcp, ok, sizeof ok - 1) <= 0)
        perror("read intro status");
    else if (strncmp(ok, "ok", 2))
        fprint(stderr, "server rejected user/group: %s\n", ok);
    else if (send(server_tcp, "?", 2, 0) < 0)
        perror("send list request");
    else {
        fprint(stdout, "Connection %s\n", connect_id);

        while ( EXIT_FLAG == 0 )
        {
            prompt();
            if ( wait_for_input(server_tcp) < 0 )
                break;
        }
    }
    return 1;
}

void *punch_client_glue(void *_argv)
{
    char **argv = _argv;
    int argc;
    for (argc=0; argv[argc]!=0; argc++);
    while ( 1 )
    {
        punch_client_main(argc,argv);
        printf("punch client finished\n");
        EXIT_FLAG = 0;
        memset(group,0,sizeof(group));
    }
    return(0);
}
