#include "keyspacenotification.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <inttypes.h>
#include <adapters/libevent.h>

void
handle_set_event(const keyspace_notifier *n, const char *event)
{
	printf("event: %s\n", event);
	uint64_t res;
	redisReply * r = notifier_issue_redis_command(n, event);
	if (!r->str) printf("key not found: this is a disaster\n");
	char c;
	int scanned = sscanf(r->str, "%"PRIu64 "%c", &res, &c);
	freeReplyObject(r);
	if (scanned >= 1) printf("queried value: %" PRIu64 "\n", res);
	else printf("what a horrible business\n");
}

void
_connect_sync(keyspace_notifier *notifier, const char *hostname, int port)
{
	struct timeval timeout = {1, 500000}; /* 1.5 seconds */

	notifier->c = redisConnectWithTimeout(hostname, port, timeout);
	if (!notifier->c || notifier->c->err) {
		if (notifier->c) {
			printf("Connection error: %s\n", notifier->c->errstr);
			redisFree(notifier->c);
			notifier->c = NULL;
		} else {
			printf("Connection error: can't allocate redis context\n");
		}
	}
}

void
_connect_async(keyspace_notifier *notifier, const char *hostname, int port)
{
	notifier->async = redisAsyncConnect(hostname, port);
	if (notifier->async->err) {
		printf("Connection error: %s\n", notifier->async->errstr);
	}
}

int
_enable_notifications(const keyspace_notifier *notifier)
{
	redisReply *reply = redisCommand(notifier->c, "CONFIG SET notify-keyspace-events KEA");
	if (strncmp(reply->str, "OK", strlen("OK")) == 0) return 0;
	return 1;
}

void
_handle_redis_array_reply(redisReply *r, const keyspace_notifier *n)
{
	if (r->elements == 3) {
		if (strncmp(r->element[0]->str, "message", strlen("message")) == 0) {
			char *val = r->element[2]->str;
			handle_set_event(n, val);
		}
	}
}
void
on_message(redisAsyncContext *c, void *reply, void *privdata)
{
	if (!reply) return;

	redisReply *r = reply;
	keyspace_notifier *n = (keyspace_notifier *)privdata;

	if (r->type == REDIS_REPLY_ARRAY) _handle_redis_array_reply(r, n);
}

int
notifier_register_set_event(keyspace_notifier *notifier)
{
	char buf[256] = "__keyevent@0__:set";
    	
	int res = redisAsyncCommand(notifier->async, on_message, notifier, "SUBSCRIBE %s", buf);
	if (res == REDIS_ERR) {
		printf("Failed to subscribe on %s\n", buf);
		return res;
	}
	return 0;
}

int
notifier_deregister_set_event(keyspace_notifier *notifier)
{
	char buf[256] = "__keyevent@0__:set";
	int res = redisAsyncCommand(notifier->async, on_message, notifier, "UNSUBSCRIBE %s", buf);
	if(res == REDIS_ERR) {
        	printf("Failed to unsubscribe from: %s\n", buf);
    	}
	return res;
}

redisReply *
notifier_issue_redis_command(const keyspace_notifier *notifier, const char *key)
{
	char buf[256];
	snprintf(buf, 256, "GET %s", key);
	redisReply *reply = redisCommand(notifier->c, buf);
    	return reply;
}

void
disconnect_callback(const redisAsyncContext *c, int status)
{
	if (status != REDIS_OK) {
	printf("Error: %s\n", c->errstr);
	return;
	}
	printf("DISCONNECTED\n");
}

static void *
thread_start(void *t)
{
	keyspace_notifier* notifier = (keyspace_notifier*) t;
	event_base_dispatch(notifier->base);
	printf("out of loop\n");
	return t;
}

keyspace_notifier *
new_keyspace_notifier(const char *hostname, int port)
{
	signal(SIGPIPE, SIG_IGN);

	keyspace_notifier *notifier = (keyspace_notifier *)malloc(sizeof(keyspace_notifier));
	notifier->base = event_base_new();
	
	_connect_sync(notifier, hostname, port);
	_connect_async(notifier, hostname, port);
	_enable_notifications(notifier);
	redisAsyncSetDisconnectCallback(notifier->async, disconnect_callback);
	redisLibeventAttach(notifier->async, notifier->base);

	if (pthread_create(&notifier->tid, NULL, thread_start, (void *)notifier) != 0) {
		printf("error starting thread\n");
		return NULL;
	}

	return notifier;
}

void
free_keyspace_notifier(keyspace_notifier *notifier)
{
	if (!notifier) return;

	redisFree(notifier->c);
	notifier_deregister_set_event(notifier);
	free(notifier);
}