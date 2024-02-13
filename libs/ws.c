#include <openssl/sha.h>
#include <regex.h>
#include <math.h>
#include "b64.c"


//complile with -lcrypto -lm


/*


    0               1               2               3
    0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
    +-+-+-+-+-------+-+-------------+-------------------------------+
    |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
    |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
    |N|V|V|V|       |S|             |   (if payload len==126/127)   |
    | |1|2|3|       |K|             |                               |
    +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
    |     Extended payload length continued, if payload len == 127  |
    + - - - - - - - - - - - - - - - +-------------------------------+
    |                               |Masking-key, if MASK set to 1  |
    +-------------------------------+-------------------------------+
    | Masking-key (continued)       |          Payload Data         |
    +-------------------------------- - - - - - - - - - - - - - - - +
    :                     Payload Data continued ...                :
    + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
    |                     Payload Data continued ...                |
    +---------------------------------------------------------------+


   FIN:  1 bit

      Indicates that this is the final fragment in a message.  The first
      fragment MAY also be the final fragment.

   RSV1, RSV2, RSV3:  1 bit each

      MUST be 0 unless an extension is negotiated that defines meanings
      for non-zero values.  If a nonzero value is received and none of
      the negotiated extensions defines the meaning of such a nonzero
      value, the receiving endpoint MUST _Fail the WebSocket
      Connection_.

   Opcode:  4 bits

      Defines the interpretation of the "Payload data".  If an unknown
      opcode is received, the receiving endpoint MUST _Fail the
      WebSocket Connection_.  The following values are defined.

      *  %x0 denotes a continuation frame

      *  %x1 denotes a text frame

      *  %x2 denotes a binary frame

      *  %x3-7 are reserved for further non-control frames

      *  %x8 denotes a connection close

      *  %x9 denotes a ping

      *  %xA denotes a pong

      *  %xB-F are reserved for further control frames

   Mask:  1 bit

      Defines whether the "Payload data" is masked.  If set to 1, a
      masking key is present in masking-key, and this is used to unmask
      the "Payload data" as per Section 5.3.  All frames sent from
      client to server have this bit set to 1.

   Payload length:  7 bits, 7+16 bits, or 7+64 bits

      The length of the "Payload data", in bytes: if 0-125, that is the
      payload length.  If 126, the following 2 bytes interpreted as a
      16-bit unsigned integer are the payload length.  If 127, the
      following 8 bytes interpreted as a 64-bit unsigned integer (the
      most significant bit MUST be 0) are the payload length.  Multibyte
      length quantities are expressed in network byte order.  Note that
      in all cases, the minimal number of bytes MUST be used to encode
      the length, for example, the length of a 124-byte-long string
      can't be encoded as the sequence 126, 0, 124.  The payload length
      is the length of the "Extension data" + the length of the
      "Application data".  The length of the "Extension data" may be
      zero, in which case the payload length is the length of the
      "Application data".
  Masking-key:  0 or 4 bytes

      All frames sent from the client to the server are masked by a
      32-bit value that is contained within the frame.  This field is
      present if the mask bit is set to 1 and is absent if the mask bit
      is set to 0.  See Section 5.3 for further information on client-
      to-server masking.

   Payload data:  (x+y) bytes

      The "Payload data" is defined as "Extension data" concatenated
      with "Application data".

   Extension data:  x bytes

      The "Extension data" is 0 bytes unless an extension has been
      negotiated.  Any extension MUST specify the length of the
      "Extension data", or how that length may be calculated, and how
      the extension use MUST be negotiated during the opening handshake.
      If present, the "Extension data" is included in the total payload
      length.

   Application data:  y bytes

      Arbitrary "Application data", taking up the remainder of the frame
      after any "Extension data".  The length of the "Application data"
      is equal to the payload length minus the length of the "Extension
      data".


*/




int
ws_make_out_msg(unsigned char *msg, unsigned char *out_msg){
    size_t msg_len = strlen(msg);

printf("outmsg len %ld\n",msg_len);

    *(out_msg) = 0b10000001;
    *(out_msg+1) = 0;

    int msg_start = 2;


    if(msg_len<126){
        *(out_msg+1) = (unsigned char) (127 & msg_len);
    }
    else if(msg_len>(pow(2,7)) && msg_len<(pow(2,16))){
        *(out_msg+1) = (unsigned char) 126;
        

//ENDIANESS!!!!!!!!!!!!!!!!!
        *(out_msg+2) = *((unsigned char*)(&msg_len)+1);
        *(out_msg+3) = *((unsigned char*)(&msg_len));

        msg_start += 2;
    }
    else if(msg_len >= (pow(2,16))){
        *(out_msg+1) = (unsigned char) 127;
        memcpy(out_msg+2, &msg_len, 8);
        msg_start += 8;
    }

    int i;
    for(i=0; i<msg_len; i++){
        *(out_msg+i+msg_start) = *(msg+i);
    }
    *(out_msg+i+msg_start) = 0;
    *(out_msg+1) = *(out_msg+1) & 0b01111111;

    return i+msg_start;
    
}


