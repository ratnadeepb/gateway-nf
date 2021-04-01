#pragma once

#include "hiredis.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PID_FILE "/run/onvm_gateway_nf.pid" /* our pid file */
#define NF_TAG_FILE "/run/onvm_nf_tags" /* list of all NFs registering with the gateway */

#define MAX_NF_TAG_SZ 30

typedef enum { false=0, true } bool;

/* forward declarations */
typedef struct conn_ Conn;
typedef struct record_ Record;
typedef struct entry_ Entry;

/* 
 * copy_record copies a record to a memory location provided by the client
 * This function is at the core of the CoW design
 * @rec -> original pointer that is to be copied
 * @mem -> head of the memory region to hold the records queue
 * @offset -> where in the queue the new record needs to be stored at
 */
void copy_record(Record *rec, Record *mem, off_t offset)
{
	Record *ptr = mem + offset;
	memcpy(ptr, rec, sizeof(Record));
}

/* compare two Entry structs by value */
bool compare_entry(const Entry *lhs, const Entry *rhs)
{
	/* since each connection has its own Entry, it's enough to compare either the
	 * pointers or the file names for equality
	 */
	if (lhs->mem == rhs->mem) return true;
	return false;
}