#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define err_quit(m) { perror(m); exit(-1); }


int file_size[1005];
int checksum[1005][100];
char content[1005][35000];


int main(int argc, char *argv[]) {
	if(argc < 5) {
		return -fprintf(stderr, "usage: %s <path_to_store> <file_num> <port> <ip>\n", argv[0]);
	}

	int fd;
	int complete_cnt = 0;
	int complete[1005];
	int file_num = atoi(argv[2]);
	char file_recv[1005][100];
	for(int i=0; i<1005; i++){
		memset(content[i], 0, 35000);
		memset(file_recv[i], '0', 100);
	}
	
	char path[100];
	memset(path, 0, 100);
	strcpy(path, argv[1]);
	int len = strlen(path);
	for(int i=0; i<file_num; i++){
		snprintf(path+len, 100, "/%06d", i);
		FILE* fp = fopen(path, "r");
		file_size[i] = fread(content[i], 1, 35000, fp);
		fclose(fp);
	}
	for(int i=0; i<file_num; i++){
		int packet_num = file_size[i]/1000 + 1;
		for(int j=0; j<packet_num; j++){
			checksum[i][j] = 0;
			for(int k=0; k<1000; k++){
				checksum[i][j] += content[i][j*1000+k];
			}
		}
	}
	
	
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(strtol(argv[argc-2], NULL, 0));
	if(inet_pton(AF_INET, argv[argc-1], &sin.sin_addr) != 1) {
		return -fprintf(stderr, "** cannot convert IPv4 address for %s\n", argv[1]);
	}
	if((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		err_quit("socket");
	
	connect(fd, (struct sockaddr*) &sin, sizeof(sin));
	setvbuf(stdout, NULL, _IONBF, 0);
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags|O_NONBLOCK);
	
	
	int file_id = 0;
	while(1) {
		if(complete[file_id]==1){
			file_id = (file_id + 1) % file_num;
		}
		
		char buf[1500];
		int packet_num = file_size[file_id]/1000 + 1;
		for(int packet_id=0; packet_id<packet_num; packet_id++){
			if(file_recv[file_id][packet_id] == '1'){
				continue;
			}
			snprintf(buf+0, 1000, "%06d", file_id);
			snprintf(buf+6, 1000, "%02d", packet_id);
			snprintf(buf+8, 1000, "%07d", checksum[file_id][packet_id]);
			memcpy(buf+15, content[file_id]+packet_id*1000, 1000);
			sendto(fd, buf, strlen(buf), 0, (struct sockaddr*) &sin, sizeof(sin));
			usleep(90);
		}
		file_id = (file_id + 1) % file_num;
		
		// receive ACK
		memset(buf, 0, 1500);
		if(recvfrom(fd, buf, sizeof(buf), 0, NULL, NULL) > 0) {
			int i = (buf[3]-'0')*100 + (buf[4]-'0')*10 + (buf[5]-'0');
			memcpy(file_recv[i], buf+6, 100);
			
			// check complete transfer or not
			if(complete[i] == 0){
				int done = 1;
				int packet_num = file_size[i]/1000 + 1;
				for(int j=0; j<packet_num; j++){
					if(file_recv[i][j] == '0'){
						done = 0;
						break;
					}
				}
				if(done == 1){
					complete[i] = 1;
					complete_cnt++;
					//printf("File %d complete. (%d/%d)\n", i, complete_cnt, file_num);
				}
				if(complete_cnt==file_num){
					break;
				}
			}
		}
	}
	close(fd);
}
