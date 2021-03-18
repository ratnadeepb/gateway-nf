/* 
 * Initialize the required memory areas
 */

#ifndef __INIT_H__
#define __INIT_H__

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include "includes.h"

#define NUM_PAGES 1024 /* each page corresponds to a connection */
#define NUM_SLOTS 24   /* each slot corresponds to a header */

#define BASE_PROC_NAME "/dev/shm/processing" /* base name for the processing packet files */
#define BASE_COMPL_NAME "/dev/shm/completed" /* base name for the completed packet files */

static Record *PROCESSING; /* this is the area that holds all packets being processed */
static Record *COMPLETED; /* this holds all processed but unacknowledge packets */

void create_regions();

#endif