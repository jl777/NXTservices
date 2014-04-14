//
//  jl777str.h

//  Created by jl777
//  MIT License
//

#ifndef gateway_jl777str_h
#define gateway_jl777str_h

#define NO_DEBUG_MALLOC


#define dstr(x) ((double)(x)/SATOSHIDEN)
long MY_ALLOCATED,NUM_ALLOCATED,MAX_ALLOCATED,*MY_ALLOCSIZES; void **PTRS;

void *mymalloc(long allocsize)
{
    void *ptr;
    MY_ALLOCATED += allocsize;
    ptr = malloc(allocsize);
    memset(ptr,0,allocsize);
#ifdef NO_DEBUG_MALLOC
    return(ptr);
#endif
    if ( NUM_ALLOCATED >= MAX_ALLOCATED )
    {
        MAX_ALLOCATED += 100;
        PTRS = realloc(PTRS,MAX_ALLOCATED * sizeof(*PTRS));
        MY_ALLOCSIZES = realloc(MY_ALLOCSIZES,MAX_ALLOCATED * sizeof(*MY_ALLOCSIZES));
    }
    MY_ALLOCSIZES[NUM_ALLOCATED] = allocsize;
    printf("%ld myalloc.%p %ld | %ld\n",NUM_ALLOCATED,ptr,allocsize,MY_ALLOCATED);
    PTRS[NUM_ALLOCATED++] = ptr;
    return(ptr);
}

void myfree(void *ptr,char *str)
{
    int32_t i;
    if ( ptr == 0 )
        return;
#ifdef NO_DEBUG_MALLOC
    free(ptr);
    return;
#endif
    //printf("%s: myfree.%p\n",str,ptr);
    for (i=0; i<NUM_ALLOCATED; i++)
    {
        if ( PTRS[i] == ptr )
        {
            MY_ALLOCATED -= MY_ALLOCSIZES[i];
            printf("%s: freeing %d of %ld | %ld\n",str,i,NUM_ALLOCATED,MY_ALLOCATED);
            if ( i != NUM_ALLOCATED-1 )
            {
                MY_ALLOCSIZES[i] = MY_ALLOCSIZES[NUM_ALLOCATED-1];
                PTRS[i] = PTRS[NUM_ALLOCATED-1];
            }
            NUM_ALLOCATED--;
            free(ptr);
            return;
        }
    }
    printf("couldn't find %p in PTRS[%ld]??\n",ptr,NUM_ALLOCATED);
    while ( 1 ) sleep(1);
    free(ptr);
}

int32_t expand_nxt64bits(char *NXTaddr,uint64_t nxt64bits)
{
    int32_t i,n;
    uint64_t modval;
    char rev[64];
    for (i=0; nxt64bits!=0; i++)
    {
        modval = nxt64bits % 10;
        rev[i] = modval + '0';
        nxt64bits /= 10;
    }
    n = i;
    for (i=0; i<n; i++)
        NXTaddr[i] = rev[n-1-i];
    NXTaddr[i] = 0;
    return(n);
}

char *nxt64str(uint64_t nxt64bits)
{
    static char NXTaddr[64];
    expand_nxt64bits(NXTaddr,nxt64bits);
    return(NXTaddr);
}

int32_t cmp_nxt64bits(char *str,uint64_t nxt64bits)
{
    char expanded[64];
    if ( str == 0 )//|| str[0] == 0 || nxt64bits == 0 )
        return(-1);
    if ( nxt64bits == 0 && str[0] == 0 )
        return(0);
    expand_nxt64bits(expanded,nxt64bits);
    return(strcmp(str,expanded));
}

uint64_t calc_nxt64bits(char *NXTaddr)
{
    int32_t c;
    int64_t n,i;
    uint64_t lastval,mult,nxt64bits = 0;
    n = strlen(NXTaddr);
    if ( n >= 64 )
    {
        printf("calc_nxt64bits: illegal NXTaddr.(%s) too long\n",NXTaddr);
        return(0);
    }
    mult = 1;
    lastval = 0;
    for (i=n-1; i>=0; i--,mult*=10)
    {
        c = NXTaddr[i];
        if ( c < '0' || c > '9' )
        {
            printf("calc_nxt64bits: illegal char.(%c %d) in (%s).%d\n",c,c,NXTaddr,(int)i);
            return(0);
        }
        nxt64bits += mult * (c - '0');
        //printf("%ld of %ld: %lx mult %lu * %d\n",(long)i,(long)n,(unsigned long)nxt64bits,(unsigned long)mult,(c-'0'));
        if ( nxt64bits < lastval )
            printf("calc_nxt64bits: warning: 64bit overflow %lx < %lx\n",(unsigned long)nxt64bits,(unsigned long)lastval);
        lastval = nxt64bits;
    }
    if ( cmp_nxt64bits(NXTaddr,nxt64bits) != 0 )
        printf("error calculating nxt64bits: %s -> %lx -> %s\n",NXTaddr,(long)nxt64bits,nxt64str(nxt64bits));
    return(nxt64bits);
}

