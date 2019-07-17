/* bully algorithm */
#include <iostream>
#include <vector>
#include <map>
#include <unistd.h>
#include <sys/wait.h>
#include <wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#include <string>
#include <string.h>
using namespace std;

#define MAXHOST 100
#define INITHOST 2
#define random(x) (rand()%x)

/* 主进程中存所有host的ID */
map<int,pid_t> AllID;/* <hostID, pid> */
vector<string>::iterator it_v;

/* 主机 */
class Hosts{
public:
	int hostID;/* ID */
	pid_t pid = -1;/* 进程id */
	int leaderID;
	int isLeader = 0;
	map<int,pid_t> OthersID;/* <hostID, pid> */

    Hosts();
    ~Hosts();
};

Hosts::Hosts(void){
	;
}

Hosts::~Hosts(void){
	;
}

void Log(int hostid, int para,const char* msg){
    char buf[128];
    if(hostid<0)
        sprintf(buf,"%s\n\n",msg);
    else if(para < 0)
        sprintf(buf,"[%d]%s\n\n",hostid,msg);   
    else
        sprintf(buf,"[%d]%s %d\n\n",hostid,msg,para);
    int fp = open("hostlog.dat",O_WRONLY|O_CREAT|O_APPEND,0666);
    write(fp,buf,strlen(buf));
    close(fp);
}

/* 
选举消息  0
存活消息  1
胜利消息  2
heartbeat 3
*/

/* 消息格式 */
/*
"msgtype:srcID:dstID:pid"
*/


/*
1、初始化所有子进程，给OthersID赋值
2、进程间通信
3、流程
*/



