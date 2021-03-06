//
//  bitcoind_RPC.c
//  Created by jl777, Mar 27, 2014
//  MIT License
//
// based on example from http://curl.haxx.se/libcurl/c/getinmemory.html and util.c from cpuminer.c

#ifndef JL777_BITCOIND_RPC
#define JL777_BITCOIND_RPC

//#include <stdint.h>
//#include <stdlib.h>
//#include <unistd.h>
//#include <memory.h>
//#include <pthread.h>
//#include <sys/time.h>


#define NUM_BITCOIND_RETRIES 3  // bitcoind spews lots of errors, without retries and long enough delay, crashes
#define MAGIC_BITCOINC_RPCDELAY 13  // smaller numbers cause mysterious crash, probably in libcurl??
#define EXTRACT_BITCOIND_RESULT     // if defined, ensures error is null and returns the "result" field

#ifdef EXTRACT_BITCOIND_RESULT
//#include "cJSON.h"
char *post_process_bitcoind_RPC(char *debugstr,char *command,char *params,char *rpcstr)
{
    long i,j,len;
    char *retstr = 0;
    cJSON *json,*result,*error;
    if ( command == 0 || rpcstr == 0 || rpcstr[0] == 0 )
        return(rpcstr);
    //printf("%s post_process_bitcoind_RPC.%s.[%s] (%s)\n",debugstr,command,params,rpcstr);
    json = cJSON_Parse(rpcstr);
    if ( json == 0 )
    {
        printf("%s post_process_bitcoind_RPC.%s.[%s] can't parse.(%s)\n",debugstr,command,params,rpcstr);
        free(rpcstr);
        return(0);
    }
    result = cJSON_GetObjectItem(json,"result");
    error = cJSON_GetObjectItem(json,"error");
    if ( error != 0 && result != 0 )
    {
        if ( (error->type&0xff) == cJSON_NULL && (result->type&0xff) != cJSON_NULL )
        {
            retstr = cJSON_Print(result);
            len = strlen(retstr);
            if ( retstr[0] == '"' && retstr[len-1] == '"' )
            {
                for (i=1,j=0; i<len-1; i++,j++)
                    retstr[j] = retstr[i];
                retstr[j] = 0;
            }
        }
        else if ( (error->type&0xff) != cJSON_NULL || (result->type&0xff) != cJSON_NULL )
            printf("%s post_process_bitcoind_RPC (%s) (%s) error.%s\n",debugstr,command,params,rpcstr);
    }
    free(rpcstr);
    free_json(json);
    return(retstr);
}
#endif

struct upload_buffer { const unsigned char *buf; size_t len; };
struct MemoryStruct { char *memory; size_t size; };


static size_t WriteMemoryCallback(void *ptr,size_t size,size_t nmemb,void *data)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)data;
    //printf("WriteMemoryCallback\n");
    mem->memory = realloc(mem->memory, mem->size + realsize + 1);
    if (mem->memory) {
        memcpy(&(mem->memory[mem->size]), ptr, realsize);
        mem->size += realsize;
        mem->memory[mem->size] = 0;
    }
    return realsize;
}

static size_t upload_data_cb(void *ptr,size_t size,size_t nmemb,void *user_data)
{
    struct upload_buffer *ub = user_data;
    uint32_t len = (int)(size * nmemb);
    if ( len > ub->len )
        len = (int)ub->len;
    if ( len != 0 )
    {
        memcpy(ptr,ub->buf,len);
        ub->buf += len;
        ub->len -= len;
    }
    return len;
}

