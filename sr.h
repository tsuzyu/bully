#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/msg.h>
#include<fcntl.h>
#include <vector>
#include <unistd.h>
#include <sys/stat.h>
#include <ctime>
using namespace std;
#define BUFFERSIZE 512
typedef struct{
    long type;
    char mss[200];
     char timeStr[40];
    
}Msg;
void MsgQueueLog(const char* time,const char* msg, const char* path){
    char buf[2048];
    sprintf(buf,"[%s]%s\n\n",time,msg);
    int fp = open(path,O_WRONLY|O_CREAT|O_APPEND,0666);
    write(fp,buf,strlen(buf));
    close(fp);
}

// void RcvQueueLog(const char* time,const char* msg){
//     char buf[2048];
//     sprintf(buf,"[%s]%s\n\n",time,msg);
//     int fp = open("rvclog.dat",O_WRONLY|O_CREAT|O_APPEND,0666);
//     write(fp,buf,strlen(buf));
//     close(fp);
// }


void send(long type,const char mss[200])
{
    key_t key = ftok("./key",'a');
    int msgid = msgget(key,IPC_CREAT|O_WRONLY|0777);
    if(msgid<0)
    {   
        perror("msgget error!");
        exit(-1);
    }   

    Msg m;
    m.type=type;
    strcpy(m.mss,mss);
    //puts("please input your type name");
    //cout<<m.type<<" "<<m.mss<<endl;
     time_t curTime; //定义time_t变量
    struct tm * datePtr = NULL; //定义tm指针
    char timeStr[100]; //定义用于保存时间字符串的char数组
    time(&curTime); //取当前时间，其值表示从1970年1月1日00:00:00到当前时刻的秒数
    datePtr = localtime(&curTime); //把从1970-1-1零点零分到当前时间系统所偏移的秒数时间转换为日历时间

    sprintf(timeStr,"%d/%d/%d___%d:%d:%d",(1900 + datePtr->tm_year),
    (1 + datePtr->tm_mon),
    datePtr->tm_mday,
    datePtr->tm_hour,
    datePtr->tm_min,
    datePtr->tm_sec); 
    strcpy(m.mss,mss);
    strcpy(m.timeStr,timeStr);
    MsgQueueLog(timeStr,mss, "sndlog.dat");
    msgsnd(msgid,&m,sizeof(m)-sizeof(m.type),0);
}
void recv(long type,vector<string>&message)//input type
{
    key_t key = ftok("./key",'a');
   // printf("key:%x\n",key);

    int msgid = msgget(key,IPC_CREAT|O_RDONLY);
    if(msgid<0)
    {   
        perror("msgget error!");
        exit(-1);
    }   
    Msg rcv;
    int end;
    while(1)
    { 
        //msgrcv(msgid,&rcv,sizeof(rcv)-sizeof(type),type,0)
        end=msgrcv(msgid,&rcv,BUFFERSIZE,type,IPC_NOWAIT);
        if(end==-1)
            break;
        message.insert(message.end(), rcv.mss);
        //vector<string>::iterator iter=message.end();
        MsgQueueLog(rcv.timeStr,rcv.mss, "rcvlog.dat");
    }
}
