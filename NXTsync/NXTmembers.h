//
//  NXTmembers.h
//  Created by jl777, April 26-27 2014
//  MIT License
//
// todo: merge client with member with NXTacct

#ifndef gateway_NXTmembers_h
#define gateway_NXTmembers_h

static char *group_dir = NULL;
 time_t group_dir_read;
static struct group *group_list;
static client_t *client_list;
static int n_clients;

// Function: client_find_by_socket
// Find the client that uses the specified socket.
static client_t *client_find_by_socket(int sock)
{
    client_t *pc = client_list;
    while ( pc != 0 )
    {
        if ( pc->sock == sock )
            return(pc);
        pc = pc->next;
    }
    return NULL;
}

// Function: client_find_by_name
// Find the client that has the specified name and group.
static client_t *client_find_by_name(const char *user,const group_t *pg)
{
    client_t *pc = client_list;
    while ( pc != 0 )
    {
        if ( strcmp(pc->user,user) == 0 && pg == pc->group )
            return(pc);
        pc = pc->next;
    }
    return(NULL);
}

// Function: client_find_by_id
// Find the client with the specified ID (see <create_connection_id>).
static client_t *client_find_by_id(const char *id)
{
    client_t *pc = client_list;
    while ( pc != 0 )
    {
        if ( strcmp(pc->id,id) == 0 )
            return(pc);
        pc = pc->next;
    }
    return(NULL);
}

// Function: member_find
// Search the list for a member with the spcified name.
static member_t *member_find(const char *name)
{
    member_t *pm = member_list;
    while ( pm != 0 )
    {
        if ( strcmp(pm->name,name) == 0 )
            return(pm);
        pm = pm->next;
    }
    return(NULL);
}

member_t *member_find_NXTaddr(const char *NXTaddr)
{
    member_t *pm = member_list;
    while ( pm != 0 )
    {
        if ( strcmp(pm->NXTaddr, NXTaddr) == 0 )
            return(pm);
        pm = pm->next;
    }
    return(NULL);
}

member_t *member_find_ipbits(uint32_t ipbits)
{
    member_t *pm = member_list;
    while ( pm != 0 )
    {
        if ( pm->ipbits == ipbits )
            return(pm);
        pm = pm->next;
    }
    return(NULL);
}

// Function: member_find_by_fd
// Search the list for a member with the spcified socket file descriptor.
member_t *member_find_by_fd(int fd)
{
    member_t *pm = member_list;
    while ( pm != 0 )
    {
        if ( pm->fd == fd && fd >= 0 )
            return(pm);
        pm = pm->next;
    }
    return(NULL);
}

member_t *get_rand_member(group_t *pg)
{
    double prob;
    member_t *pm = member_list;
    if ( pg != 0 )
    {
        prob = 1. / (pg->n_members + 1);
        while ( pm != 0 )
        {
            if ( strcmp(ipbits_str(pm->ipbits),SERVER_NAMEA) != 0 && strcmp(pm->NXTaddr,Global_mp->NXTADDR) != 0 && ((double)(rand() % 1000000000L)/1000000000L) < prob )
                return(pm);
            pm = pm->next;
        }
    }
    return(0);
}

// Function: name_ok
// Return non-zero if the given string is valid for use as a name/group.
// Currently this means that it must contain only alphanumeric characters or
// the underscore, minus or plus symbols, and this it must be at least 3 characters long.
static int name_ok(const char *s)
{
    const char *t = s;
    if ( s == 0 || *s == 0 )
        return(0);
    for (; *s; ++s)
        if ( isalnum(*s) == 0 && *s != '_' && *s != '-' && *s != '+' )
        {
            printf("invalid name.(%s)\n",t);
            return(0);
        }
    return (s - t >= 3);
}

// Function: group_file_read
// Read the group password from a permanent group file.
static char *group_file_read(const char *filename)
{
    static char pw[NAME_SIZE+1];
    char path[128],*s;
    FILE *f;
    int n = sprintf(path,  "%s/%s",group_dir,filename);
    if ( n >= (int)sizeof(path) )
    {
        slog("group file path too long: %s/%s\n",group_dir,filename);
        return NULL;
    }
    if ( (f= fopen(path,"r")) == NULL)
    {
        slog_error(path);
        return(NULL);
    }
    pw[0] = '\0';
    fgets(pw,sizeof pw,f);
    s = pw + strcspn(pw, " \n\r");
    *s = '\0';
    fclose(f);
    return(pw);
}


