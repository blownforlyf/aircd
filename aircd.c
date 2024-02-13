#include "aircd.h"
#include "cmds/ping.c"
#include "cmds/join.c"
#include "cmds/nick.c"
#include "cmds/user.c"
#include "cmds/privmsg.c"



struct kevent event;
int kq;

char buffer[MAX_BUFFER];
char single_line[MAX_BUFFER];
char out_buffer[MAX_BUFFER];



void
send_numeric_reply(struct client *send_to, int numeric, char* target, char* message){
    char out_buffer[MAX_BUFFER];
    snprintf(out_buffer, MAX_BUFFER, ":%s %d %s %s\r\n", SERVER_NAME, numeric, target, message);
    write_to_client(send_to,out_buffer,strlen(out_buffer));

}




void
remove_connection(int fd){

    EV_SET(&event, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    int result = kevent(kq, &event, 1, NULL, 0, NULL);
    if (result == -1) {
        perror("kevent error removing fd");
    }


    struct client *cur = client_list;
    struct client *prev = NULL;
    while(cur->fd != fd && cur != NULL){
        prev = cur;
        cur = cur->next;
    }

    if(cur == NULL){
        perror("coudn't find client to remove in list");
        return;
    }

#if USE_SSL
    if(cur->ssl != NULL){
        SSL_shutdown(cur->ssl);
        SSL_free(cur->ssl);
    }    
#endif

    close(cur->fd);

    //REMOVE USER FROM ANY JOINED CHANNELS
    struct channel_member_list *cml, *cmlp;
    cmlp = NULL;
    while(cur->joined_channels != NULL){
        cml = cur->joined_channels->channel->members;
        while(cml!=NULL && cml->client!=cur){
            cmlp = cml;
            cml = cml->next;
        }
        if(cmlp==NULL){
            cur->joined_channels->channel->members = cml->next;
        }
        else{
            cmlp->next = cml->next;
        }
        free(cml);
        cur->joined_channels = cur->joined_channels->next;
    }

    //we hit first element in client_list
    if(prev==NULL){
        client_list = cur->next;
    }
    else{
        prev->next = cur->next;
    }
    free(cur);
    
}

void
__new_connection(int client_socket, int ssl, int ws){


    int flags = fcntl(client_socket, F_GETFL, 0);
    fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);

    struct client* new = malloc(sizeof(client));
    new->fd = client_socket;
    new->connection_type = IRC;
    new->connection_state = NO_DATA;

    new->joined_channels = NULL;
    
    printf("client socket %d\n", client_socket);
#if USE_SSL
    new->ssl = NULL;

    if(ssl){
        printf("new irc ssl conn\n");
        new->connection_type = IRC_SSL;

        if(ssl_init_socket(&new->ssl,new->fd)){
            perror("Error initiating SSL for socket");
            close(client_socket);
            free(new);
            return;
        }
    }
#endif


#if USE_WS
    if(ws){

        if(ssl){
            new->connection_type = WS_SSL;
        }
        else{
            new->connection_type = WS;
        }

        new->connection_state = PRE_WS;
    }
#endif


    EV_SET(&event, client_socket, EVFILT_READ, EV_ADD, 0, 0, NULL);
    if(kevent(kq, &event, 1, NULL, 0, NULL) == -1){
        perror("Error registering client socket with kqueue");
        close(client_socket);
        free(new);
        return;
    }
    

    new->nick[0] = 0;
    new->user[0] = 0;
    new->last_msg_timestamp = 0;
    new->next = client_list;
    client_list = new;

/*
    char *welcome = "001 user :hi";
    ssize_t sent = write(client_socket,welcome,strlen(welcome));
*/

}

void
new_irc_connection(int client_socket){
    __new_connection(client_socket, 0, 0);
}

void
new_irc_ssl_connection(int client_socket){
    __new_connection(client_socket, 1, 0);
}

void
new_irc_ws_connection(int client_socket){
    __new_connection(client_socket, 0, 1);
}

void
new_irc_ws_ssl_connection(int client_socket){
    __new_connection(client_socket, 1, 1);
}



int 
can_send(int fd){
    struct client *cur = client_list;
    struct client *prev = NULL;
    while(cur->fd != fd && cur != NULL){
        prev = cur;
        cur = cur->next;
    }
    
    if(current_time_in_milliseconds()<(cur->last_msg_timestamp+MIN_MSG_DISTANCE)){
        return 0;
    }
    cur->last_msg_timestamp = current_time_in_milliseconds();
    return 1;
}

