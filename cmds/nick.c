void
NICK(struct client *client_cursor, char *single_line){
    //empty nick
    if(!strlen(single_line+5)){
        return;
    }


    int nick_len = strlen(single_line+5);
    if(nick_len > MAX_NICK_LENGTH-1){
        nick_len = MAX_NICK_LENGTH-1;
    }

    memcpy(client_cursor->nick,single_line+5,nick_len);
    client_cursor->nick[nick_len] = '\0';

    if(client_cursor->connection_state == NO_DATA || client_cursor->connection_state == USER_NO_NICK || client_cursor->connection_state == NICK_NO_USER || client_cursor->connection_state == CONNECTED){

        //CHECKING NICK FOR UNIQUENESS
        int unique = 1;

        do{
            if(!unique){
                sprintf(client_cursor->nick, "rand_%d", rand());
            }
            struct client *comp = client_list;
            unique = 1;
            while(comp!=NULL){
                if(comp!=client_cursor && strncmp(comp->nick, client_cursor->nick, strlen(comp->nick)) == 0){
                    unique = 0;
                    break;
                }
                comp = comp->next;
            }

        }while(!unique);

    }


    switch(client_cursor->connection_state){
        case NICK_NO_USER:
        case NO_DATA:
            client_cursor->connection_state = NICK_NO_USER;
            break;
        case USER_NO_NICK:
            client_cursor->connection_state = CONNECTED;
            break;
        case CONNECTED:
        default:
            break;
    }

}