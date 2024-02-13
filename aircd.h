#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/event.h>
#include <fcntl.h>
#include <sys/time.h>
#include <signal.h>

//openbsd
#include <sys/types.h>
#include <netinet/in.h>

//compile with -pthreads
#include "libs/conn_threads.c"

//CONFIG

#define SERVER_NAME "chatter.today"
#define MAX_NICK_LENGTH 128
#define MAX_CHAN_LENGTH 128
#define MAX_BUFFER 1024
#define MAX_TOPIC_LENGTH 512



#define MIN_MSG_DISTANCE 0
#define MAX_EVENTS 512

#define PORT 6667
#define PORT_SSL 6668
#define PORT_WS 8888
#define PORT_WSS 8889

#define USE_SSL 0
#if USE_SSL
//compile with -lssl
#define SERVER_CERT "/etc/ssl/chatter.today.crt"
#define SERVER_KEY "/etc/ssl/private/chatter.today.key"
#include "libs/ssl.c"
#endif

#define USE_WS 1
#if USE_WS
//complile with -lcrypto -lm
#include "libs/ws.c"
#endif


enum CONNECTION_TYPE {
    IRC,
    IRC_SSL,
    WS,
    WS_SSL
};

enum CONNECTION_STATE {
    PRE_WS,
    NO_DATA,
    NICK_NO_USER,
    USER_NO_NICK,
    CONNECTED
};



struct channel_member_list {
    struct client *client;
    char prefix;
    struct channel_member_list *next;
} channel_member_list;

struct channel_topic {
    char topic[MAX_BUFFER/2];
    char set_by_nick[MAX_NICK_LENGTH];
    int unix_timestamp;
};

struct channel {
    char name[MAX_CHAN_LENGTH];
    struct channel_topic *topic;
    struct channel_member_list *members;
    struct channel *next;
} channel;
struct channel *channel_list = NULL;

struct clients_channels {
    struct channel *channel;
    struct clients_channels *next;
} clients_channels;

struct client {
    int fd;
#if USE_SSL
    SSL *ssl;
#endif
    enum CONNECTION_TYPE connection_type;
    enum CONNECTION_STATE connection_state;
    char nick[MAX_NICK_LENGTH];
    char user[MAX_NICK_LENGTH];
    struct clients_channels *joined_channels;
    long long last_msg_timestamp;
    struct client *next;
} client;
struct client *client_list = NULL;


long long 
current_time_in_milliseconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)(tv.tv_sec) * 1000 + (long long)(tv.tv_usec) / 1000;
}


void write_to_client(struct client *client, char* line, int len);
void write_to_channel(struct client *client, char* channel, char *line);
void write_to_user(struct client *client, char* user, char *line);


void printStringDetails(const char* str) {
    printf("String Content:\n");
    printf("ASCII values:");
    
    for (int i = 0; str[i] != '\0'; i++) {
        printf(" %d", str[i]);
    }
    printf("\n");
    
    printf("Characters:");
    for (int i = 0; str[i] != '\0'; i++) {
        printf(" '%c'", str[i]);
    }
    printf("\n");
}

#include "numerics.h"
