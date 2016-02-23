#include <libssh2.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include "syc_scp.h" 
static int waitsocket(int socket_fd, LIBSSH2_SESSION *session)
{
    struct timeval timeout;
    int rc;
    fd_set fd;
    fd_set *writefd = NULL;
    fd_set *readfd = NULL;
    int dir;
 
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
 
    FD_ZERO(&fd);
 
    FD_SET(socket_fd, &fd);
 
    /* now make sure we wait in the correct direction */ 
    dir = libssh2_session_block_directions(session);

 
    if(dir & LIBSSH2_SESSION_BLOCK_INBOUND)
        readfd = &fd;
 
    if(dir & LIBSSH2_SESSION_BLOCK_OUTBOUND)
        writefd = &fd;
 
    rc = select(socket_fd + 1, readfd, writefd, NULL, &timeout);
 
    return rc;
}
 
int syc_remote_check()
{
    const char *hostname = "101.200.178.6";
    char* commandline[2] = {"cd /data/apache-tomcat-7.0.62/webapps/ && find HMS -type f -print0 | xargs -0 md5sum > /data/HMS.md5","cat /data/HMS.md5"};
    const char *username    = "root";
    const char *password    = "7RX3KhCLgq";
    unsigned long hostaddr;
    int sock;
    struct sockaddr_in sin;
    const char *fingerprint;
    LIBSSH2_SESSION *session;
    LIBSSH2_CHANNEL *channel;
    int rc;
    int exitcode;
    char *exitsignal=(char *)"none";
    int bytecount = 0;
    size_t len;
    LIBSSH2_KNOWNHOSTS *nh;
    int type;
 
#ifdef WIN32
    WSADATA wsadata;
    int err;
 
    err = WSAStartup(MAKEWORD(2,0), &wsadata);
    if (err != 0) {
        fprintf(stderr, "WSAStartup failed with error: %d\n", err);
        return 1;
    }
#endif
 
    rc = libssh2_init (0);

    if (rc != 0) {
        fprintf (stderr, "libssh2 initialization failed (%d)\n", rc);
        return 1;
    }
 
    hostaddr = inet_addr(hostname);
 
    /* Ultra basic "connect to port 22 on localhost"
     * Your code is responsible for creating the socket establishing the
     * connection
     */ 
    sock = socket(AF_INET, SOCK_STREAM, 0);
 
    sin.sin_family = AF_INET;
    sin.sin_port = htons(22);
    sin.sin_addr.s_addr = hostaddr;
    if (connect(sock, (struct sockaddr*)(&sin),
                sizeof(struct sockaddr_in)) != 0) {
        fprintf(stderr, "failed to connect!\n");
        return -1;
    }
 
    /* Create a session instance */ 
    session = libssh2_session_init();

    if (!session)
        return -1;
 
    /* tell libssh2 we want it all done non-blocking */ 
    libssh2_session_set_blocking(session, 0);

 
    /* ... start it up. This will trade welcome banners, exchange keys,
     * and setup crypto, compression, and MAC layers
     */ 
    while ((rc = libssh2_session_handshake(session, sock)) ==

           LIBSSH2_ERROR_EAGAIN);
    if (rc) {
        fprintf(stderr, "Failure establishing SSH session: %d\n", rc);
        return -1;
    }
 
    nh = libssh2_knownhost_init(session);

    if(!nh) {
        /* eeek, do cleanup here */ 
        return 2;
    }
 
    /* read all hosts from here */ 
    libssh2_knownhost_readfile(nh, "known_hosts",

                               LIBSSH2_KNOWNHOST_FILE_OPENSSH);
 
    /* store all known hosts to here */ 
    libssh2_knownhost_writefile(nh, "dumpfile",

                                LIBSSH2_KNOWNHOST_FILE_OPENSSH);
 
    fingerprint = libssh2_session_hostkey(session, &len, &type);

    if(fingerprint) {
        struct libssh2_knownhost *host;
#if LIBSSH2_VERSION_NUM >= 0x010206
        /* introduced in 1.2.6 */ 
        int check = libssh2_knownhost_checkp(nh, hostname, 22,

                                             fingerprint, len,
                                             LIBSSH2_KNOWNHOST_TYPE_PLAIN|
                                             LIBSSH2_KNOWNHOST_KEYENC_RAW,
                                             &host);
#else
        /* 1.2.5 or older */ 
        int check = libssh2_knownhost_check(nh, hostname,

                                            fingerprint, len,
                                            LIBSSH2_KNOWNHOST_TYPE_PLAIN|
                                            LIBSSH2_KNOWNHOST_KEYENC_RAW,
                                            &host);
#endif
        fprintf(stderr, "Host check: %d, key: %s\n", check,
                (check <= LIBSSH2_KNOWNHOST_CHECK_MISMATCH)?
                host->key:"<none>");
 
        /*****
         * At this point, we could verify that 'check' tells us the key is
         * fine or bail out.
         *****/ 
    }
    else {
        /* eeek, do cleanup here */ 
        return 3;
    }
    libssh2_knownhost_free(nh);

 
    if ( strlen(password) != 0 ) {
        /* We could authenticate via password */ 
        while ((rc = libssh2_userauth_password(session, username, password)) ==

               LIBSSH2_ERROR_EAGAIN);
        if (rc) {
            fprintf(stderr, "Authentication by password failed.\n");
            goto shutdown;
        }
    }
    else {
        /* Or by public key */ 
        while ((rc = libssh2_userauth_publickey_fromfile(session, username,

                                                         "/home/user/"
                                                         ".ssh/id_rsa.pub",
                                                         "/home/user/"
                                                         ".ssh/id_rsa",
                                                         password)) ==
               LIBSSH2_ERROR_EAGAIN);
        if (rc) {
            fprintf(stderr, "\tAuthentication by public key failed\n");
            goto shutdown;
        }
    }
 
#if 0
    libssh2_trace(session, ~0 );

#endif
    FILE *result = fopen("./projects/md5","w");
    int i=0;    
    for(i=0;i<2;i++){
    /* Exec non-blocking on the remove host */ 
    while( (channel = libssh2_channel_open_session(session)) == NULL &&

           libssh2_session_last_error(session,NULL,NULL,0) ==

           LIBSSH2_ERROR_EAGAIN )
    {
        waitsocket(sock, session);
    }
    if( channel == NULL )
    {
        fprintf(stderr,"Error\n");
        exit( 1 );
    }
    while( (rc = libssh2_channel_exec(channel, commandline[i])) ==

           LIBSSH2_ERROR_EAGAIN )
    {
        waitsocket(sock, session);
    }
    if( rc != 0 )
    {
        fprintf(stderr,"Error\n");
        exit( 1 );
    }
    printf("We Read:\n");
    for( ;; )
    {
        /* loop until we block */ 
        int rc;
        do
        {
            char buffer[0x4000];
            rc = libssh2_channel_read( channel, buffer, sizeof(buffer) );

            if( rc > 0 )
            {
                int i;
                bytecount += rc;
                for( i=0; i < rc; ++i )
                    fputc( buffer[i],result);
                fprintf(stderr, "\n");
            }
            else {
                if( rc != LIBSSH2_ERROR_EAGAIN )
                    /* no need to output this for the EAGAIN case */ 
                    fprintf(stderr, "libssh2_channel_read returned %d\n", rc);
            }
        }
        while( rc > 0 );
 
        /* this is due to blocking that would occur otherwise so we loop on
           this condition */ 
        if( rc == LIBSSH2_ERROR_EAGAIN )
        {
            waitsocket(sock, session);
        }
        else
            break;
    }
    }
    fclose(result);
    exitcode = 127;
    while( (rc = libssh2_channel_close(channel)) == LIBSSH2_ERROR_EAGAIN )

        waitsocket(sock, session);
 
    if( rc == 0 )
    {
        exitcode = libssh2_channel_get_exit_status( channel );

        libssh2_channel_get_exit_signal(channel, &exitsignal,

                                        NULL, NULL, NULL, NULL, NULL);
    }
 
    if (exitsignal)
        fprintf(stderr, "\nGot signal: %s\n", exitsignal);
    else
        fprintf(stderr, "\nEXIT: %d bytecount: %d\n", exitcode, bytecount);
 
    libssh2_channel_free(channel);

    channel = NULL;
 
shutdown:
 
    libssh2_session_disconnect(session,

                               "Normal Shutdown, Thank you for playing");
    libssh2_session_free(session);

 
#ifdef WIN32
    closesocket(sock);
#else
    close(sock);
#endif
    fprintf(stderr, "all done\n");
 
    libssh2_exit();

 
    return 0;
}