int
ws_parse_frame(unsigned char *raw_buffer, unsigned char *out_buff, int max_out_payload){

    int opcode = raw_buffer[0] & 0b00001111;

    if(opcode != 1){
        return 0;
    }

    int have_mask = raw_buffer[1] & 0b10000000;
    unsigned char mask[4] = {0,0,0,0};

    int raw_data_start = 2;

    unsigned long int payload_len = raw_buffer[1] & 0b01111111;


    if(payload_len < 126){
        raw_data_start = 2;
        if(have_mask){
            memcpy(mask, (raw_buffer+2), 4);
            raw_data_start += 4;
        }
        
    }
    else if(payload_len == 126){
printf("payload len case 2\n");
        
        payload_len = (raw_buffer[2]<<8) + raw_buffer[3];


        raw_data_start = 4;
        if(have_mask){
            memcpy(mask, (raw_buffer+4), 4);
            raw_data_start += 4;
        }
    }
    else if(payload_len == 127){
printf("payload len case 3\n");

        payload_len = ((unsigned long int)raw_buffer[2]<<56) + ((unsigned long int)raw_buffer[3]<<48) + ((unsigned long int)raw_buffer[4]<<40) + ((unsigned long int)raw_buffer[5]<<32) + (raw_buffer[6]<<24) + (raw_buffer[7]<<16) + (raw_buffer[8]<<8) + raw_buffer[9];


        raw_data_start = 10;
        if(have_mask){
            memcpy(mask, (raw_buffer+10), 4);
            raw_data_start += 4;
        }
    }

    printf("payload inc %ld\n",payload_len);

    int i;
    for(i=0; i<payload_len && i<max_out_payload; i++){
        out_buff[i] = raw_buffer[i+raw_data_start] ^ mask[i%4];
    }
    
    return i;


}


int 
__ws_hash(unsigned char *in_string, unsigned char *out_string){

    //magic ws guid
	char *GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	size_t joined_len = strlen(GUID) + strlen(in_string);

	unsigned char joined_str[joined_len+1];
	snprintf(joined_str, joined_len+1, "%s%s", in_string, GUID);

	unsigned char sha_bytes[SHA_DIGEST_LENGTH];
	SHA1(joined_str,joined_len,sha_bytes);

	size_t b64_len;
	unsigned char *b64_string = base64_encode(sha_bytes,SHA_DIGEST_LENGTH,&b64_len);

    memcpy(out_string, b64_string, b64_len);
    out_string[b64_len] = 0;

	return b64_len;
}


void 
__ws_write_header(int fd, void *ssl, unsigned char *hash){
	
	unsigned char *header_begin =
"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ";

    int len = strlen(header_begin)+strlen(hash)+8;
    unsigned char joined_str[len];
	snprintf(joined_str, len, "%s%s\r\n\r\n",header_begin,hash);

    if(fd>0){
        write(fd, joined_str, strlen(joined_str));
    }
#if USE_SSL
    else{
        SSL_write(ssl, joined_str, strlen(joined_str));
    }
#endif
}



int 
ws_handle_handshake(int fd, void *ssl, char* header){
	regex_t regex;
	int reti;
	regmatch_t groupArr[2];
	char *regexHeader = "Sec-WebSocket-Key: ([^\r^\n.]*)";
	reti = regcomp(&regex, regexHeader, REG_EXTENDED);
	if(reti){
		puts("couldn't compile regex");
	}
	reti = regexec(&regex, header, 2, groupArr,0);


    


	if(!reti){

        int len_header = strlen(header);
		unsigned char sourceCopy[len_header];
		strlcpy(sourceCopy, header, len_header); 
		sourceCopy[groupArr[1].rm_eo] = 0;
		unsigned char res[(groupArr[1].rm_eo-groupArr[1].rm_so)+1];
		strlcpy(res, sourceCopy+groupArr[1].rm_so, (groupArr[1].rm_eo-groupArr[1].rm_so)+1);
		
		unsigned char res_handshake_b64[MAX_BUFFER];
		__ws_hash(res,res_handshake_b64);	
		
        if(fd>0){
            __ws_write_header(fd, NULL, res_handshake_b64);
        }
        else{
            __ws_write_header(0, ssl, res_handshake_b64);
        }
		
		return 1;	
	}
	return 0;
}
