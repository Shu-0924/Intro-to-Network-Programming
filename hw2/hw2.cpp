#include <fstream>
#include <iostream>
#include <algorithm>
#include <vector>
#include <string>
#include <map>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define all(x) (x).begin(), (x).end()
#define err_quit(m) { perror(m); exit(-1); }
using namespace std;

string cls_to_str[5] = {string("None"), string("IN"), string("CS"), string("CH"), string("HS")};
string type_to_str[30] = {string("None"), string("A"), string("NS"), string("None"), string("None"), 
						  string("CNAME"), string("SOA"), string("None"), string("None"), string("None"),
						  string("None"), string("None"), string("None"), string("None"), string("None"),
						  string("MX"), string("TXT"), string("None"), string("None"), string("None"),
						  string("None"), string("None"), string("None"), string("None"), string("None"),
						  string("None"), string("None"), string("None"), string("AAAA"), string("None")};
struct DomainTree{
	string tmp;
	string name;
	string nip_record;
	map<string, DomainTree> child;
	map<string, vector<string>> IN;
	map<string, vector<string>> CS;
	map<string, vector<string>> CH;
	map<string, vector<string>> HS;
	map<string, vector<string>> additional_name; //key = cls + type
} DT;

map<string, DomainTree> Zone;
string Forward_IP;


int read_int(char* buf, int& idx){
	int ret = 0;
	ret += buf[idx++];
	ret <<= 8;
	ret += buf[idx++];
	return ret;
}


string read_name(char* buf, int& idx){
	string ret = "";
	int cnt;
	while(cnt = buf[idx++]){
		while(cnt--){
			ret += buf[idx++];
		}
		ret += '.';
	}
	return ret;
}


void add_int(string& str, string num_str, int cnt){
	long long num = stoll(num_str);
	string tmp = "";
	while(cnt--){
		char c = num % 256;
		num/=256;
		tmp += c;
	}
	reverse(all(tmp));
	str += tmp;
}


void add_name(string& str, string name){
	string tmp = "";
	int size = name.size();
	char cnt = 0;
	for(int i=size-2; i>=0; i--){
		if(name[i]!='.'){
			tmp += name[i];
			cnt++;
		}
		else{
			tmp += cnt;
			cnt = 0;
		}
	}
	tmp += cnt;
	reverse(all(tmp));
	str += tmp;
	str += '\0';
}


void add_Rdata(string& str, string Rdata_str, string type){
	if(type=="A"){
		int pos = Rdata_str.find('.');
		add_int(str, Rdata_str.substr(0, pos), 1);
		Rdata_str = Rdata_str.substr(pos+1);
		pos = Rdata_str.find('.');
		add_int(str, Rdata_str.substr(0, pos), 1);
		Rdata_str = Rdata_str.substr(pos+1);
		pos = Rdata_str.find('.');
		add_int(str, Rdata_str.substr(0, pos), 1);
		add_int(str, Rdata_str.substr(pos+1), 1);
	}
	else if(type=="AAAA"){
		int colon_cnt = 7;
		for(int i=0; i<Rdata_str.size(); i++){
			if(Rdata_str[i] == ':'){
				colon_cnt--;
			}
		}
		string add_colon;
		if(colon_cnt == 0){
			add_colon = Rdata_str;
		}
		else{
			int pos = Rdata_str.find("::");
			add_colon = Rdata_str.substr(0, pos+1);
			while(colon_cnt--){
				add_colon += "0:";
			}
			add_colon += '0';
			add_colon += Rdata_str.substr(pos+1);
			add_colon += ':';
		}
		for(int i=0; i<8; i++){
			int pos = add_colon.find(':');
			int num = stoi(add_colon.substr(0, pos), nullptr, 16);
			add_int(str, to_string(num), 2);
			add_colon = add_colon.substr(pos+1);
		}
	}
	else if(type=="NS"){
		add_name(str, Rdata_str);
	}
	else if(type=="CNAME"){
		add_name(str, Rdata_str);
	}
	else if(type=="SOA"){
		int pos = Rdata_str.find(' ');
		add_name(str, Rdata_str.substr(0, pos));
		Rdata_str = Rdata_str.substr(pos+1);
		pos = Rdata_str.find(' ');
		add_name(str, Rdata_str.substr(0, pos));
		Rdata_str = Rdata_str.substr(pos+1);
		pos = Rdata_str.find(' ');
		add_int(str, Rdata_str.substr(0, pos), 4);
		Rdata_str = Rdata_str.substr(pos+1);
		pos = Rdata_str.find(' ');
		add_int(str, Rdata_str.substr(0, pos), 4);
		Rdata_str = Rdata_str.substr(pos+1);
		pos = Rdata_str.find(' ');
		add_int(str, Rdata_str.substr(0, pos), 4);
		Rdata_str = Rdata_str.substr(pos+1);
		pos = Rdata_str.find(' ');
		add_int(str, Rdata_str.substr(0, pos), 4);
		add_int(str, Rdata_str.substr(pos+1), 4);
	}
	else if(type=="MX"){
		int pos = Rdata_str.find(' ');
		add_int(str, Rdata_str.substr(0, pos), 2);
		add_name(str, Rdata_str.substr(pos+1));
	}
	else if(type=="TXT"){
		add_int(str, to_string((int)Rdata_str.size()), 1);
		str += Rdata_str;
	}
	else{
		return;
	}
}


