/* Modified from here
 * https://github.com/RedisLabs/rmnotify
 */

#pragma once

#include <event.h>
#include <pthread.h>
#include "hiredis.h"
#include "async.h"

typedef struct keyspace_notifier {
	redisContext *c; /* sync connection to configure receiving event notifications */
	redisAsyncContext *async; /* for receiving event notifications */
	struct event_base *base; /* libevent base */
	pthread_t tid; /* for running the event base */
} keyspace_notifier;

/* start a new notifier */
keyspace_notifier *new_keyspace_notifier(const char *hostname, int port);

/* handle set event notifications */
void handle_set_event(const keyspace_notifier *n, const char *event);

/* register for set event notification */
int notifier_register_set_event(keyspace_notifier *notifier);

/* deregister for set event notification */
int notifier_deregister_set_event(keyspace_notifier *notifier);

/* get key value */
redisReply *notifier_issue_redis_command(const keyspace_notifier *notifier, const char *key);

/* free the notifier */
void free_keyspace_notifier(keyspace_notifier *notifier);