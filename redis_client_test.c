#include "hiredis/hiredis.h"
#include <stdlib.h>

int main()
{
	redisContext *c = redisConnect("localhost", 6379);
	if (c != NULL && c->err) printf("Error %s\n", c->errstr);
	else printf("connected to redis\n");

	redisReply *reply;
	reply = redisCommand(c, "set conn file1");
	printf("reply: %s\n", reply->str);
	freeReplyObject(reply);

	reply = redisCommand(c, "get conn");
	printf("reply: %s\n", reply->str);
	freeReplyObject(reply);

	redisFree(c);
}