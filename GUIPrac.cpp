// GUIPrac.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//

#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "framework.h"
#include "GUIPrac.h"

#define MAX_LOADSTRING 100

// 전역 변수:
HINSTANCE hInst;                                // 현재 인스턴스입니다.
WCHAR szTitle[MAX_LOADSTRING];                  // 제목 표시줄 텍스트입니다.
WCHAR szWindowClass[MAX_LOADSTRING];            // 기본 창 클래스 이름입니다.

SOCKET listenSocket;
constexpr int offset = 20;
CRITICAL_SECTION cs;
HWND g_hDlg;
bool isUsing[10];
struct Client {
    SOCKET socket;
    char buf[BUF_SIZE + 1];
    HWND hWnd;
    HWND hText;
    std::string file;
    unsigned char id;
};

std::unordered_map<SOCKET, Client*> clients;

BOOL CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);
void DisplayText(HWND,const char* fmt, ...);
int recvn(SOCKET s, char* buf, int len, int flags, FILE* fp);
DWORD WINAPI ServerMain(LPVOID arg);
DWORD WINAPI ProcessClient(LPVOID arg);
int recv_fileName(SOCKET clientSocket, char* buf, int& len);
int recv_file(SOCKET clientSocket, char* buf, int& len, FILE* fp);
void err_quit(const char* msg);
void err_display(const char* msg);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    InitializeCriticalSection(&cs);

    for (int i = 0; i < 10; ++i)
        isUsing[i] = false;
    CreateThread(NULL, 0, ServerMain, NULL, 0, NULL);

    DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, (DLGPROC)DlgProc);

    closesocket(listenSocket);

    DeleteCriticalSection(&cs);
    WSACleanup();
    return 0;
}

BOOL CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
        g_hDlg = hDlg;
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDCANCEL:
            for (auto& m : clients)
                delete m.second;
            clients.clear();
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        return FALSE;
    }
    return FALSE;
}

int recvn(SOCKET s, char* buf, int len, int flags, FILE* fp)
{
    int received = 0, retval;
    int left = len;
    float recvRate = 0;

    while (left > 0) {
        int toRecv = left >= BUF_SIZE ? BUF_SIZE : left;
        retval = recv(s, buf, toRecv, flags);

        if (retval == SOCKET_ERROR)
            return SOCKET_ERROR;
        else if (retval == 0)
            break;

        left -= retval;
        received += retval;

        recvRate = (float)received / (float)len * 100.f;
        SendMessage(clients[s]->hWnd, PBM_SETPOS, (WPARAM)(int)recvRate, NULL);
        fwrite(buf, retval, 1, fp);
    }
    fclose(fp);

    return (len - left);
}

DWORD WINAPI ServerMain(LPVOID arg)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 1;

    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    SOCKADDR_IN addr;
    ZeroMemory(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVERPORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    ::bind(listenSocket, (sockaddr*)&addr, sizeof(addr));
    listen(listenSocket, SOMAXCONN);

    HANDLE hThread;

    while (true) {
        SOCKADDR_IN cAddr;
        int len = sizeof(cAddr);
        SOCKET clientSocket = accept(listenSocket, (sockaddr*)&cAddr, &len);
        if (clientSocket == INVALID_SOCKET)
            return 1;
        
        hThread = CreateThread(NULL, 0, ProcessClient, (LPVOID)clientSocket, 0, NULL);
        if (NULL == hThread)
            closesocket(clientSocket);
        else
            CloseHandle(hThread);
    }

    return 0;
}

DWORD WINAPI  ProcessClient(LPVOID arg)
{
    SOCKET clientSocket = (SOCKET)arg;
    clients[clientSocket] = new Client;
    clients[clientSocket]->socket = clientSocket;
    EnterCriticalSection(&cs);
    for (int i = 0; i < 10; ++i) {
        if (!isUsing[i]) {
            isUsing[i] = true;
            clients[clientSocket]->id = i;
            break;
        }
    }
    LeaveCriticalSection(&cs);
    SOCKADDR_IN clientAddr;
    int len, addrlen, retval;
    addrlen = sizeof(clientAddr);
    getpeername(clientSocket, (sockaddr*)&clientAddr, &addrlen);

    //소켓 패킷 처리
    while (true)
    {
        retval = recv_fileName(clientSocket, clients[clientSocket]->buf, len);
        if (retval == SOCKET_ERROR) {
            err_display("recv_fileName()");
            break;
        }
        else if (retval == 0)
            break;

        FILE* fp = fopen(clients[clientSocket]->buf, "wb");

        retval = recv_file(clientSocket, clients[clientSocket]->buf, len, fp);
        if (retval == SOCKET_ERROR) {
            err_display("recv_file()");
            break;
        }
        else if (retval == 0)
            break;

        ZeroMemory(clients[clientSocket]->buf, BUF_SIZE);
    }

    isUsing[clients[clientSocket]->id] = false;
    clients.erase(clientSocket);
    closesocket(clientSocket);

    return 0;
}

void DisplayText(HWND hWnd, const char* fmt, ...)
{
    va_list arg;
    va_start(arg, fmt);

    char cbuf[BUF_SIZE + 256];
    vsprintf(cbuf, fmt, arg);

    int nLength = GetWindowTextLength(hWnd);
    SendMessage(hWnd, EM_SETSEL, 0, nLength);
    SendMessage(hWnd, EM_REPLACESEL, TRUE, (LPARAM)cbuf);

    va_end(arg);
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
    buf[retval] = '\0';
    clients[clientSocket]->file = clients[clientSocket]->buf;
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

    clients[clientSocket]->hText = CreateWindow("static", NULL, WS_CHILD | WS_VISIBLE,
        10, clients[clientSocket]->id * offset, 150, offset, g_hDlg, NULL, hInst, NULL);
    SendMessage(clients[clientSocket]->hText, WM_SETTEXT, 0, (LPARAM)clients[clientSocket]->file.c_str());

    clients[clientSocket]->hWnd = CreateWindowEx(NULL, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        100, clients[clientSocket]->id * offset, 200, offset, g_hDlg, NULL, hInst, NULL);

    //파일 내용 수신
    retval = recvn(clientSocket, buf, len, NULL, fp);
    if (retval == SOCKET_ERROR) {
        err_display("recv()");
        return retval;
    }
    else if (retval == 0)
        return retval;

    ::DestroyWindow(clients[clientSocket]->hWnd);
    ::DestroyWindow(clients[clientSocket]->hText);
}

void err_quit(const char* msg)
{
    LPVOID lpMsgBuf;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, WSAGetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf, 0, NULL);
    MessageBox(NULL, (LPCTSTR)lpMsgBuf, (LPCSTR)msg, MB_ICONERROR);
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