// Function: group_create
// Create the named group.  Space is allocated for a group structure, the name
// and password are copied, and the struct is added to the linked list of
// groups.  The caller must ensure that the group does not already exist.
static group_t *group_create(const char *group)//, const char *pass)
{
    group_t *pg = calloc(1, sizeof(group_t));
    if ( pg == 0 )
        slog_error("calloc");
    else
    {
        strncpy(pg->name,group,NAME_SIZE);
        //strncpy(pg->pass,pass,NAME_SIZE);
        // link to global list
        pg->next = group_list;
        pg->prev = NULL;
        group_list = pg;
        if ( pg->next != 0 )
            pg->next->prev = pg;
        slog(": Group '%s' created\n", group);
        printf("Created group.%s\n",group);
    }
    return pg;
}

// Function: group_add_perm
// Add a permanent group
static void group_add_perm(const char *name)
{
    const char *pw = group_file_read(name);
    group_t *pg = NULL;
    if ( pw != 0 )
        pg = group_create(name);//,pw);
    if ( pg != 0 )
        pg->n_members = 1;
}

#ifdef ENABLE_DIRENT
// Function: group_dir_changed
// Return non-zero if the group directory has changed.
static int group_dir_changed(void)
{
    struct stat st;
    if ( group_dir == 0 )
        return(0);
    else if ( stat(group_dir, &st) != 0 )
    {
        slog_error(group_dir);
        return(0);
    }
    return(st.st_mtime > group_dir_read);
}

// Function: group_dir_add
// Scan the group directory for new entries  - ie those that are not currently registered as groups.
static void group_dir_add(void)
{
    group_t *group_find(const char *group);
    struct dirent *dp;
    DIR *dirp = opendir(group_dir);
    if ( dirp == 0 )
        slog_error(group_dir);
    else
    {
        group_dir_read = time(NULL);
        while ( (dp= readdir(dirp)) != NULL )
            if ( dp->d_name[0] != '.' && group_find(dp->d_name) == 0 )
                group_add_perm(dp->d_name);
        closedir(dirp);
    }
}
#endif

// Function: group_find
// Find the named group, ignoring characters beyond the end of the allowed name size.
group_t *group_find(const char *group)
{
    group_t *pg;
#ifdef ENABLE_DIRENT
   if ( group_dir_changed() != 0 ) // jl777: disabled to make windows compatibility easier
        group_dir_add();        // changes group_list
#endif
    pg = group_list;
    while ( pg != 0 )
    {
        if ( strncmp(pg->name,group,NAME_SIZE) == 0 )
            return(pg);
        pg = pg->next;
    }
    return NULL;
}

// Function: group_auth
// Validate the supplied password against the group password.
/*static int group_auth(group_t *pg, const char *pass)
 {
 printf("Password.(%s) vs (%s)\n",pg->pass,pass);
 return strcmp(pg->pass, pass) ? -1 : 0;
 }*/

// Function: group_join
// Check that a client with the same name is not a member of this group and increment the member count (needed to allow empty groups to be deleted).
static int group_join(group_t *pg,const char *user,char *NXTaddr)
{
    client_t *pc;
    if ( (pc= client_find_by_name(user,pg)) == 0 )
    {
        pg->n_members++;
        return 0;
    }
    if ( strcmp(pc->NXTaddr,NXTaddr) == 0 )
    {
        printf("%s returning NXT.%s\n",user,NXTaddr);
        return(0);
    }
    return -1;
}

// Function: group_delete
// Delete the specified group, unlinking it from the linked list and freeing memory.
static void group_delete(group_t *pg)
{
    group_t *prev = pg->prev;
    group_t *next = pg->next;
    assert(pg->n_members == 0);
    slog(": Group '%s' deleted\n", pg->name);
    free(pg);
    if ( prev != 0 )
        prev->next = next;
    else group_list = next;
    if ( next != 0 )
        next->prev = prev;
}

