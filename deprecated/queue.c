/*
 c-pthread-queue - c implementation of a bounded buffer queue using posix threads
 Copyright (C) 2008  Matthew Dickinson
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <pthread.h>
#include <stdio.h>

#ifndef _QUEUE_H
#define _QUEUE_H

#define QUEUE_INITIALIZER(buffer) { buffer, sizeof(buffer) / sizeof(buffer[0]), 0, 0, 0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER }

typedef struct queue
{
	void **buffer;
    int32_t capacity;
	int32_t size;
	int32_t in;
	int32_t out;
	pthread_mutex_t mutex;
	pthread_cond_t cond_full;
	pthread_cond_t cond_empty;
} queue_t;

struct pingpong_queue
{
    char *name;
    queue_t pingpong[2],*destqueue,*errorqueue;
    int32_t (*action)();
};

void queue_enqueue(queue_t *queue,void *value)
{
	pthread_mutex_lock(&(queue->mutex));
	while ( queue->size == queue->capacity )
    {
        queue->capacity++;
        queue->buffer = realloc(queue->buffer,sizeof(*queue->buffer) * queue->capacity);
        queue->buffer[queue->size] = 0;
		//pthread_cond_wait(&(queue->cond_full), &(queue->mutex));
    }
//printf("enqueue %lx -> [%d] size.%d capacity.%d\n",*(long *)value,queue->in,queue->size,queue->capacity);
	queue->buffer[queue->in] = value;
	++queue->size;
	++queue->in;
	queue->in %= queue->capacity;
	pthread_mutex_unlock(&(queue->mutex));
	//pthread_cond_broadcast(&(queue->cond_empty));
}

void *queue_dequeue(queue_t *queue)
{
    void *value = 0;
	pthread_mutex_lock(&(queue->mutex));
	//while ( queue->size == 0 )
	//	pthread_cond_wait(&(queue->cond_empty), &(queue->mutex));
    if ( queue->size > 0 )
    {
        value = queue->buffer[queue->out];
        queue->buffer[queue->out] = 0;
//printf("dequeue %lx from %d, size.%d capacity.%d\n",*(long *)value,queue->out,queue->size,queue->capacity);
        --queue->size;
        ++queue->out;
        queue->out %= queue->capacity;
    }
	pthread_mutex_unlock(&(queue->mutex));
	//pthread_cond_broadcast(&(queue->cond_full));
	return value;
}

int32_t queue_size(queue_t *queue)
{
	pthread_mutex_lock(&(queue->mutex));
	int32_t size = queue->size;
	pthread_mutex_unlock(&(queue->mutex));
	return size;
}

void init_pingpong_queue(struct pingpong_queue *ppq,char *name,int32_t (*action)(),queue_t *destq,queue_t *errorq)
{
    ppq->name = name;
    ppq->destqueue = destq;
    ppq->errorqueue = errorq;
    ppq->action = action;
}

// seems a bit wastefull to do all the two iter queueing/dequeuing with threadlock overhead
// however, there is assumed to be plenty of CPU time relative to actual blockchain events
// also this method allows for adding of parallel threads without worry
void process_pingpong_queue(struct pingpong_queue *ppq,void *argptr)
{
    int32_t iter,retval;
    void *ptr;
    //printf("%p process_pingpong_queue.%s %d %d\n",ppq,ppq->name,queue_size(&ppq->pingpong[0]),queue_size(&ppq->pingpong[1]));
    for (iter=0; iter<2; iter++)
    {
        while ( (ptr= queue_dequeue(&ppq->pingpong[iter])) != 0 )
        {
            //printf("%s pingpong[%d].%p action.%p\n",ppq->name,iter,ptr,ppq->action);
            retval = (*ppq->action)(&ptr,argptr);
            if ( retval == 0 )
                queue_enqueue(&ppq->pingpong[iter ^ 1],ptr);
            else if ( retval < 0 )
            {
                printf("iter.%d errorqueue %p vs %p\n",iter,ppq->errorqueue,&ppq->pingpong[0]);
                if ( ppq->errorqueue == &ppq->pingpong[0] )
                    queue_enqueue(&ppq->pingpong[iter ^ 1],ptr);
                else queue_enqueue(ppq->errorqueue,ptr);
            }
            else if ( ppq->destqueue != 0 )
            {
                printf("iter.%d destqueue %p vs %p\n",iter,ppq->destqueue,&ppq->pingpong[0]);
                if ( ppq->destqueue == &ppq->pingpong[0] )
                    queue_enqueue(&ppq->pingpong[iter ^ 1],ptr);
                else queue_enqueue(ppq->destqueue,ptr);
            }
            else free(ptr);
        }
    }
}

#endif
