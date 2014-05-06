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
#ifndef punchclient
#define punchclient


//#include "punch.h"


// Function: get_correspondent_address
// Extract the correspondent name/group, IP address and UDP port number from the punch request.
static member_t *get_correspondent_address(char *request)
{
    uint32_t ipbits;
    uint64_t nxt64bits;
    char NXTaddr[64];
    member_t *pm = NULL;
    //printf("REQUEST.(%s)\n",request);
    char *name = stgsep(&request, "/");
    char *grp  = stgsep(&request, " ");
    char *ipaddr = stgsep(&request, ":");
    char *portstr  = stgsep(&request," ");
    char *pubkey  = stgsep(&request, "\n");
    uint16_t port_nr = (unsigned short) strtoul(portstr,NULL,0);
    printf("grp.(%s) port.(%s) %d name.(%s)\n",grp,portstr,port_nr,name);
    if ( strcmp(grp,Global_mp->groupname) == 0 && port_nr != 0 )
    {
        ipbits = calc_ipbits(ipaddr);
        if ( (pm= member_find(name)) == NULL )
        {
            nxt64bits = _match_ipaddr_nxt64bits(ipbits,&port_nr);
            if ( nxt64bits != 0 )
            {
                expand_nxt64bits(NXTaddr,nxt64bits);
                pm = member_create(grp,name,NXTaddr,"online");
                printf("late binding (%s) NXT.%s %s/%d\n",name,NXTaddr,ipaddr,port_nr);
            } 
        }
        if ( pm != 0 )
        {
            pm->port = port_nr;
            pm->ipbits = ipbits;
            pm->addr.sin_port = htons(port_nr);
            pm->addr.sin_family = AF_INET;
            //inet_aton(ipaddr,&pm->addr.sin_addr);
            *(uint32_t *)&pm->addr.sin_addr = ipbits;//inet_addr(ipaddr);
            if ( strcmp(ipaddr,inet_ntoa(pm->addr.sin_addr)) != 0 )
                printf("problem with sin_addr %s != %s\n",ipaddr,inet_ntoa(pm->addr.sin_addr));
            pm->have_address = 1;
            if ( pubkey[0] != 0 )
            {
                decode_hex(pm->pubkey,sizeof(pm->pubkey),pubkey);
                safecopy(pm->pubkeystr,pubkey,sizeof(pm->pubkeystr));
            }
            ++Global_mp->corresponding;
            punch_add_ipaddr(pm->NXTaddr,ipaddr,port_nr,(struct sockaddr *)&pm->addr);
            copy_member_data(pm);
            // fprint(stdout, "fd.%d correspondent is %s/%s at %s/%d %s\n",pm->fd,name,grp,addr,port_nr,pubkey);
            return(pm);
        }
    }
    fprint(stdout, "correspondence failed or denied: %s/%s at %s/%d\n",name,grp,ipaddr,port_nr,pubkey);
    return(0);
}

// Function: tcp_io
// Handler for traffic on the server TCP connection.
// - group member additions or deletions
// - punch requests
static void tcp_io(int tcp_socket,char *servername,char *connect_id)
{
    char buf[TCP_MESSAGE_MAX];
    char *line = buf;
    long n;
    if ( tcp_socket < 0 )
        return;
    memclear(buf);
    if ( (n= read(tcp_socket,buf,sizeof(buf) - 1)) < 0 )
    {
        perror("FATAL: tcp read");
        EXIT_FLAG = 1;
        return;
       // exit(1);
    }
    else if ( n == 0 )
    {
        fprint(stderr, "server closed connection\n");
        EXIT_FLAG = 1;
        return;
    }
    //printf("TCP.(%s)\n",line);
    while( line[0] != 0 ) // for each line of input
    {            
        char *s = stgsep(&line, "\n\r");
        if ( *s == '+' )
            member_add(Global_mp->groupname,++s);
        else if ( *s == '-' )
            member_del(++s);
        else if ( strchr("><!", *s) != 0 )
        {
            member_t *pm = get_correspondent_address(&s[1]);
            if ( pm == 0 )            // punch denied
                fprint(stdout, "punch denied\n");
            else if ( *s == '<' )
                ping_all(Global_mp->NXTADDR,servername,connect_id);
            else if ( *s == '>')
            {
                punch(pm,PUNCH_CONFIRM,servername,connect_id);
                ping_all(Global_mp->NXTADDR,0,0);
            }
        }
        else
        {
            fprint(stderr, "unknown tcp traffic : '%s'\n", s);
            return;
        }
    }
}