// Function: group_delete_member
// Reduce the member-count for the specified group and delete the group if no  members remain.
static void group_delete_member(group_t *pg)
{
    if ( pg != 0 && --pg->n_members <= 0 )
        group_delete(pg);
}

// Function: create_connection_id
// Create an ID to be returned to the client upon successful connection.  The
// ID is used when a client wants to punch a hole.  Lame ID, but probably not easily spoofable.
static const char *create_connection_id(char *id,int max,unsigned long count)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    sprintf(id,"id:%d-%ld",(int)tv.tv_usec,count);
    printf("created connection_id.(%s)\n",id);
    return(id);
}

// Function: client_create
// Create a new client structure, populate its non-zero members and link it into the global list.
static client_t *client_create(struct sockaddr_in *addr,int fd)
{
    static unsigned long count;
    client_t *pc = calloc(1, sizeof(client_t));
    if ( pc == 0 )
        slog_error("calloc");
    else
    {
        pc->addr = *addr;
        pc->sock = fd;
        create_connection_id(pc->id,sizeof(pc->id)-1,++count);
        // link to global list
        pc->next = client_list;
        pc->prev = NULL;
        client_list = pc;
        if ( pc->next != 0 )
            pc->next->prev = pc;
        n_clients++;
        //slog(": Client [%d] added at %s\n",++n_clients,print_host_address(addr));
        printf(": Client [%d].(%s) added at %s\n",n_clients,pc->id,print_host_address(addr));
    }
    return(pc);
}

// Function: client_delete
// Close the specified client, freeing memory and unlinking from the global client list.
static void client_delete(client_t *pc)
{
    client_t *prev = pc->prev;
    client_t *next = pc->next;
    close(pc->sock);
    group_delete_member(pc->group);
    --n_clients;
    free(pc);
    if (prev)
        prev->next = next;
    else client_list = next;
    if ( next != 0 )
        next->prev = prev;
}

// Function: member_create
// Create a new group member and link into the list
static member_t *member_create(char *groupname,const char *name,const char *NXTaddr,const char *status)//,char *pubkey)
{
    member_t *pm = calloc(1, sizeof(member_t) + (strcmp(groupname,"NXTsubatomic")==0)*2*NXTSUBATOMIC_SHAREDMEMSIZE);
    if ( pm == 0 )
        perror("calloc");
    else
    {
        stgncpy(pm->name,name,sizeof(pm->name));
        stgncpy(pm->NXTaddr,NXTaddr,sizeof(pm->NXTaddr));
        stgncpy(pm->status,status,sizeof(pm->status));
        pm->fd = -1;
        // link to global list
        pm->next = member_list;
        pm->prev = NULL;
        pm->memfragments = SYNC_MAXUNREPORTED;
        pm->memincr = SYNC_FRAGSIZE;
        member_list = pm;
        if ( pm->next != 0 )
            pm->next->prev = pm;
    }
    return pm;
}

// Function: member_set_status
// Record the updated status reported by the server.
static void member_set_status(member_t *pm, const char *status)
{
    stgncpy(pm->status,status,sizeof(pm->status));
}

// Function: member_add
// Add a new group member if not already present.
static void member_add(char *groupname,char *s)
{
    member_t * pm;
    printf("add member.(%s)\n",s);
    char *name = stgsep(&s, " ");
    char *NXTaddr = stgsep(&s, " ");
    // char *pubkey   = stgsep(&s, " ");
    char *status = stgsep(&s, "]");
    status += strspn(status, " ["); // status points to space or '['
    pm = member_find(name);
    if ( pm != 0 )
        member_set_status(pm,status);
    else member_create(groupname,name,NXTaddr,status);
}

// Function: member_del
// Delete a member of the group and unlink from the global list
static void member_del(char *s)
{
    char *name = stgsep(&s, " ");
    member_t *pm = member_find(name);
    if ( pm != 0 )
    {
        member_t *prev = pm->prev;
        member_t *next = pm->next;
        clear_pending_sync_files(pm);
        punch_cancel(pm);
        free(pm);
        if ( prev != 0 )
            prev->next = next;
        else member_list = next;
        if ( next != 0 )
            next->prev = prev;
    }
}

