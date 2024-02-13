void
PING(struct client *client_cursor, char *out_buffer, char *single_line){
    sprintf(out_buffer, ":%s PONG %s :%s\n",SERVER_NAME,SERVER_NAME,single_line+5);
    write_to_client(client_cursor,out_buffer,strlen(out_buffer));
}