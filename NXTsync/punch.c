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



//#include "punch.h"
//extern unsigned short rvz_port;// = RVZ_PORT;
static int child_died = 0;

#define maxfd(a,b) ((a) > (b) ? (a) : (b))

//static client_t *client_find_by_name(const char *, const group_t *);
//static group_t *group_find(const char *);
//static group_t *group_create(const char *, const char *);


// Function: make_udp_socket
// Create the server UDP socket.  The server will watch for connections to
// this socket by clients that wish to punch a hole in another client's
// NAT/firewall.  See <punch>
static int make_udp_socket(int port)
{
    int fd;
    struct sockaddr_in addr;
    printf("make udp socket on port.%d\n",port);
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    if ( (fd= socket(AF_INET,SOCK_DGRAM,0)) < 0 )
        slog_error("UDP socket");
    else if ( bind(fd, (struct sockaddr *) &addr, (socklen_t) sizeof addr) < 0 )
        slog_error("UDP bind");
    else
        return(fd);
    printf("make_udp_socket close fd.%d\n",fd);
    if ( fd >= 0 )
        close(fd);
    return(-1);
}

// Function: make_tcp_socket
// Create the server TCP socket and bind it to the address/port on which the
// server will listen for new client connections.
static int make_tcp_socket(int port)
{
    int fd,opt = 1;
    struct sockaddr_in addr;
    printf("make tcp socket on port.%d\n",port);
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    if ( (fd= socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0 )
        slog_error("TCP socket");
    else if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt,(socklen_t) sizeof opt) < 0)
        slog_error("TCP set socket options");
    else if ( bind(fd, (struct sockaddr *) &addr, (socklen_t) sizeof addr) < 0 )
        slog_error("TCP bind");
    else if (listen(fd, 4) < 0)
        slog_error("TCP listen");
    else return fd;
    printf("make_tcp_socket close fd.%d\n",fd);
    if ( fd >= 0 )
        close(fd);
    return(-1);
}

// Function: print_member
// Print the name and NXTaddr of a group member.  The sign indicates (+ or -) whether the member joined or left the group.
static size_t print_member(char *buf, size_t size, client_t *pc, int sign)
{
    char ipaddr[32];
    int len = sprintf(buf, "%c%s %s [%s] %s/%d %s\n",sign,pc->user,pc->NXTaddr,pc->status,get_client_ipaddr(ipaddr,pc),get_client_port(pc),pc->pubkeystr);
    if ( len >= (int)size )
        printf("print_member: len.%d vs size.%ld\n",len,size);
    assert(len < (int) size);
    return (size_t) len;
}

// Function: notify_group_members
// Send a message to all members of the group that a client has joined or left.
static void notify_group_members(client_t *pc, int joined, int all)
{
    int32_t i;
    member_t *pm;
    client_t *cl = client_list;
    char buf[TCP_MESSAGE_MAX];
    size_t len = print_member(buf, sizeof buf, pc, joined ? '+' : '-');
    while ( cl != 0 )
    {
        if ( pc->group == cl->group )
        {
            if ( all != 0 || cl != pc )
            {
               // printf("send to sock.%d: (%s)\n",cl->sock,buf);
                (void)send(cl->sock,buf,len+1,0);
            }
        }
        cl = cl->next;
    }
    for (i=0; i<Num_punch_servers; i++)
    {
        if ( (pm= member_find_ipbits(Punch_servers[i])) != 0 && strcmp(pm->NXTaddr,Global_mp->NXTADDR) != 0 )
        {
            //printf("ping punchserver.%d NXT.%s %s\n",i,pm->NXTaddr,ipbits_str(Punch_servers[i]));
            //ping(pm,Global_mp->NXTADDR,find_Active_NXTaddr(calc_nxt64bits(pm->NXTaddr)));
        }
    }
}

