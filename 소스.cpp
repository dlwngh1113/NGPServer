#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib, "ws2_32")
#include<WinSock2.h>
#include<iostream>
#include<fstream>
#include<string>

#define SERVERPORT 9000
#define BUFSIZE 512

void err_quit(const char* msg);
void err_display(const char* msg);
int recvn(SOCKET s, char* buf, int len, int flags);
void check_file(char* buf, int& len);

void err_quit(const char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, (LPCWSTR)msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

void err_display(const char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", msg, (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

int recvn(SOCKET s, char* buf, int len, int flags)
{
	int received;
	char* ptr = buf;
	int left = len;

	while (left > 0) {
		received = recv(s, ptr, left, flags);
		if (received == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if (received == 0)
			break;
		left -= received;
		ptr += received;

		std::cout << "수신율 : " << (float)received / (float)len * 100.f << "%\n";
	}

	return (len - left);
}

int main(int argc, char* argv[])
{
	int retval;

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, NULL);
	if (listenSocket == INVALID_SOCKET)err_quit("socket()");

	SOCKADDR_IN serverAddr;
	ZeroMemory(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(SERVERPORT);
	retval = bind(listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
	if (retval == SOCKET_ERROR)err_quit("bind()");

	retval = listen(listenSocket, SOMAXCONN);
	if (retval == SOCKET_ERROR)err_quit("listen()");

	SOCKET clientSocket;
	SOCKADDR_IN clientAddr;
	int addrlen;
	char buf[BUFSIZE + 1];
	int len;

	while (true)
	{
		addrlen = sizeof(clientAddr);
		clientSocket = accept(listenSocket, (SOCKADDR*)&clientAddr, &addrlen);
		if (clientSocket == INVALID_SOCKET) {
			err_display("accept()");
			break;
		}
		printf("\n[TCP 서버] 클라이언트 접속: IP주소 = %s, 포트 번호 = %d\n",
			inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

		while (true)
		{
			//파일 길이 수신
			retval = recvn(clientSocket, (char*)&len, sizeof(int), NULL);
			if (retval == SOCKET_ERROR) {
				err_display("recv()");
				break;
			}
			else if (retval == 0)
				break;

			printf("[TCP/%s:%d] %d bytes received\n", inet_ntoa(clientAddr.sin_addr),
				ntohs(clientAddr.sin_port), len);

			//파일 이름, 내용 수신
			retval = recvn(clientSocket, buf, len, NULL);
			if (retval == SOCKET_ERROR) {
				err_display("recv()");
				break;
			}
			else if (retval == 0)
				break;
			buf[len] = '\0';

			check_file(buf, len);

			printf("[TCP/%s:%d] %s\n", inet_ntoa(clientAddr.sin_addr),
				ntohs(clientAddr.sin_port), buf);
		}
		closesocket(clientSocket);
		printf("\n[TCP 서버] 클라이언트 종료: IP주소 = %s, 포트 번호 = %d\n",
			inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
	}
	closesocket(listenSocket);

	WSACleanup();
	return 0;
}

void check_file(char* buf, int& len)
{
	int idx{ 0 };
	for (int i = 0; i < len; ++i) {
		if (buf[i] == '\n') {
			idx = i;
			break;
		}
	}
	char* p = new char[idx + 1];
	for (int i = 0; i < idx; ++i)
		p[i] = buf[i];
	p[idx] = NULL;

	std::ofstream out(p, std::ios::trunc | std::ios::binary);
	for (int i = idx + 1; i < len; ++i) {
		out.put(buf[i]);
	}

	out.close();

	delete[] p;
}