// Function: close_connection
// Close connections named in the command.
static void close_connection(char *cmd)
{
    member_t *pm;
    stgsep(&cmd, " \t");
    if ( Global_mp->corresponding == 0 )
        fprint(stderr, "No correspondents\n");
    else if ( *cmd == 0 )
        close_all_connections();
    else
        while ( *cmd != 0 )
        {
            char *name = stgsep(&cmd, ", \t");
            if ( (pm= member_find(name)) != NULL )
            {
                if ( punch_cancel(pm) == 0 )
                    fprint(stderr, "User '%s' not correspondent\n", name);
            }
        }
}

// Function: set_status
// Report new status to he server.
static void send_status(int server_tcp,char *cmd)
{
    char *s = stgstrip(cmd,punch_whitespace);
    if ( *s != 0 && is_printable(s) != 0 )
    {
        char buf[STATUS_SIZE + 1] = "=";
        stgncpy(&buf[1],s,sizeof buf-1);
        if ( send(server_tcp,buf,strlen(buf)+1,0) < 0 )
            perror("send status");
    }
}

// Function: terminal_command
// Execute the user command. Commands are prefixed by '/' (stripped by  <terminal_io>)
static void terminal_command(int server_tcp,char *cmd,char *servername,char *connect_id)
{
    member_t *pm;
    printf("cmd.(%s)\n",cmd);
    if ( strcmp(cmd, "exit") == 0 )
    {
        close_all_connections();
        memset(Global_mp->groupname,0,sizeof(Global_mp->groupname));
        EXIT_FLAG = 1;
        printf("exit detected\n");
        return;
    }
    else if ( strncmp(cmd,"close",5) == 0 )
    {
        printf("close connection\n");
        close_connection(cmd);
    }
    else if ( strcmp(cmd,"rtt") == 0 )
    {
        print_ping ^= 1;
        printf("PING is %s\n",print_ping?"ON":"OFF");
    }
    else if ( strncmp(cmd,"status",6) == 0 )
    {
        printf("status\n");
        send_status(server_tcp,cmd + 6);
    }
    else if ( strcmp(cmd,"bell") == 0 )
    {
        bell ^= 1;
        fprint(stdout, "Bell: %s\n", bell ? "on" : "off");
    }
    else if ( strcmp(cmd,Global_mp->dispname) == 0 )
        fprint(stderr, "cannot punch a hole to yourself\n");
    else if ( (pm= member_find(cmd)) != NULL )
    {
        printf("got user request for punch\n");
        punch(pm,PUNCH_INITIATE,servername,connect_id);
    }
    else print_help();
}

// Function: terminal_io
// Handle activity on the command line.  Lines beginning with a '/' are
// commands.  A ? gets a list of members.  Text prefixed by @username is sent
// to that single correspondent user (if corresponding) otherwise all other
// text is sent to all correspondent clients.
static void terminal_io(int server_tcp,char *servername,char *connect_id)
{
    static char prevline[1024];
    char line[1024];
    long n;
    memclear(line);
    prompt_again = 1;
    if ( (n= read(STDIN_FILENO, line, sizeof line - 1)) < 0 )
    {
        perror("FATAL: read stdin");
        memset(Global_mp->groupname,0,sizeof(Global_mp->groupname));
        EXIT_FLAG = 1;
        return;
    }
    else if ( n == 0 ) // ctrl-d 
    {          
        close_all_connections();
        fprint(stdout, "exit\n");
        fflush(stdin);
        memset(Global_mp->groupname,0,sizeof(Global_mp->groupname));
        EXIT_FLAG = 1;
        return;
    }
    n = strcspn(line, "\n\r");
    line[n] = '\0';
    if ( (line[0] & 0xff) == 0xef )
        strcpy(line,prevline);
    else strcpy(prevline,line);
    //printf("line.(%s) %x\n",line,line[0]);
    if ( n == 0 )
        timeout_connections();
    else if ( n == 1 && *line == '?' )
        members_list();
    else if ( *line == '/' )
    {
        if ( server_tcp >= 0 )
            terminal_command(server_tcp,&line[1],servername,connect_id);
        else printf("no Punch server available\n");
    }
    else if ( Global_mp->corresponding > 0 )
    {
        if ( *line == '@' )
            send_one(line);
        else send_all(line);
    }
    else print_help();
}

// Function: get_fdset
// Fill an fdset for select() with the descriptors of all open socket and stdin.
static int get_fdset(int server_sock,fd_set *fdset)
{
    int max = -1;
    member_t *pm = member_list;
    FD_ZERO(fdset);
    if ( server_sock >= 0 )
    {
        FD_SET(server_sock,fdset);
        max = server_sock;
    }
    FD_SET(STDIN_FILENO,fdset);
    while ( pm != 0 )
    {
        if ( pm->fd >= 0 )
        {
            FD_SET(pm->fd,fdset);
            max = maxfd(max,pm->fd);
        }
        pm = pm->next;
    }
    return(max);
}

