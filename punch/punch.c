/* Title: Server
 Latest version: http://basyl.co.uk/code/punch/punch.c.html */

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
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "punch.h"

static unsigned short rvz_port = RVZ_PORT;
static int child_died = 0;
static char *log_file = NULL;
static char *group_dir = NULL;
static time_t group_dir_read;

/* Group details. */
typedef struct group {
    char           name[NAME_SIZE+1];
    char           pass[NAME_SIZE+1];
    int            n_members;
    struct group  *next;
    struct group  *prev;
} group_t;
static struct group *group_list;


/* Client connection details. */
typedef struct client {
    struct sockaddr_in  addr;
    char                id[ID_SIZE+1];
    int                 sock;
    group_t            *group;
    char                user[NAME_SIZE+1];
    char                email[EMAIL_SIZE+1];
    char                status[STATUS_SIZE+1];
    struct client      *next;
    struct client      *prev;
    
} client_t;
static client_t *client_list;
static int n_clients;

#define maxfd(a,b) ((a) > (b) ? (a) : (b))

static client_t *client_find_by_name(const char *, const group_t *);
static group_t *group_find(const char *);
static group_t *group_create(const char *, const char *);



#define slog_error(s)  slog(": error in %s (%s): %s\n",       \
__func__, (s), strerror(errno))
#define slog_info(s)   slog(": %s\n", (s))
#define slog_addr(s,a) slog(": %s %s\n", (s), print_host_address(a))


/* Function: slog
 
 Log the supplied text with current time to the log file or stderr.
 */
static void
slog(const char *format, ...)
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
    
    if (n >= (int) sizeof buf)
        n = (int) sizeof buf - 1;
    
    if (log_file && !log_fail) {
        if ((f = fopen(log_file, "a+")) != NULL)
            log = f;
        else {
            log_fail = 1;
            perror(log_file);
        }
    }
    (void) fwrite(stamp, 24, 1, log);
    (void) fwrite(buf, n, 1, log);
    
    if (f)
        fclose(f);
}


/* Function: name_ok
 
 Return non-zero if the given string is valid for use as a name/group.
 Currently this means that it must contain only alphanumeric characters or
 the underscore, minus or plus symbols, and this it must be at least 3
 characters long.
 */
static int
name_ok(const char *s)
{
    const char *t = s;
    for (; *s; ++s)
        if (!isalnum(*s) && (*s != '_') && (*s != '-') && (*s != '+'))
            return 0;
    
    return (s - t >= 3);
}

/* Function: group_file_read
 
 Read the group password from a permanent group file.
 */
static char *
group_file_read(const char *filename)
{
    static char pw[NAME_SIZE+1];
    char path[128];
    char *s;
    FILE *f;
    
    int n = snprintf(path, sizeof path, "%s/%s", group_dir, filename);
    if (n >= (int) sizeof path) {
        slog("group file path too long: %s/%s\n", group_dir, filename);
        return NULL;
    }
    if ((f = fopen(path, "r")) == NULL) {
        slog_error(path);
        return NULL;
    }
    
    pw[0] = '\0';
    fgets(pw, sizeof pw, f);
    s = pw + strcspn(pw, " \n\r");
    *s = '\0';
    fclose(f);
    return pw;
}


/* Function: group_add_perm
 
 Add a permanent group
 */
static void
group_add_perm(const char *name)
{
    const char *pw = group_file_read(name);
    group_t *pg = NULL;
    
    if (pw)
        pg = group_create(name, pw);
    if (pg)
        pg->n_members = 1;
}


/* Function: group_dir_add
 
 Scan the group directory for new entries  - ie those that are not currently
 registered as groups.
 */
static void
group_dir_add(void)
{
    struct dirent *dp;
    DIR *dirp = opendir(group_dir);
    if (!dirp)
        slog_error(group_dir);
    else {
        group_dir_read = time(NULL);
        
        while ((dp = readdir(dirp)) != NULL)
            if ((dp->d_name[0] != '.') &&
                !group_find(dp->d_name))
                group_add_perm(dp->d_name);
        
        (void)closedir(dirp);
    }
}