// Function: list
// Handler for the 'list' request.  Allocates a buffer to hold a list of names
// and NXTaddr addresses for all members of the client group, prints the list
// and sends it to the requesting client.  Each entry in the list is a line of
// text - refer to <Message Formats> (in Messages.txt)
static long list(int sock, group_t *pg)
{
    long status = -1;
    size_t len = IDENTITY_SIZE * pg->n_members;
    char *buf = malloc(len+1);
    if ( buf == 0 )
        slog_error("malloc");
    else
    {
        client_t *pc = client_list;
        char *s = buf;
        *s = '\0';
        while ( pc != 0 )
        {
            if ( pc->group == pg && pc->user[0] != 0 )
                s += print_member(s,len - (s - buf),pc,'+');
            pc = pc->next;
        }
        //printf("SENDLIST (%s)\n",buf);
        status = send(sock,buf,s - buf + 1,0);  // jl777: make sure doesnt get too big
        free(buf);
        if ( status < 0 )
            slog_error("send list reply");
    }
    return(status);
}

// Function: intro
// Handle the client introductory message.  This message must be sent by the
// client after connecting to the server TCP port.  It contains the user and group names, the group password and NXTaddr address.
// The intro request can only be sent once.  User and group names must conform to the requirements of <name_ok>.
// If the group does not exist it is created.
static int intro(client_t *pc,char *buf)
{
    const char *msg = NULL;
    char pubkey[4096];
    char *text  = stgsep(&buf, " ");
    char *user  = stgsep(&text, "/");
    char *group = stgsep(&text, "/");
    char *pass  = text;
    char *NXTaddr = buf;
    int err;
    //printf("intro  NXT.(%s) user.(%s) group.(%s) pass.(%s)\n",NXTaddr,user,group,pass);
    if ( name_ok(user) == 0 || name_ok(group) == 0 )
    {
        msg = "bad user or group name\n";
        printf("(%s) (%s) %s\n",user,group,msg);
        slog(": bad user/group: %s/%s",user,group);
        return(-1);
    }
    else
    {
        group_t *pg = group_find(group);
        if ( pg == 0 )
            pg = group_create(group);//pass);
        if ( pg == 0 )
            msg = "unable to create group\n";
        //else if ( group_auth(pg,pass) < 0 )
        else if ( (err= validate_token(0,pubkey,pass,NXTaddr,user,3)) < 0 )
        {
            printf("err.%d\n",err);
            msg = "bad authentication token\n";
        }
        else if ( group_join(pg,user,NXTaddr) < 0 )
            msg = "group member with this name already exists\n";
        else
        {
            if ( pubkey[0] != 0 )
            {
                safecopy(pc->pubkeystr,pubkey,sizeof(pc->pubkeystr));
                decode_hex(pc->pubkey,sizeof(pc->pubkey),pubkey);
                {
                    char tmp[512];
                    init_hexbytes_noT(tmp,pc->pubkey,sizeof(pc->pubkey));
                    if ( strcmp(tmp,pubkey) != 0 )
                        printf("error codec'ing pubkey %s vs %s\n",tmp,pubkey);
                }
            }
            strcpy(pc->user, user);
            strcpy(pc->NXTaddr, NXTaddr);
            strcpy(pc->status, "online");
            pc->group = pg;
            msg = "ok\n";
        }
    }
    //printf("intro send.(%s)\n",msg);
    if ( send(pc->sock, msg, strlen(msg)+1, 0) < 0 )
    {
        slog_error("send intro reply");
        return(-1);
    }
    else notify_group_members(pc, 1, 0);
    return(0);  // even on failure; returning -1 kills connection
}

