void
USER(struct client *client_cursor, char *out_buff){

    if(client_cursor->connection_state == CONNECTED){
        return;
    }


    switch(client_cursor->connection_state){
        case NO_DATA:
            client_cursor->connection_state = USER_NO_NICK;
            break;
        case NICK_NO_USER:
            client_cursor->connection_state = CONNECTED;
            break;
        case CONNECTED:
        default:
            break;
    }
    
    sprintf(client_cursor->user, "%s!~rand%d@inet", client_cursor->nick, rand());

    //this happens only once, send connected msg
    sprintf(out_buff, ":%s 001 %s :welcome to the network %s\n", SERVER_NAME, client_cursor->nick, client_cursor->user);

    write_to_client(client_cursor, out_buff, strlen(out_buff));
}