// Function: wait_for_input
// Wait for activity on the TCP server connection, on UDP connections to
// correspondent clients and on stdin.  Every minute ping correspondents to keep connections alive.
static int wait_for_input(int32_t server_tcp,char *servername,char *connect_id)
{
    int fastmicros = 100;
    static uint32_t counter;
    struct timeval timeout;
    fd_set fdset;
    int i,s,n,fd,maxfd = get_fdset(server_tcp,&fdset);
    counter++;
    timeout.tv_sec  = Global_mp->corresponding ? 0 : PING_INTERVAL;
    timeout.tv_usec  = Global_mp->corresponding ? fastmicros : 0;
    if ( (s= select(maxfd+1,&fdset,NULL,NULL,&timeout)) < 0 )
        return(0);//perror("select");
    for (fd=0,n=s; (n > 0)&&(fd < FD_SETSIZE); ++fd)
    {
       // process_syncmem_queue(server_tcp,servername,connect_id);
        if ( FD_ISSET(fd,&fdset) == 0 )
            continue;
        --n;
        if ( fd == STDIN_FILENO )
            terminal_io(server_tcp,servername,connect_id);
        else if ( fd == server_tcp )
            tcp_io(server_tcp,servername,connect_id);
        else client_udp_io(fd);
    }
    for (i=0; i<1;i++)
        if ( process_syncmem_queue(server_tcp,servername,connect_id) <= 0 )
            break;
    if ( s == 0  )
    {
        if ( (Global_mp->corresponding == 0 && (counter % 10) == 0) || (Global_mp->corresponding != 0 && (counter % ((1000000/fastmicros)*10)) == 0) )
        {
            //printf("ping_all ");
            ping_all(Global_mp->NXTADDR,servername,connect_id);
        }
    }
    return(s);
}

int set_intro(char *intro,int size,char *user,char *group,char *NXTaddr)
{
    int c;
    char jsonstr[4096];
    gen_tokenjson(jsonstr,user,NXTaddr,time(NULL),0);
    //fprint(stdout, "Connecting as '%s' in group '%s' secret.(%s)\n", user, group,Global_mp->NXTACCTSECRET);
    c = sprintf(intro, "%s/%s/%s %s", user, group, jsonstr, NXTaddr);
    assert(c < (int) size);
    if ( is_printable(intro) == 0 )
    {
        fprint(stderr, "unprintable character[s] in credentials\n");
        return(-1);
    }
    return(0);
}

int contact_punch_server(char *servername,char *connect_id,int max,char *intro)
{
    int server_tcp;
    struct sockaddr_in addr;
    char ok[128];
    //memclear(connect_id);
    //memclear(ok);
    memset(ok,0,sizeof(ok));
    if ( server_address(servername,NXT_PUNCH_PORT,&addr) < 0 )
        fprint(stderr, "no address\n");
    else if ( (server_tcp= server_connect_tcp(&addr,sizeof addr)) < 0 )
    {
        fprint(stderr, "no TCP connection to %s\n",servername);
    }
    else
    {
        if ( read(server_tcp,connect_id,max - 1) <= 0 )
            perror("read connection id");
        else if ( strncmp(connect_id, "id:", 3) )
            fprint(stderr, "server rejected connection: %s\n", connect_id);
        else
        {
            printf("recv.(%s)\n",connect_id);
            if ( send(server_tcp,intro,strlen(intro)+1,0) < 0 )
                perror("send intro");
            else if ( read(server_tcp, ok, sizeof ok - 1) <= 0 )
                perror("read intro status");
            else if ( strncmp(ok, "ok", 2) )
                fprint(stderr, "server rejected user/group: %s\n", ok);
            else if ( send(server_tcp, "?", 2, 0) < 0 )
                perror("send list request");
            else
            {
                fprint(stdout, "Connection %s\n", connect_id);
                add_punch_server(servername);
                return(server_tcp);
            }
        }
    }
    return(-1);
}

