void 
privmsg_to_channel(struct client *client, char *channel, char *line){

printf("priv to chan\n");
    struct channel *c_cursor = channel_list;
    if(c_cursor== NULL){
        return;
    }
    
    int match = 0;
    do{
printf("this channel name: ...%s...\n",c_cursor->name);
        if(strlen(c_cursor->name)==strlen(channel) && strncmp(c_cursor->name,channel,strlen(channel))==0){
            match = 1;
            break;
        }
        c_cursor = c_cursor->next;
    }while(c_cursor != NULL);

    if(!match){
        printf("no channel match for channel %s\n",channel);
        return;
    }

    char out_buffer[MAX_BUFFER];

    sprintf(out_buffer, ":%s PRIVMSG %s :%s\r\n", client->user, channel, line);

    struct channel_member_list *m_c = c_cursor->members;

    while(m_c!=NULL){
        write_to_client(m_c->client,out_buffer,strlen(out_buffer));
        m_c = m_c->next;
    }

}


void 
privmsg_to_user(struct client *client, char *nick, char *line){

    struct client *c_cursor = client_list;
    int match = 0;
    do{
        if(strlen(c_cursor->nick)==strlen(nick) && strncmp(c_cursor->nick,nick,strlen(nick))==0){
            match = 1;
            break;
        }
        c_cursor = c_cursor->next;
    }while(c_cursor != NULL);

    if(!match){
        return;
    }

    char out_buffer[MAX_BUFFER];

    sprintf(out_buffer, ":%s PRIVMSG %s :%s\r\n", client->user, c_cursor->user, line);

    write_to_client(c_cursor,out_buffer,strlen(out_buffer));

}

void
PRIVMSG(struct client *client_cursor, char *line){
    // Find the positions of the first and second space  in the text
    //PRIVMSG uwe :hihoho
    const char *first_space = strchr(line, ' ');
    const char *second_space = strchr(first_space + 1, ' ');

    if(first_space==NULL && second_space==NULL){
        return;
    }

    char nick[MAX_BUFFER];
    char target[MAX_BUFFER];
    char msg[MAX_BUFFER];

    strncpy(nick, line, first_space-line);
    strncpy(target, first_space+1, second_space-(first_space+1));
    strncpy(msg, second_space+1, strlen(second_space+1));
    nick[first_space-line] = '\0';
    target[second_space-(first_space+1)] = '\0';
    msg[strlen(second_space+1)] = '\0';

    if(!strlen(nick) || !strlen(target) || !strlen(msg)){
        return;
    }

    if(target[0] == '#'){
        if(strlen(target)<=1){
            return;
        }
        privmsg_to_channel(client_cursor, target, msg);
    }
    else{
        privmsg_to_user(client_cursor, target, msg);
    }


}
