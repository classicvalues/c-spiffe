/**
 *
 * (C) Copyright 2020-2021 Hewlett Packard Enterprise Development LP
 *
 *
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at
 *
 *
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 *
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 */

#include "c-spiffe/spiffetls/listen.h"
#include "c-spiffe/spiffetls/mode.h"
#include "c-spiffe/spiffetls/option.h"
#include "c-spiffe/spiffetls/tlsconfig/config.h"
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

static int createSocket(in_port_t port)
{
    struct sockaddr_in address = { .sin_family = AF_INET,
                                   .sin_addr.s_addr = htonl(INADDR_ANY),
                                   .sin_port = htons(port) };

    const int sockfd = socket(/*IPv4*/ AF_INET, /*TCP*/ SOCK_STREAM, /*IP*/ 0);
    if(sockfd < 0) {
        // could not create socket
        return -1;
    }

    const int opt = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                  sizeof opt)
       < 0) {
        // could not set socket option
        return -1;
    }

    const int bind_ret
        = bind(sockfd, (const struct sockaddr *) &address, sizeof address);
    if(bind_ret < 0) {
        // could not bind socket
        return -1;
    }

    const int listen_ret = listen(sockfd, /*backlog*/ 1);
    if(listen_ret < 0) {
        // could not listen from socket
        return -1;
    }

    return sockfd;
}

static SSL_CTX *createTLSContext()
{
    const SSL_METHOD *method = TLS_method();
    SSL_CTX *ctx = SSL_CTX_new(method);

    return ctx;
}

SSL *spiffetls_ListenWithMode(in_port_t port, spiffetls_ListenMode *mode,
                              spiffetls_listenConfig *config, int *sock,
                              err_t *err)
{
    if(!mode->unneeded_source) {
        workloadapi_X509Source *source = mode->source;
        if(!source) {
            source = workloadapi_NewX509Source(NULL, err);
            if(*err) {
                *err = ERR_CREATE;
                goto error;
            }

            /**err = workloadapi_X509Source_Start(source);
            if(*err) {
                // could not start source
                *err = ERROR1;
                goto error;
            }*/
        }
        mode->source = source;
        mode->bundle = x509bundle_SourceFromSource(source);
        mode->svid = x509svid_SourceFromSource(source);
    }

    SSL_CTX *tls_config
        = config->base_TLS_conf ? config->base_TLS_conf : createTLSContext();
    if(!tls_config) {
        *err = ERR_CREATE;
        goto error;
    }

    switch(mode->mode) {
    case TLS_SERVER_MODE:
        tlsconfig_HookTLSServerConfig(tls_config, mode->svid, NULL);
        break;
    case MTLS_SERVER_MODE:
        tlsconfig_HookMTLSServerConfig(tls_config, mode->svid, mode->bundle,
                                       mode->authorizer, NULL);
        break;
    case MTLS_WEBSERVER_MODE:
    default:
        // unknown mode
        *err = ERR_UNKNOWN_MODE;
        goto error;
    }

    const int sockfd
        = config->listener_fd > 0 ? config->listener_fd : createSocket(port);
    if(sockfd < 0) {
        // could not create socket with given address and port
        *err = ERR_CREATE;
        goto error;
    }

    struct sockaddr_in addr_tmp;
    socklen_t len;
    const int clientfd = accept(sockfd, (struct sockaddr *) &addr_tmp, &len);
    if(clientfd < 0) {
        // could not accept client
        close(sockfd);
        *err = ERR_NOT_ACCEPTED;
        goto error;
    }
    *sock = sockfd;

    SSL *conn = SSL_new(tls_config);
    if(!conn) {
        *err = ERR_CONNECT;
        goto error;
    } else if(SSL_set_fd(conn, clientfd) != 1) {
        *err = ERR_CONNECT;
        goto error;
    } else if(SSL_set_num_tickets(conn, 0) != 1) {
        *err = ERR_SET;
        goto error;
    }

    SSL_set_accept_state(conn);
    if(SSL_accept(conn) != 1) {
        // could not build a SSL session
        SSL_shutdown(conn);
        SSL_free(conn);
        close(clientfd);
        close(sockfd);
        *err = ERR_CONNECT;
        goto error;
    }
    // successful handshake
    return conn;

error:
    return NULL;
}

SSL *spiffetls_PollWithMode(in_port_t port, spiffetls_ListenMode *mode,
                            spiffetls_listenConfig *config, int *sock,
                            int control_sock, int timeout, err_t *err)
{
    if(!mode->unneeded_source) {
        workloadapi_X509Source *source = mode->source;
        if(!source) {
            source = workloadapi_NewX509Source(NULL, err);
            if(*err) {
                *err = ERR_CREATE;
                goto error;
            }
        }
        mode->source = source;
        mode->bundle = x509bundle_SourceFromSource(source);
        mode->svid = x509svid_SourceFromSource(source);
    }

    SSL_CTX *tls_config
        = config->base_TLS_conf ? config->base_TLS_conf : createTLSContext();
    if(!tls_config) {
        *err = ERR_CREATE;
        goto error;
    }

    switch(mode->mode) {
    case TLS_SERVER_MODE:
        tlsconfig_HookTLSServerConfig(tls_config, mode->svid, NULL);
        break;
    case MTLS_SERVER_MODE:
        tlsconfig_HookMTLSServerConfig(tls_config, mode->svid, mode->bundle,
                                       mode->authorizer, NULL);
        break;
    case MTLS_WEBSERVER_MODE:
    default:
        // unknown mode
        *err = ERR_UNKNOWN_MODE;
        goto error;
    }

    const int sockfd
        = config->listener_fd > 0 ? config->listener_fd : createSocket(port);
    if(sockfd < 0) {
        // could not create socket with given address and port
        *err = ERR_CREATE;
        goto error;
    }

    int nfds = 2;
    struct pollfd pfds[2];

    pfds[0].fd = control_sock;
    pfds[0].events = POLLIN;
    pfds[0].revents = 0;

    pfds[1].fd = sockfd;
    pfds[1].events = POLLIN;
    pfds[1].revents = 0;

    while(!poll(pfds, nfds, timeout))
        ; // on 0, timeout has exceeded
    char buff[20];
    if(pfds[0].revents & POLLIN) { // received close signal

        int r_res = read(control_sock, buff, 6);
        close(sockfd);
        *err = NO_ERROR;
        goto error;
    } else if(!(pfds[1].revents & POLLIN)) {
        /// error? //shouldn't happen
    }

    struct sockaddr_in addr_tmp;
    socklen_t len;
    const int clientfd = accept(sockfd, (struct sockaddr *) &addr_tmp, &len);
    if(clientfd < 0) {
        // could not accept client
        close(sockfd);
        *err = ERR_NOT_ACCEPTED;
        goto error;
    }
    *sock = sockfd;

    SSL *conn = SSL_new(tls_config);
    if(!conn) {
        *err = ERR_CONNECT;
        goto error;
    } else if(SSL_set_fd(conn, clientfd) != 1) {
        *err = ERR_CONNECT;
        goto error;
    } else if(SSL_set_num_tickets(conn, 0) != 1) {
        *err = ERR_CONNECT;
        goto error;
    }

    SSL_set_accept_state(conn);
    if(SSL_accept(conn) != 1) {
        // could not build a SSL session
        SSL_shutdown(conn);
        SSL_free(conn);
        close(clientfd);
        *err = ERR_CONNECT;
        goto error;
    }
    // successful handshake
    return conn;

error:
    return NULL;
}
