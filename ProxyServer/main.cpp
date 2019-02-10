#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <signal.h>
#include <vector>
#include <iostream>
#include <string>
#include <iomanip>
#include "time.h"
#include <map>
#include "aes.h"
#include "base64.h"

bool isstopping = false;
bool isLog = false;
bool isshow = false;
bool isserver = false;
bool isusingaes = false;

char g_key[17] = "password";
const char g_iv[17] = "";//ECB MODE不需要关心chain，可以填空

int server_fd;
int thread_cout = 0;
int buffer_times = 1024;
std::vector<int> fds;
std::map<std::string, std::string*> lookup_table;

void init_table(std::string str, bool _isserver) {
	if (_isserver)
		if (!isusingaes)
			lookup_table[str] = new std::string(base64_encode_str(str));
		else
			lookup_table[str] = new std::string(EncryptionAES(base64_encode_str(str)));
	else
		if (!isusingaes)
			lookup_table[str] = new std::string(base64_decode(str));
		else
			lookup_table[str] = new std::string((base64_decode(DecryptionAES(str))));
}
inline void removeValue(int value) {
	for (std::vector<int>::iterator it = fds.begin(); it != fds.end(); ++it)
		if (*it == value) {
			it = fds.erase(it);
			break;
		}
}
inline int sendstr(int socketfd, std::string str) {
	char buf[1];
	int status = 1;
	for (size_t i = 0; i < str.size(); i++)
	{
		buf[0] = str[i];
		status = send(socketfd, buf, sizeof(buf), 0);
		if (status < 1)
			return -1;
	}
	return 1;
}

std::string find_table(std::string key, bool isserver) {
	if (lookup_table.count(key) == 0)
		init_table(key, isserver);
	return *lookup_table[key];
	//if (isserver)
	//	return base64_encode_str(key);
	//else
	//	return base64_decode(key);
	//return key;
}

inline void closeA(int A)
{
	shutdown(A, SHUT_RDWR);
	removeValue(A);
}
void AToB(int A, int B, bool cl = true) {
	++thread_cout;
	ssize_t status = 1;
	std::string color;
	if (cl)
		color = "\033[34;1m";
	else
		color = "\033[31;1m";
	char buffer[1];
	while (status > 0 && !isstopping) {
		std::string s;
		s.clear();
		memset(buffer, 0, 1);
		if (cl)
		{
			for (int c = 0; c < buffer_times; ++c)
			{
				memset(buffer, 0, 1);
				status = recv(A, buffer, sizeof(buffer), c == 0 ? 0 : MSG_DONTWAIT);
				if (status < 0 && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN))
					continue;
				else if (status > 0)
					s += buffer[0];
				else
					goto close;
			}
			if (!isstopping)
				if (isLog)
					std::cout
					<< color + (isshow ? buffer : "+") + "\e[0m"
					<< std::flush;
			s = find_table(s, true);
			s += '\n';
		}
		else
		{
			while (buffer[0] != '\n') {
				memset(buffer, 0, 1);
				status = recv(A, buffer, sizeof(buffer), 0);
				s += buffer[0];
				if (status < 1)
					goto close;
				if (!isstopping)
					if (isLog)
						std::cout
						<< color + (isshow ? buffer : "+") + "\e[0m"
						<< std::flush;
			};
			s = find_table(s.substr(0, s.length() - 1), false);
		}
		status = sendstr(B, s);
	}
close:
	closeA(B);
	if (!isstopping) std::cout << color + "$\e[0m" << std::flush;
	--thread_cout;
};

void stop(int sig) {
	if (isstopping) return;
	isstopping = true;
	shutdown(server_fd, SHUT_RDWR);
	std::cout << std::endl << "Server Stopping: Shutting Down Sockets " << std::endl << std::endl;
	for (size_t i = 0; i < fds.size(); ++i) {
		std::cout << "\e[1A\e[K" << "[" << i << "/" << fds.size() << "]" << std::endl;
		shutdown(fds[i], SHUT_RDWR);
		usleep(33000);
	}
	sleep(1);
	std::cout << "\e[1A\e[K\e[1A\e[K[$$$$$$$$$$$$$$]" << std::endl;
	exit(0);
}