void store_record(DomainTree* node, string record){
	while(record.size()>0 && record.back()<=32){
		record.pop_back();
	}
	
	int pos = record.find(',');
	string ttl = record.substr(0, pos);
	string remain = record.substr(pos+1);
	
	pos = remain.find(',');
	string cls = remain.substr(0, pos);
	remain = remain.substr(pos+1);
	
	pos = remain.find(',');
	string type = remain.substr(0, pos);
	string Rdata_str = remain.substr(pos+1);
	string Rdata = "";
	
	string RR = "";
	add_name(RR, node->name);
	if(type=="A"){
		add_int(RR, "1", 2);
	}
	else if(type=="AAAA"){
		add_int(RR, "28", 2);
	}
	else if(type=="NS"){
		add_int(RR, "2", 2);
		node->additional_name[cls+type].push_back(Rdata_str);
	}
	else if(type=="CNAME"){
		add_int(RR, "5", 2);
	}
	else if(type=="SOA"){
		add_int(RR, "6", 2);
	}
	else if(type=="MX"){
		add_int(RR, "15", 2);
		pos = Rdata_str.find(' ');
		node->additional_name[cls+type].push_back(Rdata_str.substr(pos+1));
	}
	else if(type=="TXT"){
		add_int(RR, "16", 2);
	}
	else if(type=="NIP"){
		RR = "";
		add_name(RR, node->tmp);
		add_int(RR, "1", 2);
		add_int(RR, "1", 2);
		add_int(RR, ttl, 4);
		add_Rdata(Rdata, Rdata_str, "A");
		add_int(RR, to_string((int)Rdata.size()), 2);
		RR += Rdata;
		node->nip_record = RR;
		return;
	}
	else{
		return;
	}
	
	if(cls=="IN"){
		add_int(RR, "1", 2);
	}
	else if(cls=="CS"){
		add_int(RR, "2", 2);
	}
	else if(cls=="CH"){
		add_int(RR, "3", 2);
	}
	else if(cls=="HS"){
		add_int(RR, "4", 2);
	}
	else{
		return;
	}
	
	add_int(RR, ttl, 4);
	add_Rdata(Rdata, Rdata_str, type);
	add_int(RR, to_string((int)Rdata.size()), 2);
	RR += Rdata;
	if(cls=="IN"){
		node->IN[type].push_back(RR);
	}
	else if(cls=="CS"){
		node->CS[type].push_back(RR);
	}
	else if(cls=="CH"){
		node->CH[type].push_back(RR);
	}
	else if(cls=="HS"){
		node->HS[type].push_back(RR);
	}
}


DomainTree* find_zone(string domain){
	string D(domain.c_str());
	while(!D.empty() && Zone.find(D)==Zone.end()){
		int pos = D.find('.');
		D = D.substr(pos+1);
	}
	if(!D.empty()){
		return &(Zone[D]);
	}
	return nullptr;
}


