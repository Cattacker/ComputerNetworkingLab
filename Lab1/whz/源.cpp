#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include <string.h>
#include <tchar.h>
#pragma comment(lib,"Ws2_32.lib")
#define MAXSIZE 65507 //�������ݱ��ĵ���󳤶�
#define HTTP_PORT 80 //http �������˿�
#define cache_max 100

//Http ��Ҫͷ������

struct HttpHeader{
    char method[4]; // POST ���� GET��ע����ЩΪ CONNECT����ʵ���ݲ�����
    char url[1024]; // ����� url
	char host[1024]; // Ŀ������
    char cookie[1024 * 10]; //cookie
    HttpHeader(){
        ZeroMemory(this,sizeof(HttpHeader));
    }
};

struct Cache{
	char url[1024];
	char buffer[65507];
	char date[30];
	char host[1024];
};

int cache_flag=0;
struct Cache cache[cache_max];

BOOL InitSocket();
void ParseHttpHead(char *buffer,HttpHeader * httpHeader,char *IP_addr);
BOOL ConnectToServer(SOCKET *serverSocket,char *host);
unsigned int __stdcall ProxyThread(LPVOID vp);
bool ForbiddenToConnect(char *httpheader,char * forbiddernUrl);//��վ����

//������ز���
SOCKET ProxyServer;
sockaddr_in ProxyServerAddr;
sockaddr_in clientAddr;
const int ProxyPort = 10240;
char *gtodayhit="GET http://today.hit.edu.cn/ HTTP/1.1";
char *ptodayhit="POST http://today.hit.edu.cn/ HTTP/1.1";
char *ssina="GET http://www.sina.com.cn/ HTTP/1.1";
char *IP1="172.20.21.110";
//�����µ����Ӷ�ʹ�����߳̽��д������̵߳�Ƶ���Ĵ����������ر��˷���Դ
//����ʹ���̳߳ؼ�����߷�����Ч��
//const int ProxyThreadMaxNum = 20;
//HANDLE ProxyThreadHandle[ProxyThreadMaxNum] = {0};
//DWORD ProxyThreadDW[ProxyThreadMaxNum] = {0};

struct ProxyParam{
    SOCKET clientSocket;
    SOCKET serverSocket;
};

struct VP{
    ProxyParam *lpProxyParam;
    char *IP_addr;
};

int _tmain(int argc, _TCHAR* argv[])
{
    printf("�����������������\n");
    printf("��ʼ��...\n");
    if(!InitSocket()){
        printf("socket ��ʼ��ʧ��\n");
        return -1;
    }
    printf("����������������У������˿� %d\n",ProxyPort);
    SOCKET acceptSocket = INVALID_SOCKET;
    ProxyParam *lpProxyParam;
    HANDLE hThread;
    DWORD dwThreadID;
    //������������ϼ���
    while(true){
		int len=sizeof(clientAddr);
        acceptSocket = accept(ProxyServer,(sockaddr *)&clientAddr,&len);
		char *a=inet_ntoa(clientAddr.sin_addr);//��ȡip
		printf("%s\n",a);

		//ProxyServerAddr
        lpProxyParam = new ProxyParam;
        if(lpProxyParam == NULL){
			continue;
		}
		lpProxyParam->clientSocket = acceptSocket;
		VP *vp=(struct VP*)malloc(sizeof(struct VP));
		vp->IP_addr=a;
		vp->lpProxyParam=lpProxyParam;
		hThread = (HANDLE)_beginthreadex(NULL, 0,&ProxyThread,(LPVOID)vp, 0, 0);
		CloseHandle(hThread);
	    Sleep(200);
    }
    closesocket(ProxyServer);
    WSACleanup();
    return 0;
}

//************************************
// Method: InitSocket
// FullName: InitSocket
// Access: public
// Returns: BOOL
// Qualifier: ��ʼ���׽���
//************************************