// Function: punch
// Forward a hole-punch request to a target.  The request either initiates, confirms or a denies the request.
// Note that the denial message forwarded to the supplicant omits the correspondent address.
static void servpunch(client_t *pc,client_t *target, struct sockaddr_in *addr, const char *tag,char *mypubkey)
{
    char buf[TCP_MESSAGE_MAX];
    int n;
    int initiate = !strncmp(tag, PUNCH_INITIATE, sizeof PUNCH_INITIATE);
    int confirm  = !strncmp(tag, PUNCH_CONFIRM, sizeof PUNCH_CONFIRM);
    int deny     = !strncmp(tag, PUNCH_DENY, sizeof PUNCH_DENY);
    memclear(buf);
    if ( initiate != 0 || confirm != 0 )
    {
        char ch = initiate ? '>' : '<';
        if ( mypubkey[0] != 0 )
            safecopy(pc->pubkeystr,mypubkey,sizeof(pc->pubkeystr));
       // printf(">>>>>>>>> %s pubkey.%s\n",pc->user,pc->pubkeystr);
        n = sprintf(buf, "%c%s/%s %s:%d %s",ch,pc->user,pc->group->name,inet_ntoa(addr->sin_addr),ntohs(addr->sin_port),pc->pubkeystr);
        punch_add_ipaddr(pc->NXTaddr,inet_ntoa(addr->sin_addr),ntohs(addr->sin_port),(struct sockaddr *)addr);
    }
    else if ( deny != 0 )
        n = sprintf(buf, "!%s/%s",pc->user,pc->group->name);
    else return;                 // ignore unrecognized request 
    assert(n < (int) sizeof buf);
    //printf("send target.(%s)\n",buf);
    if ( send(target->sock,buf,n+1,0) < 0 )
        slog_error("punch: send");
}

// Function:  receive_udp_punch
// Receive a request via the UDP server port from an unknown UDP client port to punch a hole in a target firewall.
// This handles requests from both supplicant and correspondent clients.
// The server does not track which client initiates a punch request and which responds;
// the clients themselves specify this in their messages. 
// The server forwards the request to the client supplying the client with the UDP address/port of the supplicant.
static void receive_udp_punch(int sock)
{
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof addr;
    char buf[UDP_MESSAGE_MAX];
    long n;
    memclear(buf);
    n = recvfrom(sock, buf,sizeof buf, 0, (struct sockaddr *) &addr, &addr_len);
    if ( n < 0 )
        slog_error("UDP recvfrom");
    else
    {
        char *s = buf;
        Global_mp->isudpserver = 1;
        char *id = stgsep(&s, " ");
        char *user = stgsep(&s, "/");
        char *group = stgsep(&s, " ");
        char *pubkey = stgsep(&s, " ");
        char *tag = stgsep(&s, "\n\r");
        group_t *pg = group_find(group);
        client_t *pc = client_find_by_id(id);
        client_t *target = client_find_by_name(user, pg);
        printf("UDP_PUNCH.(%s) (%s) (%s) (%s) tag.(%s) %p %p %p\n",user,id,group,pubkey,tag,pg,pc,target);
        if ( pc == 0 )
        {
            //close(sock);
            slog(": Unrecognized UDP client at %s (ID %s)\n",print_host_address(&addr), id);
        }
        else if ( pg != 0 && pc->group != 0 && target != 0 && target->user[0] != 0 )
        {
            //printf("callservpunch pubkey.(%s)\n",pubkey);
            servpunch(pc,target,&addr,tag,pubkey);
            //slog_addr("UDP client added at", &addr);
        }
    }
}

// Function: receive_tcp_connection
// Accept a connection from a client and create the necessary client
// structure.  The connection ID (to be used when punching a hole) is returned
// to the caller.  The client must introduce itself once the connection has been setup.
static int receive_tcp_connection(int sock)
{
    struct sockaddr_in addr;
    socklen_t addr_len = (socklen_t) sizeof addr;
    client_t *pc;
    int fd = accept(sock,(struct sockaddr *)&addr,&addr_len);
    if ( fd < 0 )
    {
        slog_error("accept");
        printf("error: receive_tcp_connection accept\n");
    }
    else if ( (pc= client_create(&addr,fd)) == NULL )
    {
        printf("error client_create close fd.%d\n",fd);
        close(fd);
        fd = -1;
    }
    else
    {
        //printf("SEND[%d] -> (%s)\n",fd,pc->id);
        if ( send(fd,pc->id,strlen(pc->id)+1,0) < 0 )
        {
            printf("Error sendind ID.(%s), close fd.%d\n",pc->id,fd);
            slog_error("send ID");
            close(fd);
            fd = -1;
        }
        else
        {
            printf("fd.%d add NXT.%s ipaddr.(%s %d) id.(%s)\n",fd,pc->NXTaddr,inet_ntoa(addr.sin_addr),ntohs(addr.sin_port),pc->id);
            Global_mp->istcpserver = 1;
            punch_add_ipaddr(pc->NXTaddr,inet_ntoa(addr.sin_addr),ntohs(addr.sin_port),(struct sockaddr *)&addr);
        }
    }
    return(fd);
}

