#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

struct Client{
	int fd;
	int port;
	char addr[16];
	char name[50];
	int user;
	int motd;
	int join[1000]; // idx of channel_info
};

struct Channel{
	char name[50];
	char topic[50];
	int users[1000]; // idx of client_info
	int user_exist;
};

void nothing(int s){}

int readline(char* src, char* dst, int idx){
	while(src[idx]!='\n' && src[idx]!=0){
		*dst = src[idx++];
		dst++;
	}
	idx++;
	return idx;
}


void welcome_msg(int fd, char* name, int user, int* motd, int* logged_user, int client_num){
	if(user == 0 || strlen(name) == 0 || *motd == 1){
		return;
	}
	*motd = 1;
	*logged_user = *logged_user + 1;
	char welcome[1500];
	memset(welcome, 0, 1500);
	snprintf(welcome+strlen(welcome), 1500, ":mircd 001 %s :Welcome to the minimized IRC daemon!\n", name);
	snprintf(welcome+strlen(welcome), 1500, ":mircd 251 %s :There are %d users and %d invisible on 1 server\n", name, *logged_user, client_num-(*logged_user));
	snprintf(welcome+strlen(welcome), 1500, ":mircd 375 %s :- mircd Message of the day -\n", name);
	snprintf(welcome+strlen(welcome), 1500, ":mircd 375 %s :-  Hello, World!\n", name);
	snprintf(welcome+strlen(welcome), 1500, ":mircd 372 %s :-               @                    _\n", name);
	snprintf(welcome+strlen(welcome), 1500, ":mircd 372 %s :-   ____  ___   _   _ _   ____.     | |\n", name);
	snprintf(welcome+strlen(welcome), 1500, ":mircd 372 %s :-  /  _ `'_  \\ | | | '_/ /  __|  ___| |\n", name);
	snprintf(welcome+strlen(welcome), 1500, ":mircd 372 %s :-  | | | | | | | | | |   | |    /  _  |\n", name);
	snprintf(welcome+strlen(welcome), 1500, ":mircd 372 %s :-  | | | | | | | | | |   | |__  | |_| |\n", name);
	snprintf(welcome+strlen(welcome), 1500, ":mircd 372 %s :-  |_| |_| |_| |_| |_|   \\____| \\___,_|\n", name);
	snprintf(welcome+strlen(welcome), 1500, ":mircd 372 %s :-  minimized internet relay chat daemon\n", name);
	snprintf(welcome+strlen(welcome), 1500, ":mircd 372 %s :-\n", name);
	snprintf(welcome+strlen(welcome), 1500, ":mircd 376 %s :End of message of the day\n", name);
	write(fd, welcome, strlen(welcome));
}