// Function: list_members
// List all members of our group
static void members_list(void)
{
    member_t *pm = member_list;
    while ( pm != 0 )
    {
        fprint(stdout,"\t%s %s [%s] %s/%d %s\n",pm->name, pm->NXTaddr, pm->status,ipbits_str(pm->ipbits),pm->port,pm->pubkeystr);
        pm = pm->next;
    }
}

// Function: send_one
// Send the supplied text to the specified (first word) correspondent client.
static void send_one(char *text)
{
    char *name = stgsep(&text, " \t:");
    member_t *pm = member_find(++name);
    if ( pm == 0 )
        fprint(stderr, "%s: not correspondent\n", name);
    else if ( pm->have_address != 0 )
    {
        if ( text[0] == '"' && text[strlen(text)-1] == '"' )
        {
            text[strlen(text)-1] = 0;
            start_send_file(pm,pm->name,text+1);
        }
        else correspond(pm,text,strlen(text) + 1,SYNC_SENDTEXT);
    }
}

// Function: send_all
// Send the supplied text to all correspondent clients.
static int send_all(char *text)
{
    member_t *pm = member_list;
    int n = 0;
    size_t len = strlen(text) + 1;
    while ( pm != 0 )
    {
        if ( pm->have_address != 0 )
        {
            ++n;
            correspond(pm,text,len,SYNC_SENDTEXT);
        }
        pm = pm->next;
    }
    if ( n == 0 )
        Global_mp->corresponding = 0;
    return(n);
}

// Function: ping_all
// Send something to the all correspondent addresses.
static void ping_all(char *NXTaddr,char *servername,char *connect_id)
{
    member_t *pm = member_list;
    while ( pm != 0 )
    {
        if ( pm->have_address != 0 && strcmp(pm->NXTaddr,Global_mp->NXTADDR) != 0 )
            ping(pm,NXTaddr,-1,0);
        pm = pm->next;
    }
}

// Function: timeout_connections
// Close any connections that have had no pings for 60 seconds.
static void timeout_connections(void)
{
    member_t *pm = member_list;
    while ( pm != 0 )
    {
        if ( pm->have_address != 0 && elapsed_ms(&pm->ping_time) > CONNECTION_TIMEOUT && elapsed_ms(&pm->comm_time) > CONNECTION_TIMEOUT )
        {
            punch_cancel(pm);
            fprint(stderr, "Connection timed-out [%s]\n", pm->name);
        }
        pm = pm->next;
    }
}

// Function: close_all_connections
// Close all open connections.
static void close_all_connections(void)
{
    member_t *pm = member_list;
    fprint(stderr, "Closed: ");
    while ( pm != 0 )
    {
        if ( punch_cancel(pm) != 0 )
            fprint(stderr, "'%s', ", pm->name);
        pm = pm->next;
    }
    fprint(stderr, "\n");
}

// Function: prompt
// Print a command line prompt
static void prompt(char *user,char *group)
{
    if ( prompt_again == 0 )
        return;
    if ( Global_mp->corresponding != 0 )
    {
        int n = 0;
        member_t *pm = member_list;
        fprintf(stdout, "[");
        while ( pm != 0 )
        {
            if ( pm->have_address != 0 )
            {
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

int32_t find_Active_NXTaddr(uint64_t nxt64bits)
{
    int32_t i;
    for (i=0; i<Num_ipaddrs; i++)
    {
        if ( Active_NXTaddrs[i] == nxt64bits )
            return(i);
    }
    return(-1);
}

char *get_client_ipaddr(char *ipaddr,client_t *pc)
{
    int32_t ind;
    uint64_t nxt64bits;
    nxt64bits = calc_nxt64bits(pc->NXTaddr);
    if ( (ind= find_Active_NXTaddr(nxt64bits)) >= 0 )
    {
        expand_ipbits(ipaddr,IPaddrs[ind]);
        return(ipaddr);
    }
    return(0);
}

uint16_t get_client_port(client_t *pc)
{
    int32_t ind;
    uint64_t nxt64bits;
    nxt64bits = calc_nxt64bits(pc->NXTaddr);
    if ( (ind= find_Active_NXTaddr(nxt64bits)) >= 0 )
        return(IPports[ind]);
    return(0);
}
#endif
