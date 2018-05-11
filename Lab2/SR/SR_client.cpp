// GBN_client.cpp :  �������̨Ӧ�ó������ڵ㡣 
// 
//#include "stdafx.h" 
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h> 
#include <WinSock2.h> 
#include <time.h> 
#include <fstream> 
#pragma comment(lib,"ws2_32.lib") 

#define SERVER_PORT  12340 //�������ݵĶ˿ں� 
#define SERVER_IP    "127.0.0.1" //  �������� IP ��ַ 

const int BUFFER_LENGTH = 1026;
const int SEQ_SIZE = 20;//���ն����кŸ�����Ϊ 1~20 
const int RECV_WIND_SIZE = 10;//���մ��ڵĴ�С����ҪС�ڵ�����Ŵ�С��һ��


							  /****************************************************************/
							  /*            -time  �ӷ������˻�ȡ��ǰʱ��
							  -quit  �˳��ͻ���
							  -testgbn [X]  ���� GBN Э��ʵ�ֿɿ����ݴ���
							  [X] [0,1]  ģ�����ݰ���ʧ�ĸ���
							  [Y] [0,1]  ģ�� ACK ��ʧ�ĸ���
							  */
							  /****************************************************************/
void printTips() {
	printf("*****************************************\n");
	printf("| -time to get current time           |\n");
	printf("| -quit to exit client                |\n");
	printf("| -testgbn [X] [Y] to test the gbn    |\n");
	printf("*****************************************\n");
}

//************************************ 
// Method:        lossInLossRatio 
// FullName:    lossInLossRatio 
// Access:        public   
// Returns:      BOOL 

//  Qualifier:  ���ݶ�ʧ���������һ�����֣��ж��Ƿ�ʧ,��ʧ�򷵻�TRUE�����򷵻� FALSE
// Parameter: float lossRatio [0,1] 
//************************************ 
BOOL lossInLossRatio(float lossRatio) {
	/*int lossBound = (int)(lossRatio * 100);
	int r = rand() % 101;//��101��mdzz
	if (r <= lossBound) {
	return TRUE;
	}
	return FALSE;*/
	return rand() % 100 < 5;
}

