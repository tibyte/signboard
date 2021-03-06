#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define F_BUF 100 //file read buffer
#define MAX_CLIENT 127
#define BACKLOG 10
#define PORT 23432


void *h_read(void *arg);
void *h_input(void *arg);


pthread_mutex_t mutex;

int count; //client count
int list[MAX_CLIENT]; //connecting client list

int main(void)
{
	//init socket
	void *vp; //for thread join
	pthread_t t_read;
	pthread_t t_input;

	int server, client; //socket
	struct sockaddr_in s_adr, c_adr;
	int size; //client address size
	int option = 1; //bind option

	if(pthread_create(&t_input, NULL, h_input, NULL) != 0) {
		fprintf(stderr, "Faild to create thread\n");
		exit(1);
	}

	count = 0;

	printf("Type 'p' to send updates to clients\n\n");

	pthread_mutex_init(&mutex, NULL);

	server = socket(PF_INET, SOCK_STREAM, 0);

	memset(&s_adr, 0, sizeof(s_adr));
	s_adr.sin_family = AF_INET;
	s_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	s_adr.sin_port = htons(PORT);

	setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

	if(bind(server, (struct sockaddr*)&s_adr, sizeof(s_adr))==-1) {
		fprintf(stderr, "bind error\n");
		exit(1);
	}
	if(listen(server, BACKLOG)==-1) {
		fprintf(stderr, "listen error\n");
		exit(1);
	}

	printf("wating to connect.. port : %d\n", PORT);

	while(1) {
		size = sizeof(c_adr);
		client = accept(server, (struct sockaddr*)&c_adr, &size);

		pthread_mutex_lock(&mutex);
		printf("client fd : %d\n", client);
		list[count] = client;
		++count;
		pthread_mutex_unlock(&mutex);
		printf("Connected %s \n", inet_ntoa(c_adr.sin_addr));

		if(pthread_create(&t_read, NULL, h_read, (void*)&client)!=0) {
			fprintf(stderr, "thread creating error\n");
			exit(1);
		}
		pthread_detach(t_read);
	}


	pthread_join(t_input, &vp);

	close(server);
	exit(0);
}

void *h_read(void *arg)
{
	int sock = *((int*)arg);
	char msg;
	int i;
	char len;

	while(read(sock, &msg, 1)) {
		if(msg == 0x01) {
			write(sock, &msg, 1);
		}
		else if(msg == 0x02) {
			printf("client %d success\n", list[i]);
		}
		else if(msg == 0x03) {
			printf("client %d fail\n", list[i]);
		}
	}

	//disconnecting
	pthread_mutex_lock(&mutex);
	for(i=0; i<count; i++) {
		if(list[i] == sock) {
			for(; i<count-1; i++) {
				list[i] = list[i+1];
			}
			break;
		}
	}
	--count;
	pthread_mutex_unlock(&mutex);
	printf("fd %d disconnected\n", sock);
	close(sock);
	return NULL;
}

void* h_input(void *arg)
{

	int len; //
	char msg;
	char c;
	int i;
	struct sockaddr_in addr;

	FILE *fp;
	char buf[F_BUF];
	int nread;
	struct stat info;
	int file_size;

	while(1) {
		scanf("%c", &c);
		if(c == 'q')
			break;
		else if(c=='p') {
			//send db data
			for(i=0; i<count; i++) {
				pthread_mutex_lock(&mutex);
				msg = 0x02;
				write(list[i], &msg, 1);
				pthread_mutex_unlock(&mutex);
			}

			fp = fopen("up","r");
			if(fp != NULL) {

				stat("up", &info);
				file_size = info.st_size;
				for(i=0; i<count; i++) {
					write(list[i], &file_size, sizeof(int));
				}


				while(!feof(fp)) {
					nread = fread(buf, 1, F_BUF, fp);
					for(i=0; i<count; i++) {
						pthread_mutex_lock(&mutex);
						write(list[i], buf, nread);
						pthread_mutex_unlock(&mutex);
					}
				}
				printf("complete\n");
				fclose(fp);
			}
			else {
				file_size = 0;
				for(i=0; i<count; i++) {
					write(list[i], &file_size, sizeof(int));
				}
			}

		}


		else if(c=='i') {
			printf("current client count : %d\n", count);
			for(i=0; i<count; i++) {
				printf("client fd : %d\n", list[i]);
				getpeername(list[i], (struct sockaddr *)&addr, &len);
				printf("%s\n", inet_ntoa(addr.sin_addr));
			}
		}
	}
	exit(0);
}