/* Function: group_dir_changed
 
 Return non-zero if the group directory has changed.
 */
static int
group_dir_changed(void)
{
    struct stat st;
    
    if (!group_dir)
        return 0;
    
    else if (stat(group_dir, &st)) {
        slog_error(group_dir);
        return 0;
    }
    return st.st_mtime > group_dir_read;
}


/* Function: group_find
 
 Find the named group, ignoring characters beyond the end of the allowed
 name size.
 */
static group_t *
group_find(const char *group)
{
    group_t *pg;
    if (group_dir_changed())
        group_dir_add();        /* changes group_list */
    
    pg = group_list;
    while (pg) {
        if (!strncmp(pg->name, group, NAME_SIZE))
            return pg;
        
        pg = pg->next;
    }
    return NULL;
}


/* Function: group_create
 
 Create the named group.  Space is allocated for a group structure, the name
 and password are copied, and the struct is added to the linked list of
 groups.  The caller must ensure that the group does not already exist.
 */
static group_t *
group_create(const char *group, const char *pass)
{
    group_t *pg = calloc(1, sizeof(group_t));
    if (!pg)
        slog_error("calloc");
    else {
        strncpy(pg->name, group, NAME_SIZE);
        strncpy(pg->pass, pass, NAME_SIZE);
        
        /* link to global list */
        pg->next = group_list;
        pg->prev = NULL;
        group_list = pg;
        if (pg->next)
            pg->next->prev = pg;
        slog(": Group '%s' created\n", group);
        printf("Created group.%s\n",group);
    }
    return pg;
}


/* Function: group_auth
 
 Validate the supplied password against the group password.
 */
static int
group_auth(group_t *pg, const char *pass)
{
    printf("Password.(%s) vs (%s)\n",pg->pass,pass);
    return strcmp(pg->pass, pass) ? -1 : 0;
}


/* Function: group_join
 
 Check that a client with the same name is not a member of this group and
 increment the member count (needed to allow empty groups to be deleted).
 */
static int
group_join(group_t *pg, const char *user)
{
    if (!client_find_by_name(user, pg)) {
        pg->n_members++;
        return 0;
    }
    return -1;
}


/* Function: group_delete
 
 Delete the specified group, unlinking it from the linked list and freeing
 memory.
 */
static void
group_delete(group_t *pg)
{
    group_t *prev = pg->prev;
    group_t *next = pg->next;
    assert(pg->n_members == 0);
    
    slog(": Group '%s' deleted\n", pg->name);
    free(pg);
    if (prev)
        prev->next = next;
    else
        group_list = next;
    if (next)
        next->prev = prev;
}


/* Function: group_delete_member
 
 Reduce the member-count for the specified group and delete the group if no
 members remain.
 */
static void
group_delete_member(group_t *pg)
{
    if (pg && (--pg->n_members <= 0))
        group_delete(pg);
}


/* Function: create_connection_id
 
 Create an ID to be returned to the client upon successful connection.  The
 ID is used when a client wants to punch a hole.  See <punch>.
 
 Lame ID, but probably not easily spoofable.
 */
static const char *
create_connection_id(unsigned long count)
{
    int n;
    static char id[ID_SIZE+1];
    struct timeval tv;
    gettimeofday(&tv, NULL);
    n = snprintf(id, sizeof id, "id:%d-%ld", (int) tv.tv_usec, count);
    assert(n < (int) sizeof id);
    return id;
}


/* Function: client_create
 
 Create a new client structure, populate its non-zero members and link it
 into the global list.
 */
static client_t *
client_create(struct sockaddr_in *addr, int fd)
{
    static unsigned long count;
    
    client_t *pc = calloc(1, sizeof(client_t));
    if (!pc)
        slog_error("calloc");
    else {
        pc->addr     = *addr;
        pc->sock     = fd;
        strcpy(pc->id, create_connection_id(++count));
        
        /* link to global list */
        pc->next = client_list;
        pc->prev = NULL;
        client_list = pc;
        if (pc->next)
            pc->next->prev = pc;
        slog(": Client [%d] added at %s\n",
             ++n_clients, print_host_address(addr));
    }
    return pc;
}


