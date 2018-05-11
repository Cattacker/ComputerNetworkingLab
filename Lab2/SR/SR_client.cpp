// GBN_client.cpp :  定义控制台应用程序的入口点。 
// 
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h> 
#include <WinSock2.h> 
#include <time.h> 
#include <fstream> 
#pragma comment(lib,"ws2_32.lib") 

#define SERVER_PORT  12340 //接收数据的端口号 
#define SERVER_IP    "127.0.0.1" //  服务器的 IP 地址 

const int BUFFER_LENGTH = 1027;
const int SEQ_SIZE = 20;//接收端序列号个数，为 1~20 
const int RECV_WIND_SIZE = 10;//接收窗口的大小，他要小于等于序号大小的一半
const int SEND_WIND_SIZE = 10;
const int NO_ACK = 88;
const int NO_SEQ = 89;
int recv_ack[20];//收到ack的情况
				 //0：已经发过seq，但没有收到ACK
				 //1：空闲
				 //2：已经超时，正在等待重传
				 //3：已经收到ACK，但是还在窗口内
int remainingPacket;
int curAck;//当前等待对方发过来确认的 ack 
int ack_going_to_send = NO_ACK;
int totalAck = 0;

/****************************************************************/
/*            -time  从服务器端获取当前时间
-quit  退出客户端
-testgbn [X]  测试 GBN 协议实现可靠数据传输
[X] [0,1]  模拟数据包丢失的概率
[Y] [0,1]  模拟 ACK 丢失的概率
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
//  Qualifier:  根据丢失率随机生成一个数字，判断是否丢失,丢失则返回TRUE，否则返回 FALSE
// Parameter: float lossRatio [0,1] 
//************************************ 
BOOL lossInLossRatio(float lossRatio) {
	/*int lossBound = (int)(lossRatio * 100);
	int r = rand() % 101;//还101，mdzz
	if (r <= lossBound) {
	return TRUE;
	}
	return FALSE;*/
	return rand() % 100 < 5;
}

int seqIsAvailable() {
	for (int i = 0; i < SEND_WIND_SIZE && i<remainingPacket; ++i) {
		int index = (i + curAck) % SEQ_SIZE;
		if (recv_ack[index] == 1 || recv_ack[index] == 2) {
			return index;
		}
	}
	return -1;
}

void ackHandler(char c) {
	unsigned char index = (unsigned char)c - 1; //序列号减一 ，表示对方确认收到了index号包
	printf("Recv a ack of %d\n", index);
	// 收到的ACK>=等待的ACK 且 收到的ACK在窗口内（ 窗口跨过了end->0，则一定在内 或 真的在内 ）

	//if (curAck <= index && (curAck + SEND_WIND_SIZE >= SEQ_SIZE ? true : index<curAck + SEND_WIND_SIZE)) 
	if ((curAck + SEND_WIND_SIZE >= SEQ_SIZE) ? (curAck <= index || index <(curAck + SEND_WIND_SIZE + SEQ_SIZE) % SEQ_SIZE) : (curAck <= index && index<curAck + SEND_WIND_SIZE))
	{

		recv_ack[index] = 3;
		while (recv_ack[curAck] == 3) {
			recv_ack[curAck] = 1;
			curAck = (curAck + 1) % SEQ_SIZE;
			totalAck++;
			remainingPacket--;

		}
	}
}