void *run_NXTsync(void *_servername)
{
    char *servername = _servername;
    int server_tcp;
    char intro[INTRO_SIZE + 1],dispname[128],groupname[128],NXTaddr[128],connect_id[512];
    //int32_t NXTsync_dispatch(void **ptrp,void *ignore);
    //init_pingpong_queue(&NXTsync_received,"NXTsync_received",NXTsync_dispatch,0,0);
    printf("init_NXTsync %s %s %s\n",Global_mp->dispname,Global_mp->NXTADDR,Global_mp->groupname);
    strcpy(dispname,Global_mp->dispname);
    strcpy(groupname,Global_mp->groupname);
    strcpy(NXTaddr,Global_mp->NXTADDR);
    if ( get_options(0,0,dispname,groupname,NXTaddr) < 0 )
    {
        printf("init_NXTsync: Invalid options, try again\n");
        return(0);
    }
    if ( Global_mp->groupname[0] == 0 && groupname[0] != 0 )
        safecopy(Global_mp->groupname,groupname,sizeof(Global_mp->groupname));
    while ( 1 )
    {
        if ( set_intro(intro,sizeof(intro),Global_mp->dispname,Global_mp->groupname,Global_mp->NXTADDR) < 0 )
        {
            printf("init_NXTsync: invalid intro.(%s), try again\n",intro);
            sleep(30);
            continue;
        }
        printf("got connect_id.%s intro.%s\n",connect_id,intro);
        if ( (server_tcp= contact_punch_server(servername,connect_id,sizeof(connect_id),intro)) < 0 )
        {
            printf("init_NXTsync: error contacting (%s), try again\n",servername);
            sleep(30);
            continue;
        }
        strcpy(Global_mp->connect_id,connect_id);
        strcpy(Global_mp->servername,servername);
        Global_mp->server_tcp = server_tcp;
        printf("Start NXTsync\n");
        EXIT_FLAG = 0;
        while ( EXIT_FLAG == 0 )
        {
            prompt(Global_mp->dispname,Global_mp->groupname);
            if ( wait_for_input(server_tcp,servername,connect_id) < 0 )
                break;
        }
        close(server_tcp), server_tcp = -1;
        printf("NXTsync %s finished\n",servername);
        sleep(10);
    }
    return(0);
}

int punch_client_main(int argc, char ** argv)
{
    uint16_t port;
    int i,server_tcp;
    char connect_id[512],intro[INTRO_SIZE + 1],servername[32],dispname[128],groupname[128],NXTaddr[128],otherNXTaddr[128];
    printf("punch_client_main argc.%d [%s]\n",argc,Global_mp->NXTACCTSECRET);
    while ( 1 )
    {
        strcpy(dispname,Global_mp->dispname);
        strcpy(groupname,Global_mp->groupname);
        strcpy(NXTaddr,Global_mp->NXTADDR);
        if ( get_options(argc,argv,dispname,groupname,NXTaddr) < 0 )
        {
            printf("Invalid options, try again\n");
            sleep(10);
            continue;
        }
        //if ( Global_mp->dispname[0] == 0 && dispname[0] != 0 )
        //    safecopy(Global_mp->dispname,dispname,sizeof(Global_mp->dispname));
        if ( Global_mp->groupname[0] == 0 && groupname[0] != 0 )
            safecopy(Global_mp->groupname,groupname,sizeof(Global_mp->groupname));
        //if ( Global_mp->NXTADDR[0] == 0 && NXTaddr[0] != 0 )
        //    safecopy(Global_mp->NXTADDR,NXTaddr,sizeof(Global_mp->NXTADDR));
        EXIT_FLAG = 1;
        for (i=0; i<10; i++)
        {
            if ( set_intro(intro,sizeof(intro),Global_mp->dispname,Global_mp->groupname,Global_mp->NXTADDR) < 0 )
            {
                printf("invalid intro.(%s), try again\n",intro);
                sleep(10);
                continue;
            }
            memset(connect_id,0,sizeof(connect_id));
            if ( get_rand_ipaddr(otherNXTaddr,&port,servername) != 0 && (server_tcp= contact_punch_server(servername,connect_id,sizeof(connect_id),intro)) >= 0 )
            {
                EXIT_FLAG = 0;
                break;
            }
            if ( wait_for_input(server_tcp,servername,connect_id) < 0 )
                break;
        }
        while ( EXIT_FLAG == 0 )
        {
            prompt(Global_mp->dispname,Global_mp->groupname);
            if ( wait_for_input(server_tcp,servername,connect_id) < 0 )
                break;
        }
        printf("punch client finished\n");
    }
    return(1);
}

void *punch_client_glue(void *_argv)
{
    char **argv = _argv;
    int argc;
    for (argc=0; argv[argc]!=0; argc++)
    {
        ;
    }
    while ( 1 )
    {
        punch_client_main(argc,argv);
        printf("punch punch_client_main finished\n");
        EXIT_FLAG = 0;
        memset(Global_mp->groupname,0,sizeof(Global_mp->groupname));
    }
    return(0);
}
#endif
