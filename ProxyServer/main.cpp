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
#define BufferSize 1
bool isLog = false;
void closeAB(int A, int B)
{
	shutdown(A, SHUT_RDWR);
	shutdown(B, SHUT_RDWR);
}
void AToB(int A, int B, bool cl = true) {
	int status = 1;
	std::string color;
	if (cl)color = "\033[34;1m"; else color = "\033[31;1m";
	char buffer[BufferSize];
	while (status > 0)
	{
		memset(buffer, 0, BufferSize);
		status = recv(A, buffer, sizeof(buffer), 0);
		send(B, buffer, sizeof(buffer), 0);
		if (isLog)std::cout << color + "+\e[0m" << std::flush;
	}
	closeAB(A, B);
	std::cout << color + "$\e[0m" << std::flush;
};

int main(int argc, char* argv[])
{
	if (argc < 4)return -1;
	int ThisPort = std::stoi(argv[1]);
	char* ObjectAddress = argv[2];
	int ObjectPort = std::stoi(argv[3]);
	if (argc > 4)	isLog = true;
	signal(SIGPIPE, SIG_IGN);
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
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
	std::cout << "Server Started" << std::endl;
	while (true) {
		sockaddr_in r;
		int sin_size = sizeof(struct sockaddr_in);
		int client_sockfd;
		if ((client_sockfd = accept(server_fd, (struct sockaddr *)&r, (socklen_t*)&sin_size)) < 0)
			continue;
		std::cout << "#" << std::flush;
		int socketfd = socket(AF_INET, SOCK_STREAM, 0);
		sockaddr_in sockaddr;
		sockaddr.sin_family = AF_INET;
		sockaddr.sin_port = htons(ObjectPort);
		inet_pton(AF_INET, ObjectAddress, &sockaddr.sin_addr);
		if ((connect(socketfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr))) < 0)
		{
			printf("Client Error");
			close(client_sockfd);
			std::cout << "$" << std::flush;
		}
		else {
			std::cout << "%" << std::flush;
			std::thread c2s(AToB, client_sockfd, socketfd, true);
			std::thread s2c(AToB, socketfd, client_sockfd, false);
			c2s.detach();
			s2c.detach();
		}
	}
	return 0;
}