int main(int argc, char* argv[])
{
	//�����׽��ֿ⣨���룩 
	WORD wVersionRequested;
	WSADATA wsaData;
	//�׽��ּ���ʱ������ʾ 
	int err;
	//�汾 2.2 
	wVersionRequested = MAKEWORD(2, 2);
	//���� dll �ļ� Scoket ��   
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		//�Ҳ��� winsock.dll 
		printf("WSAStartup failed with error: %d\n", err);
		return 1;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("Could not find a usable version of Winsock.dll\n");
		WSACleanup();
	}
	else {
		printf("The Winsock 2.2 dll was found okay\n");
	}
	SOCKET socketClient = socket(AF_INET, SOCK_DGRAM, 0);
	int iMode = 1; //1����������0������ 
	ioctlsocket(socketClient, FIONBIO, (u_long FAR*) &iMode);//���������� 
	SOCKADDR_IN addrServer;
	addrServer.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(SERVER_PORT);

	//���ջ����� 
	char buffer[BUFFER_LENGTH];
	ZeroMemory(buffer, sizeof(buffer));
	int len = sizeof(SOCKADDR);
	//Ϊ�˲���������������ӣ�����ʹ��  -time  ����ӷ������˻�õ�ǰʱ��
	//ʹ��  -testgbn [X] [Y]  ���� GBN  ����[X]��ʾ���ݰ���ʧ���� 
	//                    [Y]��ʾ ACK �������� 
	printTips();
	int ret;
	int interval = 1;//�յ����ݰ�֮�󷵻� ack �ļ����Ĭ��Ϊ 1 ��ʾÿ�������� ack��0 ���߸�������ʾ���еĶ������� ack
	char cmd[128];
	float packetLossRatio = 0.2;  //Ĭ�ϰ���ʧ�� 0.2 
	float ackLossRatio = 0.2;  //Ĭ�� ACK ��ʧ�� 0.2 
							   //��ʱ����Ϊ������ӣ�����ѭ���������� 

	srand((unsigned)time(NULL));
	while (true) {
		gets_s(buffer);
		ret = sscanf(buffer, "%s%f%f", &cmd, &packetLossRatio, &ackLossRatio);
		//��ʼ GBN ���ԣ�ʹ�� GBN Э��ʵ�� UDP �ɿ��ļ����� 
		if (!strcmp(cmd, "-testgbn")) {
			printf("%s\n", "Begin to test GBN protocol, please don't abort the process");
			printf("The  loss  ratio  of  packet  is  %.2f,the  loss  ratio  of  ack is %.2f\n", packetLossRatio, ackLossRatio);
			int waitCount = 0;
			int stage = 0;
			BOOL b;
			unsigned char u_code;//״̬�� 
			unsigned short seq;//�������к� 
			unsigned short recvSeq;//��ȷ�ϵ�������к�
			unsigned short waitSeq;//�ȴ������к� �����ڴ�СΪ10�����Ϊ��С��ֵ
			char buffer_1[RECV_WIND_SIZE][BUFFER_LENGTH];//���յ��Ļ���������----------------add bylvxiya
			int i_state = 0;
			for (i_state = 0; i_state < RECV_WIND_SIZE; i_state++) {
				ZeroMemory(buffer_1[i_state], sizeof(buffer_1[i_state]));
			}
			BOOL ack_send[RECV_WIND_SIZE];//ack��������ļ�¼����Ӧ1-20��ack,�տ�ʼȫΪfalse
			int success_number = 0;// �����ڳɹ����յĸ���
			for (i_state = 0; i_state < RECV_WIND_SIZE; i_state++) {//��¼��һ���ɹ�������
				ack_send[i_state] = false;
			}
			std::ofstream out_result;
			out_result.open("result.txt", std::ios::out | std::ios::trunc);
			if (!out_result.is_open()) {
				printf("�ļ���ʧ�ܣ�����\n");
				continue;
			}
			//---------------------------------
			sendto(socketClient, "-testgbn", strlen("-testgbn") + 1, 0,
				(SOCKADDR*)&addrServer, sizeof(SOCKADDR));
			bool runFlag = true;
			int receiveSize;
			while (runFlag)
			{
				switch (stage) {
				case 0://�ȴ����ֽ׶� 
					recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, &len);
					u_code = (unsigned char)buffer[0];
					if ((unsigned char)buffer[0] == 205)
					{
						printf("Ready for file transmission\n");
						buffer[0] = 200;
						buffer[1] = '\0';
						sendto(socketClient, buffer, 2, 0,
							(SOCKADDR*)&addrServer, sizeof(SOCKADDR));
						stage = 1;
						recvSeq = 0;
						waitSeq = 1;
					}
					break;
				case 1://�ȴ��������ݽ׶� 
					receiveSize = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, &len);
					if (receiveSize > 0) {
						if (!memcmp(buffer, "ojbk\0", 5)) {
							printf("���ݴ��������\n");
							stage = 2;//�������ģʽ
							waitCount = 21;//�����Ϳ���ֱ�ӷ�һ��end ANS�ˣ�������ʡ��һ��
							continue;
						}
						seq = (unsigned short)buffer[0];
						//�����ģ����Ƿ�ʧ 
						b = lossInLossRatio(packetLossRatio);
						if (b) {
							printf("The packet with a seq of %d loss\n", seq - 1);
							Sleep(500);
							continue;
						}
						//printf("recv a packet with a seq of %d\n", seq);


						int window_seq = (seq - waitSeq+SEQ_SIZE)% SEQ_SIZE;
						if (window_seq >= 0 && window_seq < RECV_WIND_SIZE && !ack_send[window_seq]) {
							printf("recv a packet with a seq of %d\n", seq - 1);
							ack_send[window_seq] = true;
							memcpy(buffer_1[window_seq], buffer + 1, sizeof(buffer) - 1);
							int ack_s = 0;
							while (ack_send[ack_s] && ack_s < RECV_WIND_SIZE) {
								//���ϲ㴫������							
								out_result << buffer_1[ack_s];
								//printf("%s",buffer_1[ack_s - 1]);
								ZeroMemory(buffer_1[ack_s], sizeof(buffer_1[ack_s]));
								waitSeq++;
								if (waitSeq == 21) {
									waitSeq = 1;
								}
								ack_s = ack_s + 1;
							}
							if (ack_s > 0) {
								for (int i = 0; i < RECV_WIND_SIZE; i++) {
									if (ack_s + i < RECV_WIND_SIZE)
									{
										ack_send[i] = ack_send[i + ack_s];
										memcpy(buffer_1[i], buffer_1[i + ack_s], sizeof(buffer_1[i + ack_s]));
										ZeroMemory(buffer_1[i + ack_s], sizeof(buffer_1[i + ack_s]));
									}
									else
									{
										ack_send[i] = false;
										ZeroMemory(buffer_1[i], sizeof(buffer_1[i]));
									}

								}
							}

							//������ڴ��İ��ķ�Χ����ȷ���գ�����ȷ�ϼ��ɣ����������Χ��һ�����߼��������ֵ�ϲ�һ������ֱ�ӻ�Ӧack 

							//������� 
							//printf("%s\n",&buffer[1]); 
							buffer[0] = seq;
							recvSeq = seq;
							buffer[1] = '\0';
						}
						else {

							//�����ǰһ������û���յ�����ȴ� Seq Ϊ 1 �����ݰ��������򲻷��� ACK����Ϊ��û����һ����ȷ�� ACK��
							if (!recvSeq) {
								continue;
							}
							buffer[0] = seq;//����յ��˹�ʱ�İ���˵��ACK���ˣ��Ͳ�һ��ACK��������
							buffer[1] = '\0';
						}
						b = lossInLossRatio(ackLossRatio);
						if (b) {
							printf("The  ack  of  %d  loss\n", (unsigned char)buffer[0] - 1);
							Sleep(500);
							continue;
						}
						sendto(socketClient, buffer, 2, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
						printf("send a ack of %d\n", (unsigned char)buffer[0] - 1);
					}
					break;
				case 2:
					memcpy(buffer, "otmspbbjbk\0", 11);
					sendto(socketClient, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
					printf("send a end ANSWER\n");
					Sleep(500);



					goto success;//���������Ҳ��֪��������˭������ģ������ͼ̳й�����
					break;
				}
				Sleep(500);
			}
		success:			out_result.close();
		}
		sendto(socketClient, buffer, strlen(buffer) + 1, 0,
			(SOCKADDR*)&addrServer, sizeof(SOCKADDR));

		ret = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, &len);
		printf("%s\n", buffer);
		if (!strcmp(buffer, "Good bye!")) {
			break;
		}
		printTips();
	}
	//�ر��׽��� 
	closesocket(socketClient);
	WSACleanup();
	return 0;
}