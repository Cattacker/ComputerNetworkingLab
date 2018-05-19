/* 
 * THIS FILE IS FOR IP FORWARD TEST
 */
#include "sysInclude.h"
#include <iostream>
#include <stdlib.h>
using std::vector;
using std::cout; 
// system support 
extern void fwd_LocalRcv(char *pBuffer, int length); 

extern void fwd_SendtoLower(char *pBuffer, int length, unsigned int nexthop); 

extern void fwd_DiscardPkt(char *pBuffer, int type); 

extern unsigned int getIpv4Address( ); 

// implemented by students 

struct routeTable 
{ 
    unsigned int destIP; //目的地址IP
    unsigned int mask; //子网掩码
    unsigned int masklen; //子网掩码长度
    unsigned int nexthop; //下一跳地址
}; 
vector<routeTable> m_table;  //路由表

//初始化路由表 直接清空vector
void stud_Route_Init() 
{ 
    m_table.clear();
    return;
} 

void stud_route_add(stud_route_msg *proute) 
{ 
    routeTable newTableItem;
    newTableItem.masklen = ntohl(proute->masklen);
    //int最高位为符号位 左移31位最高位为1 负数  右移补1 已经有一个1故masklen-1
    newTableItem.mask = (1<<31)>>(ntohl(proute->masklen)-1);
    //IP地址&子网掩码=子网地址 ->地址范围
    newTableItem.destIP = ntohl(proute->dest)&newTableItem.mask;
    newTableItem.nexthop = ntohl(proute->nexthop);
    m_table.push_back(newTableItem);
    return;
} 

int stud_fwd_deal(char *pBuffer, int length) 
{ 
    //第一个字节前4位版本号后四位首部长度 与0xf后为首部长度
    int IHL = pBuffer[0] & 0xf;
    //TTL在buffer的第9个字节开始
    int TTL = (int)pBuffer[8];
    //checksum在第11个字节开始
    int headerChecksum = ntohl(*(unsigned short*)(pBuffer+10));//指针强制转换
    int destIP = ntohl(*(unsigned int*)(pBuffer+16));
    
    //仅等于本机地址才接收  上一个实验要求广播地址也接收
    if(destIP == getIpv4Address())
    {
        fwd_LocalRcv(pBuffer, length);
        return 0;
    }
    
    
    
    if(TTL <= 0)
    {
        fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_TTLERROR);
        return 1;
    }
    
    
    
    
    bool isMatch = false;
    unsigned int longestMatchLen = 0;
    int bestMatch = 0;
    //遍历路由表
    for(int i = 0; i < m_table.size(); i ++)
    {
        //子网掩码1的长度大于已匹配到的最大长度 & 子网地址相同
        if(m_table[i].masklen > longestMatchLen && m_table[i].destIP == (destIP & m_table[i].mask))
        {
            bestMatch = i;
            isMatch = true;
            longestMatchLen = m_table[i].masklen;
            //cout << "find one" << endl;
        }
    }
    
    if(isMatch)  //找到匹配的地址
    {
        char *buffer = new char[length];
        memcpy(buffer,pBuffer,length);
        buffer[8]--; //TTL - 1
        int sum = 0;
        unsigned short int localCheckSum = 0;
        //首部长度以4字节为单位  checksum为2字节
        for(int j = 0; j < 2 * IHL; j ++)
        {
            //跳过checksum部分
            if (j != 5) {
                sum = sum + (buffer[j*2]<<8) + (buffer[j*2+1]);
            }
            
        }
        //进位部分继续加
        while((unsigned(sum) >> 16) != 0)
            sum = unsigned(sum) >> 16 + sum & 0xffff;
        //取反
        localCheckSum = htons(0xffff - (unsigned short int)sum);
        memcpy(buffer+10, &localCheckSum, sizeof(unsigned short));
        
        fwd_SendtoLower(buffer, length, m_table[bestMatch].nexthop);
        return 0;
    }
    else  //为找到匹配的地址
    {
        fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_NOROUTE);
        return 1;
    }
    return 1;
} 
