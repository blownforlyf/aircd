#define MAX_NUM_BODY MAX_BUFFER-MAX_NICK_LENGTH-8

char numeric_message[MAX_NUM_BODY];


#define RPL_TOPIC 332
#define RPL_NAMEREPLY 353
#define RPL_ENDOFNAMES 366

#define ERR_NEEDMOREPARAMS 461
#define ERR_BANNEDFROMCHAN 474

void send_numeric_reply(struct client *send_to, int numeric, char* target, char* message);