int main(int argc, char* argv[])
{
	//加载套接字库（必须） 
	WORD wVersionRequested;
	WSADATA wsaData;
	//套接字加载时错误提示 
	int err;
	//版本 2.2 
	wVersionRequested = MAKEWORD(2, 2);
	//加载 dll 文件 Scoket 库   
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		//找不到 winsock.dll 
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
	int iMode = 1; //1：非阻塞，0：阻塞 
	ioctlsocket(socketClient, FIONBIO, (u_long FAR*) &iMode);//非阻塞设置 
	SOCKADDR_IN addrServer;
	addrServer.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(SERVER_PORT);
	//接收缓冲区 
	char buffer[BUFFER_LENGTH];
	ZeroMemory(buffer, sizeof(buffer));
	int len = sizeof(SOCKADDR);
	//为了测试与服务器的连接，可以使用  -time  命令从服务器端获得当前时间
	//使用  -testgbn [X] [Y]  测试 GBN  其中[X]表示数据包丢失概率 
	//                    [Y]表示 ACK 丢包概率 
	printTips();
	int ret;
	int interval = 1;//收到数据包之后返回 ack 的间隔，默认为 1 表示每个都返回 ack，0 或者负数均表示所有的都不返回 ack
	char cmd[128];
	float packetLossRatio = 0.2;  //默认包丢失率 0.2 
	float ackLossRatio = 0.2;  //默认 ACK 丢失率 0.2 
							   //用时间作为随机种子，放在循环的最外面 
	srand((unsigned)time(NULL));
	while (true) {
		gets_s(buffer);
		ret = sscanf(buffer, "%s%f%f", &cmd, &packetLossRatio, &ackLossRatio);
		//开始 GBN 测试，使用 GBN 协议实现 UDP 可靠文件传输 
		if (!strcmp(cmd, "-testgbn")) {
			printf("%s\n", "Begin to test GBN protocol, please don't abort the process");
			printf("The  loss  ratio  of  packet  is  %.2f,the  loss  ratio  of  ack is %.2f\n", packetLossRatio, ackLossRatio);
			int waitCount = 0;
			int stage = 0;
			BOOL b;
			unsigned char u_code;//状态码 
			unsigned short seq;//包的序列号 
			unsigned short recvSeq;//已确认的最大序列号
			unsigned short waitSeq;//等待的序列号 ，窗口大小为10，这个为最小的值
			char buffer_1[RECV_WIND_SIZE][BUFFER_LENGTH];//接收到的缓冲区数据----------------add bylvxiya
			int i_state = 0;
			for (i_state = 0; i_state < RECV_WIND_SIZE; i_state++) {
				ZeroMemory(buffer_1[i_state], sizeof(buffer_1[i_state]));
			}

			BOOL ack_send[RECV_WIND_SIZE];//ack发送情况的记录，对应1-20的ack,刚开始全为false
			int seqflag = 0;
			int success_number = 0;// 窗口内成功接收的个数
			for (i_state = 0; i_state < RECV_WIND_SIZE; i_state++) {//记录哪一个成功接收了
				ack_send[i_state] = false;
			}
			std::ofstream out_result;
			out_result.open("result.txt", std::ios::out | std::ios::trunc);
			if (!out_result.is_open()) {
				printf("文件打开失败！！！\n");
				continue;
			}
			//---------------------------------
			sendto(socketClient, "-testgbn", strlen("-testgbn") + 1, 0,
				(SOCKADDR*)&addrServer, sizeof(SOCKADDR));
			bool runFlag = true;
			int receiveSize;
			int nowSeq;
			while (runFlag)
			{
				switch (stage) {
				case 0://等待握手阶段 
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
				case 1://等待接收数据阶段 
					receiveSize = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, &len);
					if (receiveSize > 0) {
						if (!memcmp(buffer, "ojbk\0", 5)) {
							printf("数据传输结束！\n");
							stage = 2;//进入结束模式
							waitCount = 21;//这样就可以直接发一个end ANS了，就是懒省事一下
							continue;
						}
						seq = (unsigned short)buffer[0];
						//随机法模拟包是否丢失 
						b = lossInLossRatio(packetLossRatio);
						if (b) {
							printf("The packet with a seq of %d loss\n", seq - 1);
							Sleep(500);
							continue;
						}
						//printf("recv a packet with a seq of %d\n", seq);

						int window_seq = (seq - waitSeq + SEQ_SIZE) % SEQ_SIZE;
						if (window_seq >= 0 && window_seq < RECV_WIND_SIZE && !ack_send[window_seq]) {
							printf("recv a packet with a seq of %d\n", seq - 1);
							//printf("----------Recv a ack of %d\n", buffer[1] - 1);
							ack_send[window_seq] = true;
							memcpy(buffer_1[window_seq], buffer + 3, sizeof(buffer) - 3);
							int ack_s = 0;
							while (ack_send[ack_s] && ack_s < RECV_WIND_SIZE) {
								//向上层传输数据							
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

							//如果是期待的包的范围，正确接收，正常确认即可，如果超出范围（一定是逻辑上落后，数值上不一定），直接回应ack 

							//输出数据 
							//printf("%s\n",&buffer[1]); 
							//buffer[1] = seq;
							ack_going_to_send = seq - 1;
							seqflag++;
							if (seqflag == 1)
							{
								int asfafasf;
								asfafasf = 0;
							}
							if (seqflag == 21)
								seqflag = 1;
							//buffer[0] = seqflag;
							recvSeq = seq;
							//buffer[2] = '\0';
							//
						}
						else {
							//如果当前一个包都没有收到，则等待 Seq 为 1 的数据包，不是则不返回 ACK（因为并没有上一个正确的 ACK）
							if (!recvSeq) {
								continue;
							}
							//buffer[1] = seq;//如果收到了过时的包，说明ACK丢了，就补一个ACK给服务器
							ack_going_to_send = seq;
							seqflag++;
							if (seqflag = 21)
								seqflag = 1;
							//buffer[0] = seqflag;
							//buffer[2] = '\0';
						}
						b = lossInLossRatio(ackLossRatio);
						if (b) {
							printf("The  ack  of  %d  loss\n", (unsigned char)buffer[0] - 1);
							Sleep(500);
							continue;
						}
						//sendto(socketClient, buffer, 2, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
						//printf("send a ack of %d\n", (unsigned char)buffer[1] - 1);
						//printf("------------send a seq of %d\n", (unsigned char)buffer[0] - 1);

						//对收到的ACK进行处理
						int now_recv_ack = buffer[1];
						if (now_recv_ack != NO_ACK) {
							ackHandler(now_recv_ack);
							printf("recv ACK %d", now_recv_ack);
						}

					}
					else {
						//这里添加timeout
					}

					nowSeq = seqIsAvailable();
					if (nowSeq >= 0) {
						//发送给客户端的序列号从 1 开始 
						buffer[0] = nowSeq + 1;
						recv_ack[nowSeq] = 0;

						buffer[1] = ack_going_to_send;
						//数据发送的过程中应该判断是否传输完成 
						//为简化过程此处并未实现 
						//memcpy(&buffer[1], data + 1024 * (curSeq + (totalSeq / SEND_WIND_SIZE)*SEND_WIND_SIZE), 1024);
						//memcpy(&buffer[2], data + 1024 * (totalAck + nowSeq - curAck), 1024);
						printf("send a packet with a seq of %d\n", nowSeq);
						printf("----------send ack %d\n", ack_going_to_send - 1);
						sendto(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
						ack_going_to_send = NO_ACK;
						//++curSeq;
						//curSeq %= SEQ_SIZE;
						//++totalSeq;
						Sleep(500);
					}
					else if (ack_going_to_send != NO_ACK) {//这里添加如果不需要发包但需要发ack
														   //发送给客户端的序列号从 1 开始 
														   //buffer[0] = nowSeq + 1;
														   //ack[nowSeq] = 0;
						buffer[0] = NO_SEQ;//不发sequence

						buffer[1] = ack_going_to_send;
						buffer[2] = '\0';
						//数据发送的过程中应该判断是否传输完成 
						//为简化过程此处并未实现 
						//memcpy(&buffer[1], data + 1024 * (curSeq + (totalSeq / SEND_WIND_SIZE)*SEND_WIND_SIZE), 1024);
						//memcpy(&buffer[2], data + 1024 * (totalAck + nowSeq - curAck), 1024);
						//printf("send a packet with a seq of %d\n", nowSeq);
						printf("----------send ack %d\n", ack_going_to_send - 1);
						sendto(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
						ack_going_to_send = NO_ACK;
						//++curSeq;
						//curSeq %= SEQ_SIZE;
						//++totalSeq;
						Sleep(500);
					}


					break;
				case 2:
					memcpy(buffer, "otmspbbjbk\0", 11);
					sendto(socketClient, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
					printf("send a end ANSWER\n");
					Sleep(500);


					goto success;//这个操作我也不知道最早是谁想出来的，反正就继承过来了
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
	//关闭套接字 
	closesocket(socketClient);
	WSACleanup();
	return 0;
}