/*
returns length of out_buffer
*/
int
get_nth_line(char *in_buffer, char *out_buffer, int in_len, int nth_line){

    char *ret = NULL;

    int line_count = 0;
    int i;
    int j = 0;
    for(i=0; i<in_len;i++){
        out_buffer[j] = in_buffer[i];
        j++;

        if(in_buffer[i] == '\n'){
            out_buffer[j-1] = 0;
            if(j >=2 && out_buffer[j-2] == '\r'){
                j--;
                out_buffer[j-1] = 0;
            }

            //we got our target line
            if(line_count == nth_line){
                return j;
            }

            //we dont we try next line
            line_count++;
            j = 0;
        }
    }

    //nicht genug \n gefunden für line count requirements oben return
    //dh. bei nth_line 0 ... KEIN \n gefunden aber ne line wohl ...

    if(nth_line == line_count){
        out_buffer[j] = '\0';
        return j;
    }
    else{
        return 0;
    }
}

int
create_server_socket(int port){

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Set SO_REUSEADDR option
    int reuse = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        perror("Error setting SO_REUSEADDR option");
        exit(EXIT_FAILURE);
    }

    bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_socket, SOMAXCONN);
/*
    int flags = fcntl(server_socket, F_GETFL, 0);
    fcntl(server_socket, F_SETFL, flags | O_NONBLOCK);
*/  
    //add server socket to kqueue

    EV_SET(&event, server_socket, EVFILT_READ, EV_ADD, 0, 0, NULL);
    if(kevent(kq, &event, 1, NULL, 0, NULL) == -1){
        perror("Error registering server socket with kqueue");
        close(server_socket);
        close(kq);
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d\n", port);

    return server_socket;
}


char ws_buffer[1300];
void
write_to_client(struct client *client, char* line, int len){

printf("wanna write to client %d %d len\n",client->fd,len);

    switch(client->connection_type){
        case IRC:
            write(client->fd, line, len);
            break;
#if USE_SSL
        case IRC_SSL:
            SSL_write(client->ssl, line, len);
            break;
#endif
#if USE_WS
        case WS:
            len = ws_make_out_msg(line, ws_buffer);
            write(client->fd, ws_buffer, len);
            break;
#if USE_SSL
        case WS_SSL:
            len = ws_make_out_msg(line, ws_buffer);
            SSL_write(client->ssl, ws_buffer, len);
            break;
#endif
#endif
        default:
            break;
    }
printf("wrote %s\n",line);
}


