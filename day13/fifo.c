#include "bootpack.h"

#define FLAGS_OVERRUN	0x0001;

void fifo8_init(struct FIFO8 *fifo, int size, unsigned char* buf)
{
	fifo->data = buf;
	fifo->size = size;
}

int fifo8_put(struct FIFO8 *fifo, unsigned char data)
{
	if(!fifo->full) {
		fifo->data[fifo->end] = data;
		fifo->end = (fifo->end + 1) % fifo->size;
		if(fifo->end == fifo->start) {
			fifo->full = 1;
		}
		return 0;
	} else {
		fifo->flag |= FLAGS_OVERRUN;
		return -1;
	}
}

int fifo8_get(struct FIFO8 *fifo)
{
	if(fifo->start < fifo->end) {
		return fifo->data[fifo->start++];
	} else if(fifo->start > fifo->end) {
		int ret = fifo->data[fifo->start];
		fifo->start = (fifo->start + 1) % fifo->size;
		return ret;
	} 
	// otherwise it's either full or empty
	else if(fifo->full) {
		int ret = fifo->data[fifo->start];
		fifo->start = (fifo->start + 1) % fifo->size;
		fifo->full = 0;
		return ret;
	} else {
		return -1;
	}
}

/* check number of valid entry of this fifo, if it's >0 then return the size */ 
int fifo8_status(struct FIFO8 *fifo)
{
	if(fifo->start < fifo->end) {
		return fifo->end - fifo->start;
	} else if (fifo->start > fifo->end) {
		return fifo->size - fifo->start + fifo->end;
	} else if(fifo->full) {
		return fifo->size;
	} else {
		return 0;
	}
}

/* 32 bit version */

void fifo32_init(struct FIFO32 *fifo, int size, int* buf)
{
	fifo->data = buf;
	fifo->size = size;
	fifo->full = 0;
	fifo->flag = 0;
	fifo->start = 0;
	fifo->end = 0;
	return;
}

int fifo32_put(struct FIFO32 *fifo, int data)
{
	if(!fifo->full) {
		fifo->data[fifo->end] = data;
		fifo->end = (fifo->end + 1) % fifo->size;
		if(fifo->end == fifo->start) {
			fifo->full = 1;
		}
		return 0;
	} else {
		fifo->flag |= FLAGS_OVERRUN;
		return -1;
	}
}

int fifo32_get(struct FIFO32 *fifo)
{
	if(fifo->start < fifo->end) {
		return fifo->data[fifo->start++];
	} else if(fifo->start > fifo->end) {
		int ret = fifo->data[fifo->start];
		fifo->start = (fifo->start + 1) % fifo->size;
		return ret;
	} 
	// otherwise it's either full or empty
	else if(fifo->full) {
		int ret = fifo->data[fifo->start];
		fifo->start = (fifo->start + 1) % fifo->size;
		fifo->full = 0;
		return ret;
	} else {
		return -1;
	}
}

/* check number of valid entry of this fifo, if it's >0 then return the size */ 
int fifo32_status(struct FIFO32 *fifo)
{
	if(fifo->start < fifo->end) {
		return fifo->end - fifo->start;
	} else if (fifo->start > fifo->end) {
		return fifo->size - fifo->start + fifo->end;
	} else if(fifo->full) {
		return fifo->size;
	} else {
		return 0;
	}
}

