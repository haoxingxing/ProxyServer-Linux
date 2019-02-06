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

bool isstopping = false;
bool isLog = false;
bool isshow = false;
bool isUsingVector = false;
int server_fd;
int thread_cout = 0;
std::vector<int> fds;
//void printVector() {
//	std::cout << "[";
//	for (int i = 0; i < fds.size(); ++i)
//		std::cout << "'" << fds[i] << "'";
//	std::cout << "]";
//}
void removeValue(int value) {
	while (isUsingVector)
		sleep(1);
	isUsingVector = true;
	//printVector();
	for (std::vector<int>::iterator it = fds.begin(); it != fds.end(); ++it)
		if (*it == value) {
			it = fds.erase(it);
			break;
		}
	//printVector();
	isUsingVector = false;
}

void closeA(int A)
{
	shutdown(A, SHUT_RDWR);
	removeValue(A);
}
void AToB(int A, int B, bool cl = true) {
	int status = 1;
	std::string color;
	if (cl)color = "\033[34;1m"; else color = "\033[31;1m";
	char buffer[1];
	while (status > 0 && !isstopping)
	{
		memset(buffer, 0, 1);
		status = recv(A, buffer, sizeof(buffer), 0);
		if (status < 1) break;
		send(B, buffer, sizeof(buffer), 0);
		if (!isstopping) if (isLog) std::cout << color + (isshow ? buffer[0] : '+') + "\e[0m" << std::flush;
	}
	closeA(B);
	if (!isstopping) std::cout << color + "$\e[0m" << std::flush;
	--thread_cout;
};

static void stop(int sig) {
	isstopping = true;
	//printVector();
	close(server_fd);
	//printVector();
	for (int i = 0; i < (int)fds.size(); ++i) {
		//	std::cout << fds[i];
		shutdown(fds[i], SHUT_RDWR);
		close(fds[i]);
	}
	std::cout << std::endl << "Waiting For Threads To exit" << std::endl << std::endl;
	while (thread_cout != 0) {
		std::cout << "\e[1A\e[K" << "[" << thread_cout << "] threads left" << std::endl;
		usleep(33000);
	};
	sleep(1);
	std::cout << "\e[1A\e[K\e[1A\e[K[$$$$$$$$$$$$$$]" << std::endl;
	exit(0);
}
int main(int argc, char* argv[])
{
	if (argc < 4) {
		std::cout << "\n Usage: \n\rProxyServer LocalPort RemoteAddress RemotePort [-log]";
		return -1;
	}
	int ThisPort = std::stoi(argv[1]);
	char* ObjectAddress = argv[2];
	int ObjectPort = std::stoi(argv[3]);
	if (argc > 4)	isLog = true;
	if (argc > 5)	isshow = true;
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
	while (true) {
		sockaddr_in r;
		int sin_size = sizeof(struct sockaddr_in);
		int client_sockfd;
		if ((client_sockfd = accept(server_fd, (struct sockaddr *)&r, (socklen_t*)&sin_size)) < 0)
			continue;
		std::cout << "#" << std::flush;
		fds.push_back(client_sockfd);
		int socketfd = socket(AF_INET, SOCK_STREAM, 0);
		sockaddr_in sockaddr;
		sockaddr.sin_family = AF_INET;
		sockaddr.sin_port = htons(ObjectPort);
		inet_pton(AF_INET, ObjectAddress, &sockaddr.sin_addr);
		if ((connect(socketfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr))) < 0)
		{
			printf("Client Error");
			close(client_sockfd);
			removeValue(client_sockfd);
			std::cout << "\033[34;1m$\033[0m" << std::flush;
		}
		else {
			fds.push_back(socketfd);
			std::cout << "%" << std::flush;
			std::thread c2s(AToB, client_sockfd, socketfd, true);
			std::thread s2c(AToB, socketfd, client_sockfd, false);
			c2s.detach();
			++thread_cout;
			s2c.detach();
			++thread_cout;
			//printVector();
		}
	}
	close(server_fd);
	return 0;
}