#include "helper.h"
#include <unistd.h>

union header {
	struct {
		size_t size;
		unsigned short is_free;
		union header *next;
	} s;
	char align[16];
};

union header *head, *tail;
pthread_mutex_t mutex_lock;

union header *get_free_blk(size_t size)
{
	for (union header *ptr = head; ptr; ptr = ptr->next)
		if (ptr->s.is_free && ptr->s.size >= size)
			return ptr;
	return NULL;
}

void *s_malloc(size_t size)
{
	if (!size)
		return NULL;

	pthread_mutex_lock(&mutex_lock);

	union header *hptr = get_free_blk(size);
	if (hptr) {
		hptr->s.is_free = 0;
	} else {
		void *blk = sbrk(sizeof(union header_t) + size);
		if (blk == (void*) -1) {
			pthread_mutex_unlock(&mutex_lock);
			return NULL;
		}
		hptr = blk;
		hptr->s.size = size;
		hptr->s.is_free = 0;
		hptr->s.next = NULL;
		if (!head)
			head = hptr;
		if (tail)
			tail->s.next = hptr;
		tail = hptr;
	}

	pthread_mutex_unlock(&mutex_lock);
	return (void*)(hptr + 1);
}

void s_free(void *blk)
{
	if (!blk)
		return NULL;

	pthread_mutex_lock(&mutex_lock);

	union header *hptr = (union header*) blk - 1;
	if ((char*) blk + hptr->s.size == sbrk(0)) {
		if (head == tail) {
			head = tail = NULL;
		} else {
			union header *tmp = head;
			while (tmp->s.next != tail)
				tmp = tmp->s.next;
			tmp->s.next = NULL;
			tail = tmp;
		}
		sbrk(0 - sizeof(union header*) - hptr->s.size);
	} else {
		hptr->is_free = 1;
	}

	pthread_mutex_unlock(&mutex_lock);
}