int 
main(){

    srand(time(NULL));

    // Ignore SIGPIPE signal
    signal(SIGPIPE, SIG_IGN);

    //Thread pool
    ThreadPool t_pool;
    initializeThreadPool(&t_pool);

    kq = kqueue();

    int irc_socket = create_server_socket(6667);
    printf("fd %d\n",irc_socket);
#if USE_SSL
    ssl_init_ctx();
    int irc_ssl_socket = create_server_socket(6668);
    printf("fd %d\n",irc_ssl_socket);
#endif
#if USE_WS
    int ws_socket = create_server_socket(8888);
    printf("fd %d\n",ws_socket);
#if USE_SSL
    int ws_ssl_socket = create_server_socket(8889);
    printf("fd %d\n",ws_ssl_socket);
#endif
#endif

    int fd;

    struct kevent events[MAX_EVENTS];

    int num_events;
    ssize_t bytes_read;

    int line_read;

    struct client *client_cursor;

    for(;;){

        num_events = kevent(kq, NULL, 0, events, MAX_EVENTS, NULL);


        for(int i = 0; i < num_events; i++){
            fd = (int)events[i].ident;
printf("action on fd %d\n",fd);
            //new connections
            if(fd == irc_socket 
#if USE_SSL
            || fd == irc_ssl_socket 
#endif
#if USE_WS
            || fd == ws_socket 
#if USE_SSL
            || fd == ws_ssl_socket
#endif
#endif
            ){
                int client_socket = accept(fd, NULL, NULL);
                printf("accptd new connectino on fd %d\n",fd);
                if(client_socket == -1){
                    perror("Error accepting connection");
                }
                else if(fd == irc_socket){
                    printf("new plain irc conn\n");
                    submitTask(&t_pool, &new_irc_connection, client_socket);
                }
#if USE_SSL
                else if(fd == irc_ssl_socket){
                    printf("new ssl irc conn\n");
                    submitTask(&t_pool, &new_irc_ssl_connection, client_socket);
                }
#endif
#if USE_WS
                else if(fd == ws_socket){
                    printf("new plain ws irc conn\n");
                    submitTask(&t_pool, &new_irc_ws_connection, client_socket);
                }
#if USE_SSL
                else if(fd == ws_ssl_socket){
                    printf("new ssl ws irc conn %d\n", client_socket);
                    submitTask(&t_pool, &new_irc_ws_ssl_connection, client_socket);
                }
#endif
#endif
                continue;
            }
printf("GETTING SENDING CLIENT\n");
            //get sending client
            client_cursor = client_list;
            while(client_cursor != NULL && client_cursor->fd != fd){
                client_cursor = client_cursor->next;
            }
            if(client_cursor==NULL){
                continue;
            }
printf("GOT GETTING SENDING CLIENT\n");
#if USE_SSL
            if(client_cursor->ssl != NULL){
                bytes_read = SSL_read(client_cursor->ssl,buffer,MAX_BUFFER);
            }
            else{
                bytes_read = read(client_cursor->fd,buffer,MAX_BUFFER);
            }
#else
            bytes_read = read(fd,buffer,MAX_BUFFER);
#endif
            if(!bytes_read){
                printf("removing user fd %d\n",fd);
                remove_connection(fd);
                continue;
            };
printf("bytes read %ld\n", bytes_read);
            buffer[bytes_read] = '\0';
            
//printf("raw read in >>%s<<\n",buffer);

#if USE_WS
            //ws handshake
            if(client_cursor->connection_state == PRE_WS){
                int hs_res = 0;
                printf("try ws hs\n");
#if USE_SSL
                if(client_cursor->ssl != NULL){
                    hs_res = ws_handle_handshake(0, client_cursor->ssl, buffer);
                }
#else
                hs_res = ws_handle_handshake(client_cursor->fd, NULL, buffer);
#endif
                if(hs_res){
                    printf("ws hs success\n");
                    client_cursor->connection_state = NO_DATA;
                }

                continue;
            }
#endif


#if USE_WS
        if(client_cursor->connection_type == WS || client_cursor->connection_type == WS_SSL){
            bytes_read = ws_parse_frame(buffer,ws_buffer,MAX_BUFFER);
            printf("n ws bytes read :%ld\n",bytes_read);
            strncpy(buffer,ws_buffer,bytes_read);
            buffer[bytes_read] = '\0';
        }
#endif



/*
TODO: check this if it blocks connections

            //find out if can send (time lock)
            if(!can_send(fd)){
                printf("spam dropped\n");
                continue;
                
            }
*/


            line_read = 0;
            while(get_nth_line(buffer,single_line,(int)bytes_read,line_read)){
                line_read++;
printStringDetails(single_line);
                printf("... %s ...\n",single_line);

                //IRC STUFF
                if(strncasecmp("CAP LS 302", single_line, strlen("CAP LS 302"))==0){
                    char *cap = "CAP * LS :JOIN PRIVMSG PING\n";
                    write_to_client(client_cursor, cap, strlen(cap));
                    continue;
                }

                if(strncasecmp("NICK ",single_line,strlen("NICK "))==0){
                    NICK(client_cursor, single_line);
                    continue;
                }

                if(strncasecmp("USER ",single_line,strlen("USER "))==0 && client_cursor->nick[0] != 0){
                    USER(client_cursor, out_buffer);
                    continue;
                }

                if(strncasecmp("QUIT",single_line,strlen("QUIT"))==0){
                    remove_connection(client_cursor->fd);
                    break;
                }

                if(strncasecmp("PING",single_line,strlen("PING"))==0){
                    PING(client_cursor, out_buffer, single_line);
                    continue;
                }

                if(client_cursor->connection_state != CONNECTED){
                    continue;
                }

                if(strncasecmp("JOIN ",single_line,strlen("JOIN "))==0){ 
                    JOIN(client_cursor, single_line);
                    continue;
                }

                if(strncasecmp("PRIVMSG ",single_line,strlen("PRIVMSG "))==0){
                    PRIVMSG(client_cursor, single_line);
                    continue;
                }
                

            }






/*

send to all clients

            struct client *cur = client_list;
            while(cur != NULL){
                write(cur->fd, buffer, strlen(buffer));
                cur = cur->next;
            }
*/


        }
    }



}