/* Function: client_delete
 
 Close the specified client, freeing memory and unlinking from the global
 client list.
 */
static void
client_delete(client_t *pc)
{
    client_t *prev = pc->prev;
    client_t *next = pc->next;
    
    close(pc->sock);
    group_delete_member(pc->group);
    --n_clients;
    
    free(pc);
    if (prev)
        prev->next = next;
    else
        client_list = next;
    
    if (next)
        next->prev = prev;
}


/* Function: client_find_by_socket
 
 Find the client that uses the specified socket.
 */
static client_t *
client_find_by_socket(int sock)
{
    client_t *pc = client_list;
    while (pc) {
        
        if (pc->sock == sock)
            return pc;
        pc = pc->next;
    }
    return NULL;
}


/* Function: client_find_by_name
 
 Find the client that has the specified name and group.
 */
static client_t *
client_find_by_name(const char *user, const group_t *pg)
{
    client_t *pc = client_list;
    
    while (pc) {
        
        if (!strcmp(pc->user, user) && (pg == pc->group))
            return pc;
        
        pc = pc->next;
    }
    return NULL;
}


/* Function: client_find_by_id
 
 Find the client with the specified ID (see <create_connection_id>).
 */
static client_t *
client_find_by_id(const char *id)
{
    client_t *pc = client_list;
    
    while (pc) {
        
        if (!strcmp(pc->id, id))
            return pc;
        
        pc = pc->next;
    }
    return NULL;
}


/* Function: make_udp_socket
 
 Create the server UDP socket.  The server will watch for connections to
 this socket by clients that wish to punch a hole in another client's
 NAT/firewall.  See <punch>
 */
static int
make_udp_socket(int port)
{
    int fd;
    struct sockaddr_in addr;
    
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        slog_error("UDP socket");
    
    else if (bind(fd, (struct sockaddr *) &addr, (socklen_t) sizeof addr) < 0)
        slog_error("UDP bind");
    
    else
        return fd;
    
    if (fd >= 0)
        close(fd);
    
    return -1;
}


/* Function: make_tcp_socket
 
 Create the server TCP socket and bind it to the address/port on which the
 server will listen for new client connections.
 */
static int
make_tcp_socket(int port)
{
    int fd;
    int opt = 1;
    struct sockaddr_in addr;
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        slog_error("TCP socket");
    
    else if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt,
                        (socklen_t) sizeof opt) < 0)
        slog_error("TCP set socket options");
    
    else if (bind(fd, (struct sockaddr *) &addr, (socklen_t) sizeof addr) < 0)
        slog_error("TCP bind");
    
    else if (listen(fd, 4) < 0)
        slog_error("TCP listen");
    
    else
        return fd;
    
    if (fd >= 0)
        close(fd);
    
    return -1;
}


/* Function: print_member
 
 Print the name and email of a group member.  The sign indicates (+ or -)
 whether the member joined or left the group.
 */
static size_t
print_member(char *buf, size_t size, client_t *pc, int sign)
{
    int len = snprintf(buf, size, "%c%s %s [%s]\n",
                       sign,
                       pc->user,
                       pc->email,
                       pc->status);
    assert(len < (int) size);
    return (size_t) len;
}


/* Function: notify_group_members
 
 Send a message to all members of the group that a client has joined or
 left.
 */
static void
notify_group_members(client_t *pc, int joined, int all)
{
    client_t *cl = client_list;
    char buf[IDENTITY_SIZE];
    
    size_t len = print_member(buf, sizeof buf, pc, joined ? '+' : '-');
    
    while (cl) {
        
        if (pc->group == cl->group) {
            if (all || (cl != pc))
                (void) send(cl->sock, buf, len+1, 0);
        }
        cl = cl->next;
    }
}


/* Function: list
 
 Handler for the 'list' request.  Allocates a buffer to hold a list of names
 and email addresses for all members of the client group, prints the list
 and sends it to the requesting client.  Each entry in the list is a line of
 text - refer to <Message Formats> (in Messages.txt)
 */
