#include <ctype.h>


struct channel*
new_channel(char *name, struct client *founder){

printf("create new channel: %s\n",name);

    struct channel_member_list *member_list = malloc(sizeof(struct channel_member_list));
    member_list->client = founder;
    member_list->prefix = '*';
    member_list->next = NULL;

printf("here\n");

    struct channel *new_chan = malloc(sizeof(struct channel));
    strncpy(new_chan->name,name,MAX_CHAN_LENGTH);
    new_chan->name[strlen(name)] = '\0';

    new_chan->members = member_list;

printf("here 2\n");
    new_chan->topic = malloc(sizeof(struct channel_topic));
    new_chan->topic->topic[0] = 0;
    new_chan->topic->set_by_nick[0] = 0;
    new_chan->topic->unix_timestamp = 0;

printf("here 3\n");    
    new_chan->next = channel_list;
    channel_list = new_chan;

    return new_chan;
}

void 
user_join_channel(char *channel_name, struct client *joiner){

    //try find channel in list
    struct channel *channel = channel_list;
    while(channel!=NULL && (strlen(channel->name)!=strlen(channel_name) || strncmp(channel->name,channel_name,strlen(channel_name))!=0)){
        channel = channel->next;
    }
printf("woops\n");
    //none found, open new channel
    if(channel==NULL){
        channel = new_channel(channel_name,joiner);
    }
    else{
        //we found one, add client to member list
        struct channel_member_list *member_list = malloc(sizeof(struct channel_member_list));
        member_list->client = joiner;
        member_list->prefix = '\0';
        member_list->next = channel->members;
        channel->members = member_list;
    }

    //add to clients channels list
    if(joiner->joined_channels == NULL){
        joiner->joined_channels = malloc(sizeof(struct clients_channels));
        joiner->joined_channels->channel = channel;
        joiner->joined_channels->next = NULL;
    }
    else{
        struct clients_channels *new_chan = malloc(sizeof(struct clients_channels));
        new_chan->channel = channel;
        new_chan->next = joiner->joined_channels;
        joiner->joined_channels = new_chan;
    }

printf("before join msg\n");
    //JOIN MESSAGE TELL EVERYONE IN CHANNEL SOMEONE JOINED
    char join_msg[MAX_BUFFER];
    struct channel_member_list *member_list = channel->members;
    sprintf(join_msg,":%s JOIN :%s\n", joiner->user, channel_name);
    while(member_list != NULL){
        write_to_client(member_list->client, join_msg, strlen(join_msg));
        member_list = member_list->next;
    }

printf("before topikke\n");
    //NUMERIC REPLY THE TOPIKKE
    if(strlen(channel->topic->topic)){
        sprintf(numeric_message, "%s :%s", channel->name, channel->topic->topic);
        send_numeric_reply(joiner, RPL_TOPIC, joiner->user, numeric_message);
    }

printf("before member list\n");
    //NUMERIC REPLY THE MEMBER LIST
    member_list = channel->members;
    sprintf(numeric_message, "= %s :", channel->name);
    while(member_list != NULL){
        if(member_list->prefix != '\0'){
            snprintf(numeric_message+strlen(numeric_message), MAX_NUM_BODY, "%c%s ", member_list->prefix, member_list->client->nick);
        }
        else{
            snprintf(numeric_message+strlen(numeric_message), MAX_NUM_BODY, "%s ", member_list->client->nick);
        }
        
        member_list = member_list->next;

        //send the full batch
        if(member_list!=NULL && (strlen(member_list->client->nick)+1) > (MAX_NUM_BODY-strlen(numeric_message))){
printf("before send\n");
            send_numeric_reply(joiner, RPL_NAMEREPLY, joiner->user, numeric_message);
            sprintf(numeric_message, "= %s :", channel->name);
        }
        
    }
    if(strlen(numeric_message)>strlen(channel->name)+4){
        send_numeric_reply(joiner, RPL_NAMEREPLY, joiner->user, numeric_message);
    }
printf("final sprint\n");
    sprintf(numeric_message, "%s :end of names", channel->name);
    send_numeric_reply(joiner, RPL_ENDOFNAMES, joiner->user, numeric_message);
    

}


void
JOIN(struct client *client_cursor, char *line){

    char channel[MAX_CHAN_LENGTH];
    char *first_space = strchr(line, ' ');
    char *second_space = strchr(first_space+1, ' ');
    if(second_space!=NULL){
        return;
    }

    strncpy(channel, first_space+1, strlen(line)-(first_space-line));
    channel[strlen(line)-(first_space-line)] = '\0';

printf("wanna join channel :%s len %lu\n",channel,strlen(channel));

    if(channel[0] != '#' || strlen(channel)<2){
printf("ret 1\n");
        return;
    }

    for(int i=0; i<strlen(channel);i++){
printf(">%c< ",channel[i]);
        if(isspace(channel[i])){
printf("ret 2 \n");
            return;
        }
    }

    user_join_channel(channel, client_cursor);

} 