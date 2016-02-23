#include<stdlib.h>
#include <libssh2.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include "syc_scp.h"
int syc_files(struct send_file *sendfiles){
	unsigned long hostaddr=inet_addr("127.0.0.1");
	int sock, i, auth_pw = 1;
	int file_opt_type;
	struct sockaddr_in sin;
	const char *fingerprint;
	LIBSSH2_SESSION *session = NULL;
	LIBSSH2_CHANNEL *channel;
	const char *username="root";
	const char *password="123456";
	const char *loclfile;
	const char *scppath;
	FILE *local;
	int rc;
	char mem[1024];
	size_t nread;
	char *ptr;
	struct stat fileinfo;
	struct send_file *next_sendfile=sendfiles;
	char * delete_file_cmd = "rm -f ";
	char *mkdir_cmd ="mkdir -p ";
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

	/* Ultra basic "connect to port 22 on localhost"
	 * Your code is responsible for creating the socket establishing the
	 * connection
	 */ 
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(-1 == sock) {
		fprintf(stderr, "failed to create socket!\n");
		return -1;
	}

	sin.sin_family = AF_INET;
	sin.sin_port = htons(22);
	sin.sin_addr.s_addr = hostaddr;
	if (connect(sock, (struct sockaddr*)(&sin),
				sizeof(struct sockaddr_in)) != 0) {
		fprintf(stderr, "failed to connect!\n");
		return -1;
	}

	/* Create a session instance
	*/ 
	session = libssh2_session_init();

	if(!session)
		return -1;

	/* ... start it up. This will trade welcome banners, exchange keys,
	 * and setup crypto, compression, and MAC layers
	 */ 
	rc = libssh2_session_handshake(session, sock);

	if(rc) {
		fprintf(stderr, "Failure establishing SSH session: %d\n", rc);
		return -1;
	}

	/* At this point we havn't yet authenticated.  The first thing to do
	 * is check the hostkey's fingerprint against our known hosts Your app
	 * may have it hard coded, may go to a file, may present it to the
	 * user, that's your call
	 */ 
	fingerprint = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA1);

	fprintf(stderr, "Fingerprint: ");
	for(i = 0; i < 20; i++) {
		fprintf(stderr, "%02X ", (unsigned char)fingerprint[i]);
	}
	fprintf(stderr, "\n");

	if (auth_pw) {
		/* We could authenticate via password */ 
		if (libssh2_userauth_password(session, username, password)) {

			fprintf(stderr, "Authentication by password failed.\n");
			goto shutdown;
		}
	} else {
		/* Or by public key */ 
		if (libssh2_userauth_publickey_fromfile(session, username,

					"/home/username/.ssh/id_rsa.pub",
					"/home/username/.ssh/id_rsa",
					password)) {
			fprintf(stderr, "\tAuthentication by public key failed\n");
			goto shutdown;
		}
	}

	/* Send a file via scp. The mode parameter must only have permissions! */ 
	while(next_sendfile!=NULL){
		loclfile = next_sendfile->source_file;
		scppath = next_sendfile->target_file;
		file_opt_type = next_sendfile->opt_type;
		next_sendfile = next_sendfile->next_file;
		if(file_opt_type==1){
			//delete file
			printf("delete file %s\n",scppath);
			channel = libssh2_channel_open_session(session);
			if(!channel){
				printf("open session error\n");
				continue;
			}
			char * command = malloc(strlen(delete_file_cmd)+strlen(scppath)+1);
			strcpy(command,delete_file_cmd);
			strcat(command,scppath);
			printf("%s\n",command);
			if(libssh2_channel_exec(channel, command)==0){
				printf("delete file %s success\n",scppath);
			}else{
				printf("delete file %s failed\n",scppath);
			}
		}else{
			if(file_opt_type==2){
				printf("add new file:%s\n",scppath);
				channel = libssh2_channel_open_session(session);
				if(!channel){
					printf("open session error\n");
					continue;
				}
				int i,len=0;
				char *d_scppath;
				len = strlen(scppath);
				for(i=len-1;i>=0;i--){
					if(scppath[i]=='/'){
						d_scppath = (char *)malloc(i+1);
						strncpy(d_scppath,scppath,i);
						d_scppath[i]='\0';
						//printf("d_scppath:%s,i=%d\n",d_scppath,i);
						char * command = malloc(strlen(mkdir_cmd)+strlen(d_scppath)+1);
						strcpy(command,mkdir_cmd);
						strcat(command,d_scppath);
						printf("%s\n",command);
						if(libssh2_channel_exec(channel, command)==0){
							printf("mkdir %s success\n",d_scppath);
						}else{
							printf("mkdir file %s failed\n",d_scppath);
						}

						break;
					}
				}
			}else{
				printf("modify file %s\n",scppath);
			}
			local = fopen(loclfile, "rb");
			if (!local) {
				fprintf(stderr, "Can't open local file %s\n", loclfile);
				continue;
			}

			stat(loclfile, &fileinfo);
			channel = libssh2_scp_send(session, scppath, fileinfo.st_mode & 0777,

					(unsigned long)fileinfo.st_size);

			if (!channel) {
				char *errmsg;
				int errlen;
				int err = libssh2_session_last_error(session, &errmsg, &errlen, 0);

				fprintf(stderr, "Unable to open a session: (%d) %s\n", err, errmsg);
				//goto shutdown;
				continue;
			}

			fprintf(stderr, "SCP session waiting to send file\n");
			do {
				nread = fread(mem, 1, sizeof(mem), local);
				if (nread <= 0) {
					/* end of file */ 
					break;
				}
				ptr = mem;

				do {
					/* write the same data over and over, until error or completion */ 
					rc = libssh2_channel_write(channel, ptr, nread);

					if (rc < 0) {
						fprintf(stderr, "ERROR %d\n", rc);
						break;
					}
					else {
						/* rc indicates how many bytes were written this time */ 
						ptr += rc;
						nread -= rc;
					}
				} while (nread);

			} while (1);
		}
	}        
	fprintf(stderr, "Sending EOF\n");
	libssh2_channel_send_eof(channel);


	fprintf(stderr, "Waiting for EOF\n");
	libssh2_channel_wait_eof(channel);


	fprintf(stderr, "Waiting for channel to close\n");
	libssh2_channel_wait_closed(channel);


	libssh2_channel_free(channel);

	channel = NULL;

shutdown:

	if(session) {
		libssh2_session_disconnect(session, "Normal Shutdown, Thank you for playing");

		libssh2_session_free(session);

	}
#ifdef WIN32
	closesocket(sock);
#else
	close(sock);
#endif
	if (local)
		fclose(local);
	fprintf(stderr, "all done\n");

	libssh2_exit();


	return 0;
}