static long
list(int sock, group_t *pg)
{
    long status = -1;
    size_t len = IDENTITY_SIZE * pg->n_members;
    char *buf = malloc(len+1);
    
    if (!buf)
        slog_error("malloc");
    else {
        client_t *pc = client_list;
        char *s = buf;
        *s = '\0';
        
        while (pc) {
            
            if ((pc->group == pg) && pc->user[0])
                s += print_member(s, len - (s - buf), pc, '+');
            
            pc = pc->next;
        }
        
        status = send(sock, buf, s - buf + 1, 0);
        free(buf);
        if (status < 0)
            slog_error("send list reply");
    }
    return status;
}


/* Function: intro
 
 Handle the client introductory message.  This message must be sent by the
 client after connecting to the server TCP port.  It contains the user and
 group names, the group password and email address.  The message is defined
 in <Message Formats> (in Messages.txt)
 
 The intro request can only be sent once.  User and group names must conform
 to the requirements of <name_ok>.  If the group does not exist it is
 created.
 */
static int
intro(client_t *pc, char *buf)
{
    const char *msg = NULL;
    char *text  = stgsep(&buf, " ");
    char *user  = stgsep(&text, "/");
    char *group = stgsep(&text, "/");
    char *pass  = text;
    char *email = buf;
    
    if (!name_ok(user) || !name_ok(group)) {
        msg = "bad user or group name\n";
        slog(": bad user/group: %s/%s %s", user, group);
    }
    else {
        group_t *pg = group_find(group);
        
        if (!pg)
            pg = group_create(group, pass);
        
        if (!pg)
            msg = "unable to create group\n";
        
        else if (group_auth(pg, pass) < 0)
            msg = "bad password\n";
        
        else if (group_join(pg, user) < 0)
            msg = "group member with this name already exists\n";
        
        else {
            strcpy(pc->user, user);
            strcpy(pc->email, email);
            strcpy(pc->status, "online");
            pc->group = pg;
            msg = "ok\n";
        }
    }
    
    if (send(pc->sock, msg, strlen(msg)+1, 0) < 0) {
        slog_error("send intro reply");
        return -1;
    }
    else notify_group_members(pc, 1, 0);
    
    return 0;  /* even on failure; returning -1 kills connection */
}


/* Function: punch
 
 Forward a hole-punch request to a target.  The request either initiates,
 confirms or a denies the request.  Refer to <Message Formats> (in
 Messages.txt).  Note that the denial message forwarded to the supplicant
 omits the correspondent address.
 */
static void punch(client_t *pc,client_t *target, struct sockaddr_in *addr, const char *tag)
{
    char buf[TCP_MESSAGE_MAX];
    int n;
    int initiate = !strncmp(tag, PUNCH_INITIATE, sizeof PUNCH_INITIATE);
    int confirm  = !strncmp(tag, PUNCH_CONFIRM, sizeof PUNCH_CONFIRM);
    int deny     = !strncmp(tag, PUNCH_DENY, sizeof PUNCH_DENY);
    memclear(buf);
    
    if (initiate || confirm) {
        char ch = initiate ? '>' : '<';
        n = snprintf(buf, sizeof buf, "%c%s/%s %s:%d",
                     ch,
                     pc->user,
                     pc->group->name,
                     inet_ntoa(addr->sin_addr),
                     ntohs(addr->sin_port));
    }
    else if (deny)
        n = snprintf(buf, sizeof buf, "!%s/%s",
                     pc->user,
                     pc->group->name);
    else
        return;                 /* ignore unrecognized request */
    
    assert(n < (int) sizeof buf);
    
    if (send(target->sock, buf, n+1, 0) < 0)
        slog_error("punch: send");
}


/* Function:  receive_udp_punch
 
 Receive a request via the UDP server port from an unknown UDP client port
 to punch a hole in a target firewall.  The form of the message is defined
 in <Message Formats> (in Messages.txt)
 
 This handles requests from both supplicant and correspondent clients.  The
 server does not track which client initiates a punch request and which
 responds; the clients themselves specifiy this in their messages.
 
 The server forwards the request to the client supplying the client with the
 UDP address/port of the supplicant (refer to <punch>).
 */