void thread_wait_stdin_stop() {
	std::string buf;
	while (buf != "stop")
		std::cin >> buf;
	stop(0);
}
void usage() {
	std::cout
		<< std::endl
		<< "Proxy "
		<< "--local-port port "
		<< "--remote-port port "
		<< "--remote-address address "
		<< "[--password password]"
		<< "[--server]"
		<< "[--log] "
		<< "[--log-flow] "
		<< "[--buffer-size length]"
		<< std::endl;
}

int main(int argc, char* argv[])
{
	uint16_t ThisPort = 0;
	uint16_t ObjectPort = 0;
	char* ObjectAddress = (char*)"";
	for (int i = 1; i < argc; ++i)
	{
		if (std::string(argv[i]) == "--local-port") {
			ThisPort = (uint16_t)std::stoi(argv[i + 1]);
			++i;
			continue;
		}
		else if (std::string(argv[i]) == "--remote-port") {
			ObjectPort = (uint16_t)std::stoi(argv[i + 1]);
			++i;
			continue;
		}
		else if (std::string(argv[i]) == "--remote-address") {
			ObjectAddress = argv[i + 1];
			++i;
			continue;
		}
		else if (std::string(argv[i]) == "--buffer-size") {
			buffer_times = std::stoi(argv[i + 1]);
			++i;
			continue;
		}
		else if (std::string(argv[i]) == "--password") {
			if (std::string(argv[i + 1]).substr(0, 16) != std::string(argv[i + 1])) {
				std::cout << "Note: Password Will Cut Off After 16bit" << std::endl;
			}
			strcpy(g_key, std::string(argv[i + 1]).substr(0, 16).c_str());
			++i;
			continue;
		}
		else if (std::string(argv[i]) == "--server") {
			isserver = true;
			continue;
		}
		else if (std::string(argv[i]) == "--log") {
			isLog = true;
			continue;
		}
		else if (std::string(argv[i]) == "--log-flow") {
			isshow = true;
			continue;
		}
	}
	if (std::string(ObjectAddress) == "") { std::cout << "Remote Address Not Found"; usage(); return -1; }
	if (ObjectPort == 0) { std::cout << "Remote Port Not Found"; usage(); return -1; }
	if (ThisPort == 0) { std::cout << "Local Port Not Found"; usage(); return -1; }
	if (std::string(g_key) != "password")
		isusingaes = true;
	for (int i = 0; i < 128; ++i)
		init_table(std::string(1, (char)i), true);

	signal(SIGINT, stop);
	signal(SIGPIPE, SIG_IGN);
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in t;
	t.sin_family = AF_INET;
	t.sin_addr.s_addr = INADDR_ANY;
	t.sin_port = htons(ThisPort);
	if (bind(server_fd, (struct sockaddr *)&t, sizeof(struct sockaddr)) < 0)
	{
		perror("bind error");
		return 1;
	}
	if (listen(server_fd, 5) < 0)
	{
		perror("listen error");
		return 1;
	};
	std::cout << std::endl << "[#%#%#%#%#%#%#%]" << std::endl;
	std::thread ws(thread_wait_stdin_stop);
	ws.detach();
	while (true) {
		sockaddr_in r;
		int sin_size = sizeof(struct sockaddr_in);
		int client_sockfd;
		if ((client_sockfd = accept(server_fd, (struct sockaddr *)&r, (socklen_t*)&sin_size)) < 0)
			continue;
		std::cout << "\033[34;1m#\033[0m" << std::flush;
		fds.push_back(client_sockfd);
		int socketfd = socket(AF_INET, SOCK_STREAM, 0);
		sockaddr_in sockaddr;
		sockaddr.sin_family = AF_INET;
		sockaddr.sin_port = htons(ObjectPort);
		inet_pton(AF_INET, ObjectAddress, &sockaddr.sin_addr);
		if ((connect(socketfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr))) < 0)
		{
			close(client_sockfd);
			removeValue(client_sockfd);
			std::cout << "\033[34;1m$\033[0m" << std::flush;
		}
		else {
			fds.push_back(socketfd);
			std::cout << "\033[31;1m#\033[0m" << std::flush;
			std::thread(AToB, client_sockfd, socketfd, !isserver).detach();
			std::thread(AToB, socketfd, client_sockfd, isserver).detach();
		}
	}
	close(server_fd);
	return 0;
}