int32_t listcmp(char **list,char *str)
{
    if ( list == 0 || str == 0 )
        return(-1);
    while ( *list != 0 && *list[0] != 0 )
    {
        //printf("(%s vs %s)\n",*list,str);
        if ( strcmp(*list++,str) == 0 )
            return(0);
    }
    return(1);
}

char *clonestr(char *str)
{
    char *clone;
    if ( str == 0 || str[0] == 0 )
    {
        printf("warning cloning nullstr.%p\n",str); //while ( 1 ) sleep(1);
        str = "<nullstr>";
    }
    clone = mymalloc(strlen(str)+1);
    strcpy(clone,str);
    return(clone);
}

int32_t safecopy(char *dest,char *src,long len)
{
    int32_t i = -1;
    if ( dest != 0 )
        memset(dest,0,len);
    if ( src != 0 )
    {
        for (i=0; i<len&&src[i]!=0; i++)
            dest[i] = src[i];
        if ( i == len )
        {
            printf("%s too long %ld\n",src,len);
            return(-1);
        }
        dest[i] = 0;
    }
    return(i);
}

int32_t unhex(char c)
{
    if ( c >= '0' && c <= '9' )
        return(c - '0');
    else if ( c >= 'a' && c <= 'f' )
        return(c - 'a' + 10);
    else return(0);
}

unsigned char _decode_hex(char *hex)
{
    return((unhex(hex[0])<<4) | unhex(hex[1]));
}

void decode_hex(unsigned char *bytes,int32_t n,char *hex)
{
    int32_t i;
    for (i=0; i<n; i++)
        bytes[i] = _decode_hex(&hex[i*2]);
    bytes[i] = 0;
}

char hexbyte(int32_t c)
{
    if ( c < 10 )
        return('0'+c);
    else return('a'+c-10);
}

int32_t init_hexbytes(char *hexbytes,unsigned char *message,long len)
{
    int32_t i,lastnonz = -1;
    for (i=0; i<len; i++)
    {
        if ( message[i] != 0 )
        {
            lastnonz = i;
            hexbytes[i*2] = hexbyte((message[i]>>4) & 0xf);
            hexbytes[i*2 + 1] = hexbyte(message[i] & 0xf);
        }
        else hexbytes[i*2] = hexbytes[i*2+1] = '0';
        //printf("i.%d (%02x) [%c%c] last.%d\n",i,message[i],hexbytes[i*2],hexbytes[i*2+1],lastnonz);
    }
    lastnonz++;
    hexbytes[lastnonz*2] = 0;
    return(lastnonz*2+1);
}

int64_t stripstr(char *buf,int64_t len)
{
    int32_t i,j,c;
    for (i=j=0; i<len; i++)
    {
        c = buf[i];
        //if ( c == '\\' )
        //    c = buf[i+1], i++;
        buf[j] = c;
        if ( buf[j] != ' ' && buf[j] != '\n' && buf[j] != '\r' && buf[j] != '\t' && buf[j] != '"' )
            j++;
    }
    buf[j] = 0;
    return(j);
}

char *replacequotes(char *str)
{
    char *newstr;
    int32_t i,j,n;
    for (i=n=0; str[i]!=0; i++)
        n += (str[i] == '"') ? 3 : 1;
    newstr = malloc(n + 1);
    for (i=j=0; str[i]!=0; i++)
    {
        if ( str[i] == '"' )
        {
            newstr[j++] = '%';
            newstr[j++] = '2';
            newstr[j++] = '2';
        }
        else newstr[j++] = str[i];
    }
    newstr[j] = 0;
    free(str);
    return(newstr);
}

int64_t stripwhite(char *buf,int64_t len)
{
    int32_t i,j,c;
    for (i=j=0; i<len; i++)
    {
        c = buf[i];
        //if ( c == '\\' )
        //    c = buf[i+1], i++;
        buf[j] = c;
        if ( buf[j] != ' ' && buf[j] != '\n' && buf[j] != '\r' && buf[j] != '\t' )
            j++;
    }
    buf[j] = 0;
    return(j);
}

char safechar64(int32_t x)
{
    x %= 64;
    if ( x < 26 )
        return(x + 'a');
    else if ( x < 52 )
        return(x + 'A' - 26);
    else if ( x < 62 )
        return(x + '0' - 52);
    else if ( x == 62 )
        return('_');
    else
        return('-');
}

#endif
