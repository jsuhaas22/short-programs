#include <unistd.h>
#include <string.h>
#include <pthread.h>

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
	for (union header *ptr = head; ptr; ptr = ptr->s.next)
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
		void *blk = sbrk(sizeof(union header) + size);
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
		return;

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
		hptr->s.is_free = 1;
	}

	pthread_mutex_unlock(&mutex_lock);
}

void *s_calloc(size_t num, size_t size)
{
	if (!num || !size)
		return NULL;
	size_t total_size = num * size;
	if (size != total_size / num)
		return NULL;
	void *blk = s_malloc(total_size);
	if (!blk)
		return NULL;
	memset(blk, 0, total_size);
	return blk;
}

void *s_realloc(void *blk, size_t size)
{
	if (!blk || !size)
		return NULL;

	union header *hptr = (union header*) blk - 1;
	if (hptr->s.size >= size)
		return blk;

	void *ret = s_malloc(size);
	if (ret) {
		memcpy(ret, blk, hptr->s.size);
		s_free(blk);
	}
	return ret;
}