int main(int argc, char* argv[])
{
	struct sockaddr_in	server_addr, client_addr;
	int listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	memset(&server_addr, 0, sizeof(struct sockaddr_in));
	socklen_t client_len = sizeof(struct sockaddr_in);

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[1]));
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in));
	listen(listen_fd, atoi(argv[1]));
	signal(SIGPIPE, nothing);
	
	fd_set allset, rset;
	struct Client client_info[FD_SETSIZE];	
	for(int i=0; i<FD_SETSIZE; i++){
		client_info[i].fd = -1;
	}
	FD_ZERO(&allset);
	FD_SET(listen_fd, &allset);
	int maxfd = listen_fd;
	int client_num = 0;
	int logged_user = 0;
	
	struct Channel channel_info[1000];
	for(int i=0; i<1000; i++){
		channel_info[i].user_exist = -1;
	}
	int max_channel = -1;
	int channel_num = 0;
	
	
	while(1) {
		rset = allset;
		int nready = select(maxfd+1, &rset, NULL, NULL, NULL);
		
		// accept client by checking listen_fd in rset
		if(FD_ISSET(listen_fd, &rset)){
			int connect_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
			int success = 0;
			for(int i=0; i<FD_SETSIZE; i++){
				if(client_info[i].fd < 0) {
					success = 1;
					client_info[i].fd = connect_fd;
					client_info[i].port = client_addr.sin_port;
					inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, client_info[i].addr, 16);
					memset(client_info[i].name, 0, 50);
					client_info[i].user = 0;
					client_info[i].motd = 0;
					for(int j=0; j<1000; j++){
						client_info[i].join[j] = -1;
					}
					printf("* client connected from %s:%d\n", client_info[i].addr, client_info[i].port);
					client_num++;
					break;
				}
			}
			if(success){
				FD_SET(connect_fd, &allset);
				if(connect_fd > maxfd){
					maxfd = connect_fd;
				}
			}
			else{
				printf("Too many client...\n");
			}
			if(--nready <= 0){
				continue;
			}
		}
		
		// handle connected client by checking connect_fd in client_info
		for(int i=0; i<FD_SETSIZE; i++){
			if(client_info[i].fd!=-1 && FD_ISSET(client_info[i].fd, &rset)){
				char buf[1500];
				char reply[1500];
				memset(buf, 0, 1500);
				memset(reply, 0, 1500);
				int b_recv = read(client_info[i].fd, buf, 1500);
				if(b_recv == 0){
					// client disconnect
					for(int k=0; k<1000; k++){
						if(client_info[i].join[k] != -1){
							for(int l=0; l<1000; l++){
								if(channel_info[client_info[i].join[k]].users[l] == i){
									channel_info[client_info[i].join[k]].users[l] = -1;
								}
								else{
									snprintf(reply, 1500, ":%s PART :%s\n", client_info[i].name, channel_info[client_info[i].join[k]].name);
									write(client_info[channel_info[client_info[i].join[k]].users[l]].fd, reply, strlen(reply));
								}
							}
							channel_info[client_info[i].join[k]].user_exist--;
							if(channel_info[client_info[i].join[k]].user_exist == 0){
								channel_info[client_info[i].join[k]].user_exist = -1;
								channel_num--;
							}
							client_info[i].join[k] != -1;
						}
					}
					printf("* client %s:%d disconnected\n", client_info[i].addr, client_info[i].port);
					close(client_info[i].fd);
					FD_CLR(client_info[i].fd, &allset);
					logged_user -= client_info[i].motd;
					client_info[i].motd = 0;
					client_info[i].user = 0;
					memset(client_info[i].name, 0, 50);
					memset(&client_info[i], 0, sizeof(struct Client));
					client_info[i].fd = -1;
					client_num--;
					continue;
				}
				else{
					// proccessing message
					char validator[1500];
					memset(validator, ' ', 1500);
					if(strcmp(buf, validator) <= 0){
						continue;
					}
					if(buf[strlen(buf)-1] == '\n'){
						buf[strlen(buf)-1] = 0;
					}
					if(buf[strlen(buf)-1] == '\r'){
						buf[strlen(buf)-1] = 0;
					}
					
					printf("%s\n", buf); //+
					int idx = 0;
					do{
						char cmd[1500];
						char reply[1500];
						char param1[1000];
						char param2[1000];
						char param3[1000];
						char param4[1000];
						memset(cmd, 0, 1500);
						memset(reply, 0, 1500);
						idx = readline(buf, cmd, idx);
						if(strcmp(cmd, validator) <= 0){
							continue;
						}
						if(cmd[strlen(cmd)-1] == '\n'){
							cmd[strlen(cmd)-1] = 0;
						}
						if(cmd[strlen(cmd)-1] == '\r'){
							cmd[strlen(cmd)-1] = 0;
						}
						cmd[strlen(cmd)] = '\n';
						memset(param1, 0, 1000);
						sscanf(cmd, "%s", param1);
						if(strcmp(param1, "NICK")!=0 && strcmp(param1, "USER")!=0 && strcmp(param1, "PING")!=0){
							if(strcmp(param1, "LIST")!=0 && strcmp(param1, "JOIN")!=0 && strcmp(param1, "TOPIC")!=0){
								if(strcmp(param1, "NAMES")!=0 && strcmp(param1, "PART")!=0 && strcmp(param1, "USERS")!=0){
									if(strcmp(param1, "PRIVMSG")!=0 && strcmp(param1, "QUIT")!=0){
										if(client_info[i].motd == 1){
											snprintf(reply, 1500, ":mircd 421 %s %s :Unknown command\n", client_info[i].name, param1);
										}
										else{
											snprintf(reply, 1500, ":mircd 421 %s :Unknown command\n", param1);
										}
										write(client_info[i].fd, reply, strlen(reply));
										continue;
									}
								}
							}
						}
						cmd[strlen(cmd)-1] = 0;
						
						
						if(strncmp(cmd, "NICK", 4) == 0){
							cmd[strlen(cmd)] = '\n';
							memset(param1, 0, 1000);
							memset(param2, 0, 1000);
							sscanf(cmd, "NICK %s %[^\n]", param1, param2);
							cmd[strlen(cmd)-1] = 0;
							
							if(strlen(param1)==0){
								if(client_info[i].motd == 1){
									snprintf(reply, 1500, ":mircd 431 %s :No nickname given\n", client_info[i].name);
								}
								else{
									snprintf(reply, 1500, ":mircd 431 :No nickname given\n");
								}
								write(client_info[i].fd, reply, strlen(reply));
								continue;	
							}
							else if(strlen(param1)>=50 || strlen(param2)!=0){
								if(client_info[i].motd == 1){
									snprintf(reply, 1500, ":mircd 432 %s %s :Erroneus nickname\n", client_info[i].name, cmd+5);
								}
								else{
									snprintf(reply, 1500, ":mircd 432 %s :Erroneus nickname\n", cmd+5);
								}
								write(client_info[i].fd, reply, strlen(reply));
								continue;
							}
							
							int collision = 0;
							for(int j=0; j<FD_SETSIZE; j++){
								if(client_info[j].fd!=-1){
									if(strcmp(param1, client_info[j].name) == 0){
										collision = 1;
										break;
									}
								}
							}
							if(collision == 1){
								if(client_info[i].motd == 1){
									snprintf(reply, 1500, ":mircd 436 %s %s :Nickname collision KILL\n", client_info[i].name, cmd+5);
								}
								else{
									snprintf(reply, 1500, ":mircd 436 %s :Nickname collision KILL\n", cmd+5);
								}
								write(client_info[i].fd, reply, strlen(reply));
								continue;
							}
							
							if(client_info[i].motd == 1){
								snprintf(reply, 1500, ":%s NICK %s\n", client_info[i].name, cmd+5);
								write(client_info[i].fd, reply, strlen(reply));
							}
							strcpy(client_info[i].name, cmd+5);
							welcome_msg(client_info[i].fd, client_info[i].name, client_info[i].user, &client_info[i].motd, &logged_user, client_num);
							continue;
						}
						
						
						if(strncmp(cmd, "USER", 4) == 0 && cmd[4]!='S'){
							cmd[strlen(cmd)] = '\n';
							memset(param1, 0, 1000);
							memset(param2, 0, 1000);
							memset(param3, 0, 1000);
							memset(param4, 0, 1000);
							sscanf(cmd, "USER %s %s %s :%[^\n]", param1, param2, param3, param4);
							cmd[strlen(cmd)-1] = 0;
							
							if(strlen(param1)==0 || strlen(param2)==0 || strlen(param3)==0 || strlen(param4)==0){
								if(client_info[i].motd == 1){
									snprintf(reply, 1500, ":mircd 461 %s USER :Not enough parameters\n", client_info[i].name);
								}
								else{
									snprintf(reply, 1500, ":mircd 461 USER :Not enough parameters\n");
								}
								write(client_info[i].fd, reply, strlen(reply));
								continue;
							}
							
							client_info[i].user = 1;
							welcome_msg(client_info[i].fd, client_info[i].name, client_info[i].user, &client_info[i].motd, &logged_user, client_num);
							continue;
						}
						
						
						if(strncmp(cmd, "QUIT", 4) == 0){
							for(int k=0; k<1000; k++){
								if(client_info[i].join[k] != -1){
									for(int l=0; l<1000; l++){
										if(channel_info[client_info[i].join[k]].users[l] == i){
											channel_info[client_info[i].join[k]].users[l] = -1;
										}
										else{
											snprintf(reply, 1500, ":%s PART :%s\n", client_info[i].name, channel_info[client_info[i].join[k]].name);
											write(client_info[channel_info[client_info[i].join[k]].users[l]].fd, reply, strlen(reply));
										}
									}
									channel_info[client_info[i].join[k]].user_exist--;
									if(channel_info[client_info[i].join[k]].user_exist == 0){
										channel_info[client_info[i].join[k]].user_exist = -1;
										channel_num--;
									}
									client_info[i].join[k] != -1;
								}
							}
							printf("* client %s:%d disconnected\n", client_info[i].addr, client_info[i].port);
							close(client_info[i].fd);
							FD_CLR(client_info[i].fd, &allset);
							logged_user -= client_info[i].motd;
							client_info[i].motd = 0;
							client_info[i].user = 0;
							memset(client_info[i].name, 0, 50);
							memset(&client_info[i], 0, sizeof(struct Client));
							client_info[i].fd = -1;
							client_num--;
							continue;
						}
						
						
						if(client_info[i].motd != 1){
							snprintf(reply, 1500, ":mircd 451 :You have not registered\n");
							write(client_info[i].fd, reply, strlen(reply));
							continue;
						}
						
						
						if(strncmp(cmd, "PING", 4) == 0){
							cmd[strlen(cmd)] = '\n';
							memset(param1, 0, 1000);
							sscanf(cmd, "PING %s", param1);
							cmd[strlen(cmd)-1] = 0;
							
							if(strlen(param1)==0){
								snprintf(reply, 1500, ":mircd 409 %s :No origin specified\n", client_info[i].name);
								write(client_info[i].fd, reply, strlen(reply));
								continue;	
							}
							
							snprintf(reply, 1500, "PONG %s\n", param1);
							write(client_info[i].fd, reply, strlen(reply));
						}
						
						
						if(strncmp(cmd, "LIST", 4) == 0){
							cmd[strlen(cmd)] = '\n';
							memset(param1, 0, 1000);
							sscanf(cmd, "LIST %s", param1);
							cmd[strlen(cmd)-1] = 0;
							
							snprintf(reply, 1500, ":mircd 321 %s Channel :Users  Name\n", client_info[i].name);
							write(client_info[i].fd, reply, strlen(reply));
							if(strlen(param1)==0){
								int channel_cnt = channel_num;
								for(int j=0; j<=max_channel; j++){
									if(channel_info[j].user_exist != -1){
										snprintf(reply, 1500, ":mircd 322 %s %s %d :%s\n", client_info[i].name, channel_info[j].name, channel_info[j].user_exist, channel_info[j].topic);
										write(client_info[i].fd, reply, strlen(reply));
										if(--channel_cnt == 0){
											break;
										}
									}
								}
							}
							else{
								for(int j=0; j<=max_channel; j++){
									if(channel_info[j].user_exist != -1){
										if(strcmp(param1, channel_info[j].name) == 0){
											snprintf(reply, 1500, ":mircd 322 %s %s %d :%s\n", client_info[i].name, channel_info[j].name, channel_info[j].user_exist, channel_info[j].topic);
											write(client_info[i].fd, reply, strlen(reply));
											break;
										}
									}
								}
							}
							snprintf(reply, 1500, ":mircd 323 %s :End of /LIST\n", client_info[i].name);
							write(client_info[i].fd, reply, strlen(reply));
						}
						
						
						if(strncmp(cmd, "JOIN", 4) == 0){
							cmd[strlen(cmd)] = '\n';
							memset(param1, 0, 1000);
							sscanf(cmd, "JOIN %s", param1);
							cmd[strlen(cmd)-1] = 0;
							
							if(strlen(param1)==0){
								snprintf(reply, 1500, ":mircd 461 %s JOIN :Not enough parameters\n", client_info[i].name);
								write(client_info[i].fd, reply, strlen(reply));
								continue;
							}
							
							int found = 0;
							for(int j=0; j<=max_channel; j++){
								if(channel_info[j].user_exist!=-1){
									if(strcmp(channel_info[j].name, param1) == 0){
										found = 1;
										int join_success = 0;
										for(int k=0; k<1000; k++){
											if(channel_info[j].users[k] == -1){
												for(int l=0; l<1000; l++){
													if(client_info[i].join[l] == -1){
														client_info[i].join[l] = j;
														channel_info[j].users[k] = i;
														join_success = 1;
														break;
													}
												}
												break;
											}
										}
										if(join_success == 1){
											channel_info[j].user_exist++;
											snprintf(reply, 1500, ":%s JOIN %s\n", client_info[i].name, channel_info[j].name);
											int channel_user = channel_info[j].user_exist;
											for(int k=0; k<1000; k++){
												if(channel_info[j].users[k] != -1){
													write(client_info[channel_info[j].users[k]].fd, reply, strlen(reply));
													if(--channel_user == 0){
														break;
													}
												}
											}
											if(strlen(channel_info[j].topic) == 0){
												snprintf(reply, 1500, ":mircd 331 %s %s :No topic is set\n", client_info[i].name, channel_info[j].name);
												write(client_info[i].fd, reply, strlen(reply));
											}
											else{
												snprintf(reply, 1500, ":mircd 332 %s %s :%s\n", client_info[i].name, channel_info[j].name, channel_info[j].topic);
												write(client_info[i].fd, reply, strlen(reply));
											}
											snprintf(reply, 1500, ":mircd 353 %s %s :", client_info[i].name, channel_info[j].name);
											channel_user = channel_info[j].user_exist;
											for(int k=0; k<1000; k++){
												if(channel_info[j].users[k] != -1){
													snprintf(reply+strlen(reply), 1500, "%s ", client_info[channel_info[j].users[k]].name);
													if(--channel_user == 0){
														break;
													}
												}
											}
											reply[strlen(reply)-1] = '\n';
											write(client_info[i].fd, reply, strlen(reply));
											snprintf(reply, 1500, ":mircd 366 %s %s :End of /NAMES List\n", client_info[i].name, channel_info[j].name);
											write(client_info[i].fd, reply, strlen(reply));
										}
										else{
											printf("Fail: Channel is full. / Join too many channel.\n");
										}
										break;
									}
								}
							}
							if(found == 0){
								for(int j=0; j<1000; j++){
									if(channel_info[j].user_exist==-1){
										memset(channel_info[j].name, 0, 50);
										memset(channel_info[j].topic, 0, 50);
										for(int k=0; k<1000; k++){
											channel_info[j].users[k] = -1;
										}
										int join_success = 0;
										for(int k=0; k<1000; k++){
											if(client_info[i].join[k] == -1){
												client_info[i].join[k] = j;
												channel_info[j].users[0] = i;
												strcpy(channel_info[j].name, param1);
												if(j > max_channel){
													max_channel = j;
												}
												channel_num++;
												join_success = 1;
												break;
											}
										}
										if(join_success == 1){
											channel_info[j].user_exist = 1;
											snprintf(reply, 1500, ":%s JOIN %s\n", client_info[i].name, channel_info[j].name);
											write(client_info[i].fd, reply, strlen(reply));
											if(strlen(channel_info[j].topic) == 0){
												snprintf(reply, 1500, ":mircd 331 %s %s :No topic is set\n", client_info[i].name, channel_info[j].name);
												write(client_info[i].fd, reply, strlen(reply));
											}
											else{
												snprintf(reply, 1500, ":mircd 332 %s %s :%s\n", client_info[i].name, channel_info[j].name, channel_info[j].topic);
												write(client_info[i].fd, reply, strlen(reply));
											}
											snprintf(reply, 1500, ":mircd 353 %s %s :%s\n", client_info[i].name, channel_info[j].name, client_info[i].name);
											write(client_info[i].fd, reply, strlen(reply));
											snprintf(reply, 1500, ":mircd 366 %s %s :End of /NAMES List\n", client_info[i].name, channel_info[j].name);
											write(client_info[i].fd, reply, strlen(reply));
										}
										else{
											printf("Fail: Join too many channel.\n");
										}
										break;
									}
								}
							}
						}
						
						
						if(strncmp(cmd, "TOPIC", 5) == 0){
							cmd[strlen(cmd)] = '\n';
							memset(param1, 0, 1000);
							memset(param2, 0, 1000);
							sscanf(cmd, "TOPIC %s :%[^\n]", param1, param2);
							cmd[strlen(cmd)-1] = 0;
							
							if(strlen(param1)==0){
								snprintf(reply, 1500, ":mircd 461 %s TOPIC :Not enough parameters\n", client_info[i].name);
								write(client_info[i].fd, reply, strlen(reply));
								continue;
							}
							
							int found = 0;
							for(int j=0; j<1000; j++){
								if(client_info[i].join[j]!=-1){
									if(strcmp(param1, channel_info[client_info[i].join[j]].name) == 0){
										found = 1;
										if(strlen(param2)==0){
											if(strlen(channel_info[client_info[i].join[j]].topic) == 0){
												snprintf(reply, 1500, ":mircd 331 %s %s :No topic is set\n", client_info[i].name, param1);
											}
											else{
												snprintf(reply, 1500, ":mircd 332 %s %s :%s\n", client_info[i].name, param1, channel_info[client_info[i].join[j]].topic);
											}
										}
										else{
											strcpy(channel_info[client_info[i].join[j]].topic, param2);
											snprintf(reply, 1500, ":mircd 332 %s %s :%s\n", client_info[i].name, param1, channel_info[client_info[i].join[j]].topic);
										}
										write(client_info[i].fd, reply, strlen(reply));
										break;
									}
								}
							}
							if(found == 0){
								snprintf(reply, 1500, ":mircd 442 %s %s :You're not on that channel\n", client_info[i].name, param1);
								write(client_info[i].fd, reply, strlen(reply));
							}
						}
						
						
						if(strncmp(cmd, "NAMES", 5) == 0){
							cmd[strlen(cmd)] = '\n';
							memset(param1, 0, 1000);
							sscanf(cmd, "NAMES %s", param1);
							cmd[strlen(cmd)-1] = 0;
							
							if(strlen(param1)!=0){
								for(int j=0; j<=max_channel; j++){
									if(channel_info[j].user_exist != -1){
										if(strcmp(param1, channel_info[j].name) == 0){
											snprintf(reply, 1500, ":mircd 353 %s %s :", client_info[i].name, channel_info[j].name);
											int channel_user = channel_info[j].user_exist;
											for(int k=0; k<1000; k++){
												if(channel_info[j].users[k] != -1){
													snprintf(reply+strlen(reply), 1500, "%s ", client_info[channel_info[j].users[k]].name);
													if(--channel_user == 0){
														break;
													}
												}
											}
											reply[strlen(reply)-1] = '\n';
											write(client_info[i].fd, reply, strlen(reply));
											printf("%s\n", reply);
											break;
										}
									}
								}
								snprintf(reply, 1500, ":mircd 366 %s %s :End of /NAMES list\n", client_info[i].name, param1);
								write(client_info[i].fd, reply, strlen(reply));
							}
							else{
								for(int j=0; j<=max_channel; j++){
									if(channel_info[j].user_exist != -1){
										snprintf(reply, 1500, ":mircd 353 %s %s :", client_info[i].name, channel_info[j].name);
										int channel_user = channel_info[j].user_exist;
										for(int k=0; k<1000; k++){
											if(channel_info[j].users[k] != -1){
												snprintf(reply+strlen(reply), 1500, "%s ", client_info[channel_info[j].users[k]].name);
												if(--channel_user == 0){
													break;
												}
											}
										}
										reply[strlen(reply)-1] = '\n';
										write(client_info[i].fd, reply, strlen(reply));
										snprintf(reply, 1500, ":mircd 366 %s %s :End of /NAMES list\n", client_info[i].name, channel_info[j].name);
										write(client_info[i].fd, reply, strlen(reply));
									}
								}
							}
						}
						
						
						if(strncmp(cmd, "PART", 4) == 0){
							cmd[strlen(cmd)] = '\n';
							memset(param1, 0, 1000);
							sscanf(cmd, "PART %s", param1);
							cmd[strlen(cmd)-1] = 0;
							
							if(strlen(param1)==0){
								snprintf(reply, 1500, ":mircd 461 %s PART :Not enough parameters\n", client_info[i].name);
								write(client_info[i].fd, reply, strlen(reply));
								continue;
							}
							
							int found = 0;
							for(int j=0; j<=max_channel; j++){
								if(channel_info[j].user_exist!=-1){
									if(strcmp(channel_info[j].name, param1) == 0){
										found = 1;
										int part_success = 0;
										for(int k=0; k<1000; k++){
											if(client_info[i].join[k] == j){
												part_success = 1;
												client_info[i].join[k] = -1;
												for(int l=0; l<1000; l++){
													if(channel_info[j].users[l] == i){
														channel_info[j].users[l] = -1;
														break;
													}
												}
												break;
											}
										}
										if(part_success == 1){
											channel_info[j].user_exist--;
											//if(channel_info[j].user_exist == 0){
											//	channel_info[j].user_exist = -1;
											//	channel_num--;
											//}
											snprintf(reply, 1500, ":%s PART :%s\n", client_info[i].name, channel_info[j].name);
											int channel_user = channel_info[j].user_exist;
											for(int k=0; k<1000; k++){
												if(channel_info[j].users[k] != -1){
													write(client_info[channel_info[j].users[k]].fd, reply, strlen(reply));
													if(--channel_user == 0){
														break;
													}
												}
											}
										}
										else{
											snprintf(reply, 1500, ":mircd 442 %s %s :You're not on that channel\n", client_info[i].name, channel_info[j].name);
											write(client_info[i].fd, reply, strlen(reply));
										}
										break;
									}
								}
							}
							if(found == 0){
								snprintf(reply, 1500, ":mircd 403 %s %s :No such channel\n", client_info[i].name, param1);
								write(client_info[i].fd, reply, strlen(reply));
							}
						}
						
						
						if(strncmp(cmd, "USERS", 5) == 0){
							char terminal[50] = "-";
							snprintf(reply, 1500, ":mircd 392 %s :UserID   Terminal  Host\n", client_info[i].name);
							write(client_info[i].fd, reply, strlen(reply));
							int user_cnt = logged_user;
							for(int j=0; j<FD_SETSIZE; j++){
								if(client_info[j].fd!=-1 && client_info[j].motd==1){
									snprintf(reply, 1500, ":mircd 393 %s :%-8s %-9s %-8s\n", client_info[i].name, client_info[j].name, terminal, client_info[j].addr);
									write(client_info[i].fd, reply, strlen(reply));
									if(--user_cnt == 0){
										break;
									}
								}
							}
							snprintf(reply, 1500, ":mircd 394 %s :End of users\n", client_info[i].name);
							write(client_info[i].fd, reply, strlen(reply));
						}
						
						
						if(strncmp(cmd, "PRIVMSG", 7) == 0){
							cmd[strlen(cmd)] = '\n';
							memset(param1, 0, 1000);
							memset(param2, 0, 1000);
							sscanf(cmd, "PRIVMSG %s :%[^\n]", param1, param2);
							cmd[strlen(cmd)-1] = 0;
							
							if(strlen(param1)==0){
								snprintf(reply, 1500, ":mircd 411 %s :No recipient given (PRIVMSG)\n", client_info[i].name);
								write(client_info[i].fd, reply, strlen(reply));
								continue;
							}
							
							int channel_idx = -1;
							for(int j=0; j<=max_channel; j++){
								if(channel_info[j].user_exist!=-1){
									if(strcmp(channel_info[j].name, param1) == 0){
										channel_idx = j;
										break;
									}
								}
							}
							if(channel_idx == -1){
								snprintf(reply, 1500, ":mircd 401 %s %s :No such nick/channel\n", client_info[i].name, param1);
								write(client_info[i].fd, reply, strlen(reply));
								continue;
							}
							
							if(strlen(param2)==0){
								snprintf(reply, 1500, ":mircd 412 %s :No text to send\n", client_info[i].name);
								write(client_info[i].fd, reply, strlen(reply));
								continue;
							}
							
							int channel_user = channel_info[channel_idx].user_exist;
							for(int k=0; k<1000; k++){
								if(channel_info[channel_idx].users[k] != -1 && channel_info[channel_idx].users[k] != i){
									snprintf(reply, 1500, ":%s PRIVMSG %s :%s\n", client_info[i].name, channel_info[channel_idx].name, param2);
									write(client_info[channel_info[channel_idx].users[k]].fd, reply, strlen(reply));
									if(--channel_user == 0){
										break;
									}
								}
							}
						}
						
						
					} while(idx < b_recv);
					
				}
				if(--nready <= 0){
					break;
				}
			}
		}
	}
	close(listen_fd);
	return 0;
}