DomainTree* get_node(DomainTree* root, string sub_domain, string cls, string type){
	DomainTree* node = root;
	if(node->child.find(sub_domain) != node->child.end()){
		node = &(node->child[sub_domain]);
	}
	else if(sub_domain==root->name){
		node = root;
	}
	else{
		return nullptr;
	}
	if(cls=="IN"){
		if(node->IN.find(type) != node->IN.end()){
			return node;
		}
	}
	else if(cls=="CS"){
		if(node->CS.find(type) != node->CS.end()){
			return node;
		}
	}
	else if(cls=="CH"){
		if(node->CH.find(type) != node->CH.end()){
			return node;
		}
	}
	else if(cls=="HS"){
		if(node->HS.find(type) != node->HS.end()){
			return node;
		}
	}
	return nullptr;
}


vector<string>& get_RR(DomainTree* node, string cls, string type){
	if(cls=="IN"){
		return node->IN[type];
	}
	else if(cls=="CS"){
		return node->CS[type];
	}
	else if(cls=="CH"){
		return node->CH[type];
	}
	else if(cls=="HS"){
		return node->HS[type];
	}
	return node->IN[type];
}


void forward(int fd, char* buf, int size, struct sockaddr_in csin){
	socklen_t sinlen;
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(53);
	sin.sin_addr.s_addr = inet_addr(Forward_IP.c_str());
	int foreign_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(foreign_fd < 0){
		err_quit("socket");
	}
	
	char recv[1500];
	memset(recv, 0, 1500);
	sendto(foreign_fd, buf, size, 0, (struct sockaddr*) &sin, sizeof(sin));
	int rlen = recvfrom(foreign_fd, recv, sizeof(recv), 0, (struct sockaddr*) &sin, &sinlen);
	if(rlen > 0){
		sendto(fd, recv, rlen, 0, (struct sockaddr*) &csin, sizeof(csin));
	}
}


