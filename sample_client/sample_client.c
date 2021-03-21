#include "keyspacenotification.h"

int
main(void)
{
	char input[48];

	keyspace_notifier *notifier = new_keyspace_notifier("127.0.0.1", 6379);notifier_register_set_event(notifier);

	 printf("Listening on set event\n");
	printf("Use Redis-Cli to issue some commands\n");
	printf("Press any key to quite\n");
	scanf ("%s", input);
	printf("quiting\n");

	free_keyspace_notifier(notifier);

	return 0;
}