static void
receive_udp_punch(int sock)
{
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof addr;
    char buf[UDP_MESSAGE_MAX];
    long n;
    memclear(buf);
    n = recvfrom(sock, buf,sizeof buf, 0, (struct sockaddr *) &addr, &addr_len);
    if (n < 0)
        slog_error("UDP recvfrom");
    else {
        char *s     = buf;
        char *id    = stgsep(&s, " ");
        char *user  = stgsep(&s, "/");
        char *group = stgsep(&s, " ");
        char *tag   = stgsep(&s, "\n\r");
        
        group_t  *pg = group_find(group);
        client_t *pc = client_find_by_id(id);
        client_t *target = client_find_by_name(user, pg);
        
        if (!pc) {
            close(sock);
            slog(": Unrecognized UDP client at %s (ID %s)\n",
                 print_host_address(&addr), id);
        }
        else if (pg && pc->group && target && target->user[0]) {
            punch(pc, target, &addr, tag);
            slog_addr("UDP client added at", &addr);
        }
    }
}


/* Function: receive_tcp_connection
 
 Accept a connection from a client and create the necessary client
 structure.  The connection ID (to be used when punching a hole) is returned
 to the caller.  The client must introduce itself once the connection has
 been setup.
 */
static int
receive_tcp_connection(int sock)
{
    struct sockaddr_in addr;
    socklen_t addr_len = (socklen_t) sizeof addr;
    client_t *pc;
    
    int fd = accept(sock, (struct sockaddr *) &addr, &addr_len);
    
    if (fd < 0)
        slog_error("accept");
    
    else if ((pc = client_create(&addr, fd)) == NULL) {
        close(fd);
        fd = -1;
    }
    else if (send(fd, pc->id, strlen(pc->id)+1, 0) < 0) {
        slog_error("send ID");
        close(fd);
        fd = -1;
    }
    return fd;
}


/* Function: close_connection
 
 Close the client connection that uses the given file descriptor.
 */
static void
close_connection(int fd)
{
    client_t *pc = client_find_by_socket(fd);
    assert(pc != NULL);
    if (pc) {
        notify_group_members(pc, 0, 0);
        client_delete(pc);
    }
}


/* Function: receive_tcp_traffic
 
 Receive traffic on the server's TCP port.  There are three types of
 traffic:
 
 - A refresh request asks the server to re-send the list of group members.
 - An termination message end the session.
 - The introductory (intro) message, which contains the client identity.
 
 Note that there is not much point using password-protected groups unless
 encryption is enabled on the server connections - see <Security>.
 */
static long
receive_tcp_traffic(int sock)
{
    long status = 0;
    char buf[TCP_MESSAGE_MAX] = "";
    long n;
    client_t *pc;
    
    memclear(buf);
    
    pc = client_find_by_socket(sock);
    if (!pc)
        status = -1;
    
    else if ((n = read(sock, buf, sizeof buf - 1)) < 0) {
        slog_error("read");
        status = -1;
    }
    else if (n == 0)            /* end of file; connection closed */
        status = -1;
    
    else {
        long span = strcspn(buf, "\r\n"); /* when connection is via telnet */
        buf[span] = '\0';
        
        if (buf[0] == '\0')
            status = 0;         /* ignore empty request (CRLF in telnet)*/
        
        else if (buf[0] == '.') /* exit */
            status = -1;
        
        else if (buf[0] == '=') {
            stgncpy(pc->status, &buf[1], sizeof pc->status);
            notify_group_members(pc, 1, 1);
        }
        else if (buf[0] == '?') {
            if (pc->group)
                status = list(sock, pc->group);
            else
                status = send(sock, "introduce yourself\n", 20, 0);
        }
        else if (!is_printable(buf))
            status = send(sock, "illegal character\n", 19, 0);
        
        else if (pc->user[0])
            status = send(sock, "already introduced\n", 20, 0);
        
        else {
            buf[n] = '\0';
            status = intro(pc, buf);
        }
    }
    return status;
}