int main(int argc, char *argv[]) {
	
	int fd;
	ifstream conf_fin(argv[2]);
	conf_fin >> Forward_IP;
	
	string row;
	while(conf_fin >> row){
		int pos = row.find(',');
		string domain = row.substr(0, pos);
		string zone_path = row.substr(pos+1);
		Zone[domain] = DT;
		
		string record;
		ifstream zone_fin(zone_path);
		getline(zone_fin, record);
		DomainTree* root = &(Zone[domain]);
		
		while(getline(zone_fin, record)){
			pos = record.find(',');
			string sub_domain = record.substr(0, pos);
			DomainTree* node = root;
			if(sub_domain == "@"){
				node->name = domain;
				store_record(node, record.substr(pos+1));
			}
			else{
				sub_domain += ".";
				sub_domain += domain;
				if(node->child.find(sub_domain) == node->child.end()){
					node->child[sub_domain] = DT;
				}
				node = &(node->child[sub_domain]);
				node->name = sub_domain;
				store_record(node, record.substr(pos+1));
			}
		}
	}
	
	
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(atoi(argv[1]));
	if((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		err_quit("socket");	
	if(bind(fd, (struct sockaddr*) &sin, sizeof(sin)) < 0)
		err_quit("bind");	
	setvbuf(stdout, NULL, _IONBF, 0);
	
	while(1) {
		char buf[1500];
		memset(buf, 0, 1500);
		struct sockaddr_in sin, csin;
		socklen_t csinlen = sizeof(csin);
		
		if(recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr*) &csin, &csinlen) > 0) {
			int idx = 12;
			string name = read_name(buf, idx);
			string type = type_to_str[read_int(buf, idx)];
			string cls = cls_to_str[read_int(buf, idx)];
			int query_size = 1500;
			while(query_size > idx && buf[query_size-1]=='\0'){
				query_size--;
			}
			char option[1500];
			memset(option, 0, 1500);
			memcpy(option, buf+idx, query_size-idx);
			
			buf[7] = (char)0; // Answer_RRs
			buf[9] = (char)0; // Authority_RRs
			buf[11] = (char)1; // Additional_RRs
			
			int len = idx;
			DomainTree* root = find_zone(name);
			if(root == nullptr){
				printf("I don't know...\n");
				// forward to foreign dns server
				pid_t PID = fork();
				if(PID == 0){
					if (fork() == 0) {
						forward(fd, buf, query_size, csin);
						exit(0);
					}
					else{
						exit(0);
					}
				}
				else{
					waitpid(PID, NULL, 0);
				}
			}
			else{
				printf("Query receive!\n");
				// nip.io like
				int success = 1;
				string original_name(name.c_str());
				int pos = name.find('.');
				string num_1 = name.substr(0, pos);
				for(int i=0; i<num_1.size(); i++){
					if(num_1[i]<'0' || num_1[i]>'9'){
						success = 0;
					}
				}
				if(success==1){
					name = name.substr(pos+1);
					pos = name.find('.');
					string num_2 = name.substr(0, pos);
					for(int i=0; i<num_2.size(); i++){
						if(num_2[i]<'0' || num_2[i]>'9'){
							success = 0;
						}
					}
					if(success==1){
						name = name.substr(pos+1);
						pos = name.find('.');
						string num_3 = name.substr(0, pos);
						for(int i=0; i<num_3.size(); i++){
							if(num_3[i]<'0' || num_3[i]>'9'){
								success = 0;
							}
						}
						if(success==1){
							name = name.substr(pos+1);
							pos = name.find('.');
							string num_4 = name.substr(0, pos);
							for(int i=0; i<num_4.size(); i++){
								if(num_4[i]<'0' || num_4[i]>'9'){
									success = 0;
								}
							}
							if(success==1){
								if(stoi(num_1)<256 && stoi(num_2)<256 && stoi(num_3)<256 && stoi(num_4)<256){
									string r = "1,IN,NIP,";
									r += num_1;
									r += '.';
									r += num_2;
									r += '.';
									r += num_3;
									r += '.';
									r += num_4;
									root->tmp = original_name;
									store_record(root, r);
									buf[7] = (char)1;
									string RR = root->nip_record;
									memcpy(buf+len, RR.c_str(), RR.size());
									len += RR.size();
								}
							}
						}
					}
				}
				
				// Answer
				name = original_name;
				DomainTree* node = get_node(root, name, cls, type);
				if(node != nullptr){
					vector<string> RRs = get_RR(node, cls, type);
					buf[7] += (char)RRs.size();
					for(auto& RR: RRs){
						memcpy(buf+len, RR.c_str(), RR.size());
						len += RR.size();
					}
				}
				
				// Authority
				if(node!=nullptr && (node!=root || type=="MX" || type=="SOA")){
					if(get_node(root, root->name, cls, "NS")!=nullptr){
						vector<string> RRs = get_RR(root, cls, "NS");
						buf[9] += (char)RRs.size();
						for(auto& RR: RRs){
							memcpy(buf+len, RR.c_str(), RR.size());
							len += RR.size();
						}
					}
				}
				else{
					if(type!="NS" || node==nullptr){
						vector<string> RRs = get_RR(root, cls, "SOA");
						buf[9] += (char)RRs.size();
						for(auto& RR: RRs){
							memcpy(buf+len, RR.c_str(), RR.size());
							len += RR.size();
						}
					}
				}
				
				// Additional
				if(type=="NS" || type=="MX"){
					if(node!=nullptr && node->additional_name.find(cls+type)!=node->additional_name.end()){
						vector<string> additional_names = node->additional_name[cls+type];
						for(auto& additional_name: additional_names){
							DomainTree* additional_node = get_node(root, additional_name, cls, "A");
							if(additional_node != nullptr){
								vector<string> RRs = get_RR(additional_node, cls, "A");
								buf[11] += (char)RRs.size();
								for(auto& RR: RRs){
									memcpy(buf+len, RR.c_str(), RR.size());
									len += RR.size();
								}
							}
						}
					}
				}
				
				memcpy(buf+len, option, query_size-idx);
				sendto(fd, buf, len+query_size-idx, 0, (struct sockaddr*) &csin, sizeof(csin));
			}
		}
	}
	return 0;
}