char *bitcoind_RPC(char *debugstr,int32_t numretries,char *url,char *userpass,char *command,char *params)
{
    static int32_t count,didinit;
    static double elapsedsum,elapsedsum2,laststart;
    char *retstr,*bracket0,*bracket1,*databuf=0,len_hdr[10240];
    CURL *curl_handle;
    CURLcode res;
    long len;
    int32_t delay = MAGIC_BITCOINC_RPCDELAY;
    double starttime;
    struct curl_slist *headers = NULL;
    struct upload_buffer upload_data;
    struct MemoryStruct chunk;
    double milliseconds();
    //static pthread_mutex_t mutex;
    //pthread_mutex_lock(&mutex);
    if ( didinit == 0 )
    {
        didinit = 1;
        curl_global_init(CURL_GLOBAL_ALL); //init the curl session
    }
    //printf("start bitcoind_RPC %s\n",command!=0?command:"");
    starttime = milliseconds();
    if ( 1 && laststart+1 > starttime ) // horrible hack for bitcoind "Couldn't connect to server"
    {
        usleep(2000);
        starttime = milliseconds();
    }
    laststart = starttime;
    if ( params == 0 )
        params = "";
retry:
    chunk.memory = malloc(1);     // will be grown as needed by the realloc above
    chunk.size = 0;                 // no data at this point
    curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle,CURLOPT_CONNECTTIMEOUT,2); // chanc3r: limit any *real* timeouts to 2s
    curl_easy_setopt(curl_handle,CURLOPT_FRESH_CONNECT,1);  // chanc3r: force a new connection for each request - i'm     curl_easy_setopt(curl_handle,CURLOPT_SSL_VERIFYHOST,0);
    curl_easy_setopt(curl_handle,CURLOPT_SSL_VERIFYPEER,0);
    curl_easy_setopt(curl_handle,CURLOPT_URL,url);
    curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);        // supposed to fix "Alarm clock" and long jump crash
    curl_easy_setopt(curl_handle,CURLOPT_WRITEFUNCTION,WriteMemoryCallback); // send all data to this function
    curl_easy_setopt(curl_handle,CURLOPT_WRITEDATA,(void *)&chunk); // we pass our 'chunk' struct to the callback function
    curl_easy_setopt(curl_handle,CURLOPT_USERAGENT,"libcurl-agent/1.0"); // some servers don't like requests that are made without a user-agent field, so we provide one
    if ( userpass != 0 )
        curl_easy_setopt(curl_handle,CURLOPT_USERPWD,userpass);
    if ( command != 0 )
    {
        curl_easy_setopt(curl_handle,CURLOPT_READFUNCTION,upload_data_cb);
        curl_easy_setopt(curl_handle,CURLOPT_READDATA,&upload_data);
        curl_easy_setopt(curl_handle,CURLOPT_POST,1);
        len = strlen(params);
        if ( len > 0 && params[0] == '[' && params[len-1] == ']' )
            bracket0 = bracket1 = "";
        else bracket0 = "[", bracket1 = "]";
        databuf = malloc(4096 + strlen(command) + strlen(params));
        sprintf(databuf,"{\"id\":\"jl777\",\"method\":\"%s\",\"params\":%s%s%s}",command,bracket0,params,bracket1);
       // printf(">>>>(%s)<<<<< args.(%s)\n",databuf,params);
        upload_data.buf = (unsigned char *)databuf;
        upload_data.len = strlen(databuf);
        sprintf(len_hdr, "Content-Length: %lu",(unsigned long)upload_data.len);
        headers = curl_slist_append(NULL,"Expect:");
        headers = curl_slist_append(headers,"Content-type: application/json");
        headers = curl_slist_append(headers,len_hdr);
        curl_easy_setopt(curl_handle,CURLOPT_HTTPHEADER,headers);
    }
    res = curl_easy_perform(curl_handle);
    if ( headers != 0 )
        curl_slist_free_all(headers);
    if ( databuf != 0 )
        free(databuf), databuf = 0;
    if ( res != CURLE_OK )
    {
        fprintf(stderr, "curl_easy_perform() failed: %s %s.(%s %s %s %s)\n",curl_easy_strerror(res),debugstr,url,userpass,command,params);
        if ( command != 0 && params != 0 )
            sleep(delay);
        else sleep(1);
        //delay *= 3;
        if ( numretries-- > 0 )
        {
            free(chunk.memory);
            curl_easy_cleanup(curl_handle);
            //curl_global_cleanup();
            //curl_global_init(CURL_GLOBAL_ALL); //init the curl session
            goto retry;
        }
    }
    else
    {
        // printf("%lu bytes retrieved [%s]\n", (int64_t )chunk.size,chunk.memory);
    }
    elapsedsum += (milliseconds() - starttime);
    curl_easy_cleanup(curl_handle);
    retstr = malloc(strlen(chunk.memory)+1);
    strcpy(retstr,chunk.memory);
    free(chunk.memory);

    count++;
    elapsedsum2 += (milliseconds() - starttime);
#ifndef __APPLE__
    if ( elapsedsum2/count > 1000 && (milliseconds() - starttime) > 1000 )
        fprintf(stderr,"%d: %9.6f %9.6f | elapsed %.3f millis | bitcoind_RPC.(%s)\n",count,elapsedsum/count,elapsedsum2/count,(milliseconds() - starttime),url);
#endif
   // pthread_mutex_unlock(&mutex);
    return(post_process_bitcoind_RPC(debugstr,command,params,retstr));
}

#endif