BOOL InitSocket(){
    //�����׽��ֿ⣨���룩
    WORD wVersionRequested;
    WSADATA wsaData;
	//�׽��ּ���ʱ������ʾ
    int err;
    //�汾 2.2
    wVersionRequested = MAKEWORD(2, 2);
    //���� dll �ļ� Socket ��
    err = WSAStartup(wVersionRequested, &wsaData);
    if(err != 0){
        //�Ҳ��� winsock.dll
        printf("���� winsock ʧ�ܣ��������Ϊ: %d\n", WSAGetLastError());
        return FALSE;
    }
    if(LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) !=2)
    {
        printf("�����ҵ���ȷ�� winsock �汾\n");
        WSACleanup();
        return FALSE;
    }
    ProxyServer= socket(AF_INET, SOCK_STREAM, 0);
    if(INVALID_SOCKET == ProxyServer){
        printf("�����׽���ʧ�ܣ��������Ϊ��%d\n",WSAGetLastError());
        return FALSE;
    }
    ProxyServerAddr.sin_family = AF_INET;
    ProxyServerAddr.sin_port = htons(ProxyPort);
    ProxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY;
    if(bind(ProxyServer,(SOCKADDR*)&ProxyServerAddr,sizeof(SOCKADDR)) == SOCKET_ERROR){
        printf("���׽���ʧ��\n");
        return FALSE;
    }
    if(listen(ProxyServer, SOMAXCONN) == SOCKET_ERROR){
        printf("�����˿�%d ʧ��",ProxyPort);
        return FALSE;
    }
    return TRUE;
}

//************************************
// Method: ProxyThread
// FullName: ProxyThread
// Access: public
// Returns: unsigned int __stdcall
// Qualifier: �߳�ִ�к���
// Parameter: LPVOID lpParameter
//************************************