// Function: close_connection
// Close the client connection that uses the given file descriptor.
static void servclose_connection(int fd)
{
    client_t *pc = client_find_by_socket(fd);
    assert( pc != NULL );
    if ( pc != 0 )
    {
        notify_group_members(pc,0,0);
        client_delete(pc);
    }
}

// Function: receive_tcp_traffic
// Receive traffic on the server's TCP port.  There are three types of traffic:
// - A refresh request asks the server to re-send the list of group members.
// - An termination message end the session.
// - The introductory (intro) message, which contains the client identity.
// Note that there is not much point using password-protected groups unless
// encryption is enabled on the server connections - see <Security>.
static long receive_tcp_traffic(int sock)
{
    long n,status = 0;
    char buf[TCP_MESSAGE_MAX] = "";
    client_t *pc;
    //memclear(buf);
    memset(buf,0,sizeof(buf));
    pc = client_find_by_socket(sock);
    if ( pc == 0 )
        status = -1;
    else if ( (n= read(sock,buf,sizeof(buf) - 1)) < 0 )
    {
        slog_error("read");
        status = -1;
    }
    else if ( n == 0 )            // end of file; connection closed 
        status = -1;
    else
    {
        long span = strcspn(buf, "\r\n"); // when connection is via telnet
        //printf("received.(%s)\n",buf);
        buf[span] = '\0';
        if ( buf[0] == '\0' )
            status = 0;         // ignore empty request (CRLF in telnet)
        else if ( buf[0] == '.' ) // exit 
            status = -1;
        else if ( buf[0] == '=' )
        {
            stgncpy(pc->status, &buf[1], sizeof pc->status);
            notify_group_members(pc, 1, 1);
        }
        else if ( buf[0] == '?' )
        {
            if ( pc->group != 0 )
                status = list(sock,pc->group);
            else
            {
                //printf("send introduce\n");
                status = send(sock,"introduce yourself\n",strlen("introduce yourself\n"),0);
            }
        }
        else if ( is_printable(buf) == 0 )
        {
            printf("illegal char in (%s)\n",buf);
            status = send(sock,"illegal character\n",strlen("illegal character\n"),0);
        }
        else if ( pc->user[0] != 0 )
        {
            printf("already introduced (%s)\n",pc->user);
            status = send(sock,"already introduced\n",strlen("already introduced\n"), 0);
        }
        else
        {
            buf[n] = '\0';
            status = intro(pc,buf);
            printf("intro status.%ld id.(%s)\n",status,pc->id);
        }
    }
    //printf("receive_tcp_traffic returns %ld\n",status);
    return(status);
}

// Function: get_fdset
// Fill an fdset for select() with the descriptors of all open sockets.
static int servget_fdset(int server_tcp,int server_udp,int udp_echo,fd_set *fdset)
{
    int max = 0;
    client_t *pc = client_list;
    FD_ZERO(fdset);
    if ( server_tcp >= 0 )
        FD_SET(server_tcp,fdset);
    if ( server_udp >= 0 )
        FD_SET(server_udp,fdset);
    if ( udp_echo >= 0 )
        FD_SET(udp_echo,fdset);
    max = maxfd(server_tcp,server_udp);
    max = maxfd(max,udp_echo);
    while ( pc != 0 )
    {
        if ( pc->sock >= 0 )
            FD_SET(pc->sock,fdset);
        max = maxfd(max,pc->sock);
        pc = pc->next;
    }
    return(max);
}

