#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define err_quit(m) { perror(m); exit(-1); }

char content[1005][35000];


int main(int argc, char *argv[]) {
	if(argc < 4) {
		return -fprintf(stderr, "usage: %s <path_to_store> <file_num> <port>\n", argv[0]);
	}
	
	int fd;
	int file_num = atoi(argv[2]);
	char file_recv[1005][100];
	for(int i=0; i<1005; i++){
		memset(content[i], 0, 35000);
		memset(file_recv[i], '0', 100);
	}
	
	
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(strtol(argv[argc-1], NULL, 0));
	if((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		err_quit("socket");	
	if(bind(fd, (struct sockaddr*) &sin, sizeof(sin)) < 0)
		err_quit("bind");
		
	setvbuf(stdout, NULL, _IONBF, 0);
	
	
	while(1) {
		char path[100];
		char buf[1500];
		memset(buf, 0, 1500);
		struct sockaddr_in sin, csin;
		socklen_t csinlen = sizeof(csin);
		
		if(recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr*) &csin, &csinlen) > 0) {
			int i = (buf[3]-'0')*100 + (buf[4]-'0')*10 + (buf[5]-'0');
			int j = (buf[6]-'0')*10 + (buf[7]-'0');
			
			char check[10];
			memset(check, 0, 10);
			strncpy(check, buf+8, 7);
			int checksum = 0;
			for(int k=15; k<1015; k++){
				checksum += buf[k];
			}
			if(atoi(check) != checksum){
				continue;
			}
			
			if(file_recv[i][j] == '0'){
				file_recv[i][j] = '1';
				memcpy(content[i]+j*1000, buf+15, 1000);
				memset(path, 0, 100);
				strcpy(path, argv[1]);
				int len = strlen(path);
				snprintf(path+len, 100, "/%06d", i);
				FILE* fp = fopen(path, "w");
				fprintf(fp, "%s", content[i]);
				fclose(fp);
			}
			
			// double cumulative ACK
			memset(buf+15, 0, 150);
			memcpy(buf+6, file_recv[i], 100);
			sendto(fd, buf, strlen(buf), 0, (struct sockaddr*) &csin, sizeof(csin));
			sendto(fd, buf, strlen(buf), 0, (struct sockaddr*) &csin, sizeof(csin));
		}
	}
	close(fd);
}