/* Function: get_fdset
 
 Fill an fdset for select() with the descriptors of all open sockets.
 */
static int
get_fdset(int server_tcp, int server_udp, fd_set *fdset)
{
    int max = 0;
    client_t *pc = client_list;
    
    FD_ZERO(fdset);
    FD_SET(server_tcp, fdset);
    FD_SET(server_udp, fdset);
    max = maxfd(server_tcp, server_udp);
    
    while (pc) {
        FD_SET(pc->sock, fdset);
        max = maxfd(max, pc->sock);
        pc = pc->next;
    }
    return max;
}


/* Function: serve
 
 The main server function in which TCP and UDP ports are monitored and
 connections/traffic dispatched to handlers.
 */
static int
serve(int tcp, int udp)
{
    fd_set fdset;
    int fd;
    int s;
    int maxfd = get_fdset(tcp, udp, &fdset);
    
    if ((s = select(maxfd+1, &fdset, NULL, NULL, NULL)) < 0) {
        slog_error("select");
    }
    
    for (fd = 0; (s > 0) && (fd < FD_SETSIZE); ++fd) {
        
        if (!FD_ISSET(fd, &fdset))
            continue;
        --s;
        if (fd == tcp)
            receive_tcp_connection(tcp);
        
        else if (fd == udp)
            receive_udp_punch(udp);
        
        else if (receive_tcp_traffic(fd) < 0)
            close_connection(fd);
    }
    return s;
}


/* Function: run_server
 
 Run the server until it returns an error.
 */
static void
run_server(unsigned short port)
{
    int udp = make_udp_socket(port);
    int tcp = make_tcp_socket(port);
    
    if ((udp >= 0) &&
        (tcp >= 0))
        while (serve(tcp, udp) >= 0)
            ;
}

/* Function: get_options
 
 Get command line options.
 */
static int
get_options(int argc, char ** argv)
{
    int c;
    int foreground = 0;
    
    while(( c = getopt(argc, argv, "p:fl:g:" )) != EOF )
        switch (c) {
            case 'p': rvz_port = (unsigned short) strtoul(optarg,0,0); break;
            case 'f': foreground = 1; break;
            case 'l': log_file = optarg; break;
            case 'g': group_dir = optarg; break;
            default:
                fprintf(stderr,
                        "options: "
                        "[-p port] [-l log-file] [-g group_dir] [-f]\n");
                exit(EXIT_FAILURE);
        }
    return foreground;
}


/* Function: sigchld_handler
 
 Catch death of SIGCHLD
 */
static void
sigchld_handler(int a)
{
    (void) a;
    child_died = 1;
}


/* Function: wait_child
 
 Suspend until child server dies.
 */
static void
wait_child(void)
{
    sigset_t mask, oldmask;
    int status;
    
    /* Set mask to temporarily block SIGCHLD. */
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    
    /* Wait for SIGCHLD. */
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    while (!child_died)
        sigsuspend(&oldmask);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    
    wait(&status);
}


/* Function: main
 
 The server gets command line options, and (unless the 'foreground' option
 is set) forks to background itself and and disconnects its controlling
 terminal.
 
 It then forks again to create a child server process where the work is
 done; the parent maintains the server, restarting it if ever it should die.
 
 The child server creates its public sockets and then loops indefinitely
 serving socket traffic.
 */
int
#ifdef __APPLE__
punchmain(int argc, char ** argv)
#else
main(int argc, char ** argv)
#endif
{
    int foreground = get_options(argc, argv);
    long restarted = 0;
    
    slog_info("Server starting...");
    
    if (foreground)
        run_server(rvz_port);
    
    else {
        if (fork())
            exit(EXIT_SUCCESS);
        setsid();
        (void) signal(SIGCHLD, sigchld_handler);
        
        while (restarted < 1000) {
            child_died = 0;
            
            if (fork()) {
                wait_child();       /* parent */
                slog_info("Server died.  Restarting in 5 seconds...");
                sleep(5);
            }
            else {
                run_server(rvz_port);
                break;
            }
        }
    }
    return EXIT_FAILURE;
}