unsigned int __stdcall ProxyThread(LPVOID vp){
	ProxyParam *lpParameter=((struct VP *)vp)->lpProxyParam;
	char *IP_addr=((struct VP *)vp)->IP_addr;
    char Buffer[MAXSIZE];
    char *CacheBuffer;
    ZeroMemory(Buffer,MAXSIZE);
    SOCKADDR_IN clientAddr;
	//printf("%s\n",IP_addr);
    int length = sizeof(SOCKADDR_IN);
    int recvSize;
    int ret;
    recvSize = recv(((ProxyParam*)lpParameter)->clientSocket,Buffer,MAXSIZE,0);
    if(recvSize <= 0){
        goto error;
    }
    HttpHeader* httpHeader = new HttpHeader();
    CacheBuffer = new char[recvSize + 1];
    ZeroMemory(CacheBuffer,recvSize + 1);
    memcpy(CacheBuffer,Buffer,recvSize);
    ParseHttpHead(CacheBuffer,httpHeader,IP_addr);
    delete CacheBuffer;
	//printf("%s\n",httpHeader->url);
	//printf("%s\n",httpHeader->host);
	//printf("%s\n",httpHeader->method);
    if(!ConnectToServer(&((ProxyParam*)lpParameter)->serverSocket,httpHeader->host)) {
        goto error;
    }
    printf("������������ %s �ɹ�\n",httpHeader->host);
    //���ͻ��˷��͵� HTTP ���ݱ���ֱ��ת����Ŀ�������
	
	int found = 0;
	for (int i = 0; i < cache_max; ++i) {
		if (cache[i].url != NULL && strcmp(httpHeader->url, cache[i].url) == 0) {
			found = 1;
			int not_modified = 1;
			int length = strlen(Buffer);
			char * pr = Buffer + length;
			memcpy(pr, "If-modified-since: ", 19);
			pr += 19;
			length = strlen(cache[i].date);
			memcpy(pr, cache[i].date, length);
			ret = send(((ProxyParam *)lpParameter)->serverSocket, Buffer, strlen(Buffer) + 1, 0);

			recvSize = recv(((ProxyParam *)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
			if (recvSize <= 0) {
				goto error;
			}

			const char *blank = " ";
			const char *Modd = "304";
			printf("----------%c%c%c\n",Buffer[9],Buffer[10],Buffer[11]);
			if (strcmp(httpHeader->url, "http://today.hit.edu.cn/") == 0) {
				int cccc;
				cccc=0;
			}
			if (!memcmp(&Buffer[9], Modd, strlen(Modd)))
			{
				printf("-------���治��Ҫ����\n");
				ret = send(((ProxyParam*)lpParameter)->clientSocket, cache[i].buffer, strlen(cache[i].buffer) + 1, 0);
				break;
			}
			printf("-------������Ҫ����\n");
			char *cacheBuff = new char[MAXSIZE];
			ZeroMemory(cacheBuff, MAXSIZE);
			memcpy(cacheBuff, Buffer, MAXSIZE);
			const char *delim = "\r\n";
			char *ptr;
			//char dada[DATELENGTH];
			//ZeroMemory(dada, sizeof(dada));
			char *p = strtok_s(cacheBuff, delim, &ptr);
			while (p) {
				if (p[0] == 'L') {
					if (strlen(p) > 15) {
						if (!(strcmp(cache[i].date, "Last-Modified:")))
						{
							memcpy(cache[i].date, &p[15], strlen(p) - 15);
							not_modified = 0;
							break;
						}
					}
				}
				p = strtok_s(NULL, delim, &ptr);
			}
			memcpy(cache[i].url, httpHeader->url, sizeof(httpHeader->url));
			memcpy(cache[i].buffer, Buffer, sizeof(Buffer));
			ret = send(((ProxyParam*)lpParameter)->clientSocket, Buffer, sizeof(Buffer), 0);
			break;
		}
	}
	if (!found) {
		//���ͻ��˷��͵� HTTP ���ݱ���ֱ��ת����Ŀ�������
		printf("-------�޻���\n");
		ret = send(((ProxyParam *)lpParameter)->serverSocket, Buffer, strlen(Buffer) + 1, 0);
		//�ȴ�Ŀ���������������
		recvSize = recv(((ProxyParam*)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
		if (recvSize <= 0) {
			goto error;
		}
		//��Ŀ����������ص�����ֱ��ת�����ͻ���
		ret = send(((ProxyParam*)lpParameter)->clientSocket, Buffer, sizeof(Buffer), 0);
		//if (strcmp(httpHeader->url, "http://today.hit.edu.cn/") == 0) {
			cache_flag = (cache_flag + 1) % cache_max;
			memcpy(cache[cache_flag].url, httpHeader->url, sizeof(httpHeader->url));
			memcpy(cache[cache_flag].buffer, Buffer, sizeof(Buffer));

			char *cacheBuff = new char[MAXSIZE];
			ZeroMemory(cacheBuff, MAXSIZE);
			memcpy(cacheBuff, Buffer, MAXSIZE);
			const char *delim = "\r\n";
			char *ptr;
			//char dada[DATELENGTH];
			//ZeroMemory(dada, sizeof(dada));
			char *p = strtok_s(cacheBuff, delim, &ptr);
			while (p) {
				if (p[0] == 'L') {
					if (strlen(p) > 15) {
						memcpy(cache[cache_flag].date, &p[15], strlen(p) - 15);
						break;
					}
				}
				p = strtok_s(NULL, delim, &ptr);
			}
		//}	
	}

    error:
        printf("�ر��׽���\n");
        Sleep(200);
        closesocket(((ProxyParam*)lpParameter)->clientSocket);
        closesocket(((ProxyParam*)lpParameter)->serverSocket);
        delete lpParameter;
        _endthreadex(0);
    return 0;
}

//************************************
// Method: ParseHttpHead
// FullName: ParseHttpHead
// Access: public
// Returns: void
// Qualifier: ���� TCP �����е� HTTP ͷ��
// Parameter: char * buffer
// Parameter: HttpHeader * httpHeader
//************************************

void ParseHttpHead(char *buffer,HttpHeader * httpHeader,char *IP_addr){
    char *p;
    char *ptr;
    const char * delim = "\r\n";
	printf("%s\n",IP_addr);
    p = strtok_s(buffer,delim,&ptr);//��ȡ��һ��
		//p="GET http://today.hit.edu.cn/ HTTP/1.1";
    printf("%s\n",p);

	/*if(cg==2 && !strcmp(p, ssina)){
		return;
	}*/
	/*int sp=1;
	if(!strcmp(IP_addr, IP1) && !strcmp(p,ssina))
	{
		p=gtodayhit;
		sp=0;
	}*/

    if(p[0] == 'G'){//GET ��ʽ
        memcpy(httpHeader->method,"GET",3);
		memcpy(httpHeader->url,&p[4],strlen(p) -13);
    }else if(p[0] == 'P'){//POST ��ʽ
        memcpy(httpHeader->method,"POST",4);
        memcpy(httpHeader->url,&p[5],strlen(p) - 14);
    }

	printf("��ǰIP��%s\n",IP_addr);
	printf("����IP��%s\n",IP1);
	if(!strcmp(IP_addr, IP1) && !ForbiddenToConnect(httpHeader->url,"jwes")) //�û�����
	{
		printf("������վ�ĵ�ַ��%s\n",httpHeader->url);
		return;
	}
		

    printf("%s\n",httpHeader->url);
    p = strtok_s(NULL,delim,&ptr);
    while(p){
        switch(p[0]){
            case 'H'://Host
			/*if (strcmp(httpHeader->url, "http://today.hit.edu.com.cn/") == 0) 
			{
				int xccc;
				xccc=0;
			}
			if(sp==0)*/
			//p="Host: today.hit.edu.cn";
            memcpy(httpHeader->host,&p[6],strlen(p) - 6);
            break;
            case 'C'://Cookie
            if(strlen(p) > 8){
                char header[8];
                ZeroMemory(header,sizeof(header));
                memcpy(header,p,6);
                if(!strcmp(header,"Cookie")){
                    memcpy(httpHeader->cookie,&p[8],strlen(p) -8);
                }
            }
            break;
            default:
            break;
        }
        p = strtok_s(NULL,delim,&ptr);
    }
}

//************************************
// Method: ConnectToServer
// FullName: ConnectToServer
// Access: public
// Returns: BOOL
// Qualifier: ������������Ŀ��������׽��֣�������
// Parameter: SOCKET * serverSocket
// Parameter: char * host
//************************************

BOOL ConnectToServer(SOCKET *serverSocket,char *host){
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(HTTP_PORT);
    HOSTENT *hostent = gethostbyname(host);
    if(!hostent){
        return FALSE;
    }
    in_addr Inaddr=*( (in_addr*) *hostent->h_addr_list);
    serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));
    *serverSocket = socket(AF_INET,SOCK_STREAM,0);
    if(*serverSocket == INVALID_SOCKET){
        return FALSE;
    }
    if(connect(*serverSocket,(SOCKADDR *)&serverAddr,sizeof(serverAddr)) == SOCKET_ERROR){
        closesocket(*serverSocket);
        return FALSE;
    }
    return TRUE;
}

bool ForbiddenToConnect(char *httpheader,char * forbiddernUrl)
{
    //char * forbiddernUrl = "jwes"; //���εĺ��йؼ��ֵ���ַ
    if (strstr(httpheader, forbiddernUrl)!=NULL) //�Ƿ������εĹؼ���
    {
        return false;
    }
    else return true;
}

int ParseHttpHead0(char *buffer, HttpHeader *httpHeader) {
    int flag = 0;//���ڱ�ʾCache�Ƿ����У�����Ϊ1��������Ϊ0
    char *p;
    char *ptr;
    const char *delim = "\r\n";//�س����з�                           
    p = strtok_s(buffer, delim, &ptr);
    if (p[0] == 'G') {  //GET��ʽ
        memcpy(httpHeader->method, "GET", 3);
        memcpy(httpHeader->url, &p[4], strlen(p) - 13);
        printf("url��%s\n", httpHeader->url);//url                                       
        for (int i = 0; i < cache_max; i++) {//����cache������ǰ���ʵ�url�Ƿ��Ѿ�����cache����
            if (strcmp(cache[i].url, httpHeader->url) == 0) {//˵��url��cache���Ѿ�����
                flag = 1;   //ֻҪ���ڣ�flag��ʶ������Ϊ1
                break;
            }
        }
        if (!flag) {
			memcpy(cache[(++cache_flag)%cache_max].url, &p[4], strlen(p) - 13);
        }
    }
    else if (p[0] == 'P') { //POST��ʽ
        memcpy(httpHeader->method, "POST", 4);
        memcpy(httpHeader->url, &p[5], strlen(p) - 14);
        for (int i = 0; i < cache_max; i++) {
            if (strcmp(cache[i].url, httpHeader->url) == 0) { //ͬ��
                flag = 1;
                break;
            }
        }
        if (!flag) {
			memcpy(cache[(++cache_flag)%cache_max].url, &p[4], strlen(p) - 13);
        }
    }
    p = strtok_s(NULL, delim, &ptr);
    while (p) {
        switch (p[0]) {
        case 'H'://HOST
            memcpy(httpHeader->host, &p[6], strlen(p) - 6);
            if (!flag) {
                memcpy(cache[cache_flag].host, &p[6], strlen(p) - 6);
			}
            break;
        case 'C'://Cookie
            if (strlen(p) > 8) {
                char header[8];
                ZeroMemory(header, sizeof(header));
                memcpy(header, p, 6);
                if (!strcmp(header, "Cookie")) {
                    memcpy(httpHeader->cookie, &p[8], strlen(p) - 8);
                }
            }
            break;
            //case '':
		case 'L':
			if (strlen(p) > 15) {
				char header[15];
				ZeroMemory(header, sizeof(header));
				memcpy(header, p, 14);
				if (!(strcmp(header, "Last-Modified:")))
				{
					memcpy(cache[cache_flag].date, &p[15], strlen(p) - 15);
					break;
				}
			}
			break;
        default:
            break;
        }
        p = strtok_s(NULL, delim, &ptr);
    }
    return flag;
}