// Function: serve
// The main server function in which TCP and UDP ports are monitored and connections/traffic dispatched to handlers.
static int serve(int tcp,int udp,int udp_echo)
{
    struct timeval timeout;
    fd_set fdset;
    int fd,s,maxfd = servget_fdset(tcp,udp,udp_echo,&fdset);
    timeout.tv_sec  = 10;//Global_mp->corresponding ? PING_INTERVAL : 60;
    timeout.tv_usec = 0;
    if ( (s= select(maxfd+1,&fdset,NULL,NULL,&timeout)) < 0 )
        slog_error("select");
    for (fd=0; (s > 0)&&(fd < FD_SETSIZE); ++fd)
    {
        if ( FD_ISSET(fd,&fdset) == 0 )
            continue;
        --s;
        if ( fd == tcp )
            receive_tcp_connection(tcp);
        else if ( fd == udp )
            receive_udp_punch(udp);
        //else if ( fd == udp_echo )
         //   receive_udp_echo(udp_echo,0);
        else if ( receive_tcp_traffic(fd) < 0 )
        {
            printf("closing connection %d\n",fd);
            servclose_connection(fd);
        }
    }
    if ( s == 0 )
    {
       // receive_udp_echo(udp_echo,1);
        ping_all(Global_mp->NXTADDR,0,0);
    }
  //printf("serve returns s.%d\n",s);
    return(s);
}

// Function: run_server
// Run the server until it returns an error.
static void run_server(unsigned short port)
{
    int udp = make_udp_socket(port);
    int udp_echo = make_udp_socket(NXTSYNC_PORT);
    int tcp = make_tcp_socket(port);
    if ( udp >= 0 && tcp >= 0 )
        while ( serve(tcp, udp, udp_echo) >= 0 )
            ;
    printf("finished run_server\n");
}

// Function: get_options
// Get command line options.
static int get_servoptions(int argc, char ** argv)
{
    int c,foreground = 0;
    while( (c= getopt(argc,argv,"p:fl:g:")) != EOF )
    {
        switch (c)
        {
            //case 'p': rvz_port = (unsigned short) strtoul(optarg,0,0); break;
            case 'f': foreground = 1; break;
            case 'l': log_file = optarg; break;
            case 'g': group_dir = optarg; break;
            default:
                fprintf(stderr,"options: [-p port] [-l log-file] [-g group_dir] [-f]\n");
                exit(EXIT_FAILURE);
        }
    }
    return(foreground);
}

// Function: sigchld_handler
// Catch death of SIGCHLD
static void sigchld_handler(int a)
{
    (void) a;
    child_died = 1;
}

// Function: wait_child
// Suspend until child server dies.
static void wait_child(void)
{
    sigset_t mask, oldmask;
    int status;
    // Set mask to temporarily block SIGCHLD
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    // Wait for SIGCHLD.
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    while (!child_died)
        sigsuspend(&oldmask);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    wait(&status);
}


// Function: main
 
// The server gets command line options, and (unless the 'foreground' option
// is set) forks to background itself and and disconnects its controlling terminal.
 
// It then forks again to create a child server process where the work is
// done; the parent maintains the server, restarting it if ever it should die.
 
// The child server creates its public sockets and then loops indefinitely serving socket traffic.
int
//#ifdef __APPLE__
punch_server_main(int argc, char ** argv)
//#else
//main(int argc, char ** argv)
//#endif
{
    int foreground = get_servoptions(argc, argv);
    printf("Server starting... foreground.%d\n",foreground);
    if ( foreground != 0 )
        run_server(NXT_PUNCH_PORT);
    else
    {
        if ( fork() != 0 )
            exit(EXIT_SUCCESS);
        setsid();
        (void)signal(SIGCHLD,sigchld_handler);
        while ( 1 )//restarted < 1000 )
        {
            child_died = 0;
            if ( fork() != 0 )
            {
                wait_child();       // parent 
                slog_info("Server died.  Restarting in 5 seconds...");
                sleep(5);
            }
            else
            {
                run_server(NXT_PUNCH_PORT);
                break;
            }
        }
    }
    return EXIT_FAILURE;
}

void *punch_server_glue(void *_ptr)
{
    //char **args = _ptr;
    run_server(NXT_PUNCH_PORT);
    return(0);
}
