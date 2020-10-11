#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#pragma comment(lib, "ws2_32")
#include<WinSock2.h>
#include<stdio.h>
#include<iostream>
#include<string>

#define SERVERPORT 9000
#define BUFSIZE 1024

void err_quit(const char* msg);
void err_display(const char* msg);
int recvn(SOCKET s, char* buf, int len, int flags, FILE* fp);
int recv_fileName(SOCKET clientSocket, char* buf, int& len);
int recv_file(SOCKET clientSocket, char* buf, int& len, FILE* fp);

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

int recvn(SOCKET s, char* buf, int len, int flags, FILE* fp)
{
	int received = 0, retval;
	int left = len;
	float recvRate = 0;

	while (left > 0) {
		int toRecv = left >= BUFSIZE ? BUFSIZE : left;
		retval = recv(s, buf, toRecv, flags);

		if (retval == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if (retval == 0)
			break;

		left -= retval;
		received += retval;

		recvRate = (float)received / (float)len * 100.f;
		printf("수신율 : %f\r", recvRate);
		fwrite(buf, retval, 1, fp);
	}
	printf("[File Size] %d bytes received\n", len);
	fclose(fp);

	return (len - left);
}

DWORD WINAPI ProcessClient(LPVOID arg)
{
	SOCKET clientSocket = (SOCKET)arg;
	SOCKADDR_IN clientAddr;
	char buf[BUFSIZE + 1] = { NULL };
	int len, addrlen, retval;

	addrlen = sizeof(clientAddr);
	getpeername(clientSocket, (sockaddr*)&clientAddr, &addrlen);

	//소켓 패킷 처리
	while (true)
	{
		FILE* fp = NULL;
		retval = recv_fileName(clientSocket, buf, len);
		if (retval == SOCKET_ERROR) {
			err_display("recv_fileName()");
			break;
		}
		else if (retval == 0)
			break;

		retval = recv_file(clientSocket, buf, len, fp);
		if (retval == SOCKET_ERROR) {
			err_display("recv_file()");
			break;
		}
		else if (retval == 0)
			break;

		ZeroMemory(buf, BUFSIZE);
	}

	closesocket(clientSocket);
	printf("\n[TCP 서버] 클라이언트 종료: IP주소 = %s, 포트 번호 = %d\n",
		inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

	return 0;
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
	HANDLE hThread;

	//소켓 접속 대기
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

		hThread = CreateThread(NULL, 0, ProcessClient, (LPVOID)clientSocket, 0, NULL);
		if (NULL == hThread)
			closesocket(clientSocket);
		else
			CloseHandle(hThread);
	}
	closesocket(listenSocket);

	WSACleanup();
	return 0;
}

int recv_fileName(SOCKET clientSocket, char* buf, int& len)
{
	//파일 이름 길이 수신
	int retval = recv(clientSocket, (char*)&len, sizeof(int), NULL);
	if (retval == SOCKET_ERROR) {
		err_display("recv()");
		return retval;
	}
	else if (retval == 0)
		return retval;

	//파일 이름 수신
	retval = recv(clientSocket, buf, len, NULL);

	FILE* fp = fopen(buf, "wb");
	recv_file(clientSocket, buf, len, fp);
}

int recv_file(SOCKET clientSocket, char* buf, int& len, FILE* fp)
{
	//파일 길이 수신
	int retval = recv(clientSocket, (char*)&len, sizeof(int), NULL);
	if (retval == SOCKET_ERROR) {
		err_display("recv()");
		return retval;
	}
	else if (retval == 0)
		return retval;

	//파일 내용 수신
	retval = recvn(clientSocket, buf, len, NULL, fp);
	if (retval == SOCKET_ERROR) {
		err_display("recv()");
		return retval;
	}
	else if (retval == 0)
		return retval;
}