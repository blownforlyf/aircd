#include <openssl/ssl.h>
#include <openssl/err.h>


SSL_CTX *ctx;
void
ssl_init_ctx(){

    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    
    ctx = SSL_CTX_new(TLS_server_method());
    if (ctx == NULL) {
        perror("can't create SSL_CTX_new");
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_certificate_file(ctx, SERVER_CERT, SSL_FILETYPE_PEM) <= 0) {
        perror("can't read ssl certificate");
        exit(EXIT_FAILURE);
    }
    if (SSL_CTX_use_PrivateKey_file(ctx, SERVER_KEY, SSL_FILETYPE_PEM) <= 0) {
        perror("can't read ssl private key");
        exit(EXIT_FAILURE);
    }

}


int
ssl_init_socket(SSL **ssl, int fd){

    *ssl = SSL_new(ctx);

    if (*ssl == NULL) {
        perror("error on SSL_new");
        return 1;
    }

    SSL_set_fd(*ssl, fd);

    int ssl_result;
    int i=0;
    while((ssl_result = SSL_accept(*ssl)) != 1){
        int ssl_error = SSL_get_error(*ssl, ssl_result);

        if(i>5000){
            perror("aborting handshake after 5k tries");
            SSL_free(*ssl);
            return 1;
        }
        
        i++;

        //retry until ready
        if((ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE)){

            usleep(1000);
            continue;    
        }
        else{
            perror("unknown ssl error");
            SSL_free(*ssl);
            return 1;
        }

    }

    return 0;
}
