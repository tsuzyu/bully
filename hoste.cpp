/*冲突解决： 设置自己为leader了，但是队列里面还有之前的胜利消息未处理*/
/*solution 对比各个胜利消息的到达时间和“最新的设置leader操作”的时间 p1 */
#include<iostream>
#include<pthread.h>
#include<thread>
#include<stdlib.h>
#include<sys/time.h>
#include<vector>
#include<queue>
#include<list>
#include<map>
#include<unistd.h>
#include<signal.h>
#include<sched.h>
#include<linux/wait.h>
#include "sr.h"
#include "msg_queue.h"
#include "bully_al.h"

#define ALIVE_TIMEOUT 25
#define VICTORY_TIMEOUT 35
#define HEART_BEAT 15

//处理消息队列中heartbeat消息的间隔一定要小于HEART_BEAT
int pau=0;
using namespace std;

struct msg_queue msgqueue;
static int init_flag=0;
static time_t leader_update_time; //最近一次更新leader的时间

pthread_mutex_t mux_sys_state; //系统状态锁
static int sys_state=0;         //记录当前系统的状态：宕机中-1，开始选举1，normal状态0,等待胜利消息4,等待存活消息3
int pipefd[2];

static int election_cnt=0;

void wait_alive();
void wait_victory();
bool start_election();

static Hosts *local;

/*心跳消息超时机制:最近更新的时间*/
time_t conn_time_check;

/*
消息的内容,
mesg_s[0]=type,
mesg_s[1]=srcid,
mesg_s[2]=destid
*/
typedef vector<int> mesg_s; 




/* 整形转字符串 */
char* itoa(int num, char* str, int radix) {/*索引表*/
	char index[] = "0123456789ABCDEF";
	unsigned unum;/*中间变量*/
	int i = 0, j, k;
	/*确定unum的值*/
	if (radix == 10 && num<0) {/*十进制负数*/
		unum = (unsigned)-num;
		str[i++] = '-';
	}
	else unum = (unsigned)num;/*其他情况*/
							  /*转换*/
	do {
		str[i++] = index[unum % (unsigned)radix];
		unum /= radix;
	} while (unum);
	str[i] = '\0';
	/*逆序*/
	if (str[0] == '-')
		k = 1;/*十进制负数*/
	else
		k = 0;
	char temp;
	for (j = k; j <= (i - 1) / 2; j++) {
		temp = str[j];
		str[j] = str[i - 1 + k - j];
		str[i - 1 + k - j] = temp;
	}
	return str;
}


/*把c中的符号作为分割符，对s进行分割，分割后的子串放入vector v中*/
void SplitString(const string s, vector<string> &v, const string c){
	int pos1,pos2;
	pos2 = s.find(c);
    
	pos1 = 0;
	while(string::npos != pos2){
		v.push_back(s.substr(pos1, pos2-pos1));
		pos1 = pos2 + c.size();
		pos2 = s.find(c,pos1);
	}
	if(pos1 != s.length())
		v.push_back(s.substr(pos1));
}

/* 更新OthersID */
map<int,int> GetOtherID(string &msg){
    map<int,int> mid;
    vector<string> vet;
    
    vector<string>::iterator it;
    SplitString(msg,vet,",");
    for(it = vet.begin(); it != vet.end(); it++){
        int key,value;
        int pos = it->find("-");
        key = atoi(it->substr(0,pos).c_str());
        value = atoi(it->substr(pos+1,it->length()-pos-1).c_str());
        if(value==getpid())continue;
        mid[key] = value;
    }
    return mid;
}

/*消息构造函数*/
string gen_msg(int type,int src,int dest){
    string res="";
    res+=('0'+type);
    res+=":";
    char tmp1[100];char tmp2[100];
    itoa(src,tmp1,10);
    string res1=string(tmp1);
    itoa(dest,tmp2,10);
    string res2=string(tmp2);
    res+=res1;
    res+=":";
    res+=res2;
    return res;
}

vector<string> my_split(const string &s, const string &seperator) {
	vector<string> result;
	typedef string::size_type string_size;
	string_size i = 0;

	while (i != s.size()) {
		//找到字符串中首个不等于分割符的字母
		int flag = 0;
		while (i != s.size() && flag == 0) {
			flag = 1;
			for (string_size x = 0; x < seperator.size(); ++x)
				if (s[i] == seperator[x]) {
					++i;
					flag = 0;
					break;
				}
		}

		//找到又一个分割符，将两个分割符之间的字符串取出
		flag = 0;
		string_size j = i;
		while (j != s.size() && flag == 0) {
			for (string_size x = 0; x < seperator.size(); ++x)
				if (s[j] == seperator[x]) {
					flag = 1;
					break;
				}
			if (flag == 0)
				++j;
		}
		if (i != j) {
			result.push_back(s.substr(i, j - i));
			i = j;
		}
	}
	return result;
}

/*解析消息,存到msg_info里面*/
mesg_s parse_msg(string raw){
    vector<string> tmp;
    mesg_s msg_info;
    if(raw.length()!=0){
        tmp=my_split(raw,":");
        for(int i=0;i<tmp.size();i++)
            msg_info.push_back(atoi(tmp[i].c_str()));
    }
    return msg_info;
}


/*遍历queue,找target类型的消息*/
/*找到返回该消息，否则返回NULL*/
//todo 线程安全
// string search_queue(struct msg_queue *q,int target){
//     int i;
//     string getstr="";
//     mesg_s now;
//     char nullstr[2]="";


//     pthread_mutex_lock(&mux);
//     for(i=q->lget;i<q->nData;i++){
//         now=parse_msg(string(q->buffer[i]));
//         if(now.size()==0)continue;
//         if(now[0]==target){
//             getstr=string(q->buffer[i]);
//             //cout<<"search success :"<<getstr<<endl;
//             bzero(q->buffer[i],sizeof(q->buffer[i]));/*把消息队列里面对应位置的数据清除*/
//             break;
//         }  
//         now.clear();  
//     }
//     pthread_mutex_unlock(&mux);


//     return getstr;
// }




/* 处理消息*/

/* 处理选举消息*/
bool send_im_alive(int destid)
{
    /*发送存活消息*/
    //cout<<local->hostID<<": sent im alive to "<<destid<<endl;
    Log(local->hostID,destid,"sent im alive to ");
    string msg=gen_msg(1,local->hostID,destid);
    send(destid,msg.c_str());
    return true;
}

/*找出比hostid大的所有的id，返回到vector*/
vector<int> find_higher_prio()
{
    vector<int> tmp;
    map<int,pid_t>::iterator it;
    for(it=local->OthersID.begin();it!=local->OthersID.end();it++){
        if(it->first>local->hostID){
            tmp.push_back(it->first);    
        }       
    }      
    return tmp;
}

/*给比自己大的id发送选举消息*/
int send_elect_msg()
{
    //cout<<local->hostID<<":"<<"sending elect message\n";   
    Log(local->hostID,-2,"sending elect message");

    string elect;
    vector<int> targets=find_higher_prio() ;
    vector<int>::iterator it;
    for(it=targets.begin();it!=targets.end();it++){
        elect=gen_msg(0,local->hostID,*it);/*构造选举消息*/
        send(*it,elect.c_str());
    }   
    return 0;
}

/* 设置当前的leader_id */
int set_leader(int id)
{
    election_cnt++;
    time(&leader_update_time);  /*在这个时间点更新了新的leader p1*/
    time(&conn_time_check); /*刚刚完成选举，更新时间*/
    pthread_mutex_lock(&mux_sys_state);
    sys_state=0;/*结束选举，回到normal状态*///todo
    pthread_mutex_unlock(&mux_sys_state);
    local->leaderID=id;
    if(id==local->hostID)
        local->isLeader=1; 
    else
        local->isLeader=0;
    cout<<local->hostID<<":"<<"now leader is:"<<id<<endl;
    Log(local->hostID,id,"now leader is:");
    return id;
}

/*给所有id发送胜利消息,并设置当前的leader_id为自己*/
int send_victory_msg()
{
    set_leader(local->hostID); 
    
    //cout<<local->hostID<<":"<<"start to send victory message\n";
    Log(local->hostID,-2,"start to send victory message");

    string victory;
    map<int,pid_t>::iterator it;
    for(it=local->OthersID.begin();it!=local->OthersID.end();it++){
        victory=gen_msg(2,local->hostID,it->first);/*构造胜利消息*/
        //cout<<local->hostID<<"  other id:"<<it->first<<endl;
        send(it->first,victory.c_str());
    }   
    return 0;
}


/* 以下是流程性函数：*/


void wait_alive()
{
    pthread_mutex_lock(&mux_sys_state);
    sys_state=3;
    pthread_mutex_unlock(&mux_sys_state);
    /* 一定时间内收到存活消息，设置timeout为0*/
    //cout<<local->hostID<<":"<<"waiting for alive message\n";
    Log(local->hostID,-2,"waiting for alive message");
    //检查是否已经结束选举(如收到了新的胜利消息etc会导致选举结束)
    if(sys_state==0)
        return;

    fd_set rdfs;
    struct timeval timeout;
    int fdn;
    char buf[1024];
    int n;

    FD_ZERO(&rdfs);
    FD_SET(pipefd[0], &rdfs);
    timeout.tv_sec = ALIVE_TIMEOUT;
    timeout.tv_usec = 0;
    

again2:
    if((fdn = select(pipefd[1] + 1, &rdfs, NULL, NULL, &timeout)) < 0)
    {
        if(errno==EINTR){
            goto again2;
        }
        else{
            printf("select error: %s\n", strerror(errno));
            return;
        }
        
    }
    else if(fdn == 0)  /*超时，认为比自己大的id均未存活，发胜利*/
    {
        /*检查系统状态*/
        if(sys_state==3){
            //cout<<local->hostID<<":"<<"wait till timeout,sending victory...\n";
            Log(local->hostID,-2,"wait till timeout,sending victory...");
            send_victory_msg();
        }
    }
    else if(fdn >0){  /*select返回一个fd，说明收到了存活消息，开始等待胜利消息*/
        if(FD_ISSET(pipefd[0], &rdfs)){
            if((n = read(pipefd[0], buf, sizeof(buf) - 1)) > 0){             
                wait_victory();
            }
            else{
                cout<<"read error\n";
            }
        }
    }
}


bool start_election()
{
    
    pthread_mutex_lock(&mux_sys_state);
    sys_state=1;
    pthread_mutex_unlock(&mux_sys_state);
    local->isLeader=0;
    local->leaderID=-1;
    //cout<<local->hostID<<":"<<"start election\n";
    Log(local->hostID,-2,"start election");
    vector<int> higher_id=find_higher_prio();
    if(higher_id.size()==0){
        //cout<<local->hostID<<":"<<"no higher id\n";
        Log(local->hostID,-2,"no higher id");
        send_victory_msg();
    }  
    else{
        send_elect_msg();
        wait_alive();
    }
    pthread_mutex_lock(&mux_sys_state);
    sys_state=0;/*选举结束，恢复normal状态*/
    pthread_mutex_unlock(&mux_sys_state);
    return true;
}


void wait_victory()
{
    pthread_mutex_lock(&mux_sys_state);
    sys_state=4;
    pthread_mutex_unlock(&mux_sys_state);
    
    //cout<<local->hostID<<":"<<"wait for victory message...\n";
    Log(local->hostID,-2,"wait for victory message...");

    fd_set rdfs;
    struct timeval timeout;
    int fdn;
    char buf[1024];
    int n;

    FD_ZERO(&rdfs);
    FD_SET(pipefd[0], &rdfs);
    timeout.tv_sec = VICTORY_TIMEOUT;
    timeout.tv_usec = 0;

again:
    if((fdn = select(pipefd[1] + 1, &rdfs, NULL, NULL, &timeout)) < 0){
        if (errno == EINTR){
            goto again;
        }
            
        else{
            cout<<"select error: "<<strerror(errno)<<endl;
            return;
        }
    }
    else if(fdn == 0)
    {
        /*超时，重新开始选举流程*/
        //cout<<"wait victory message till timeout,restart election\n";
        if(sys_state==4)
            start_election();
    }
    else if(fdn >0) /* 在sys_state=4的状态下收到了胜利消息，设置leader */ 
    {
        if(FD_ISSET(pipefd[0], &rdfs)){
            if((n = read(pipefd[0], buf, sizeof(buf) - 1)) > 0){
                string msg=string(buf);
                mesg_s msg_info=parse_msg(msg);
                set_leader(msg_info[1]);
            }
            else{
                //cout<<"read error\n";
            }
                
        }
    }
}

/*检查消息队列有无heartbeat消息*/
void no_leader_action(){
    int id;
    int flag=0;/*flag==1表示:队列中存在来自leader的正确格式的心跳消息*/

    /*
    mesg_s msg;
    string hb=search_queue(&msgqueue,3);
    if(hb.length()!=0){
        msg=parse_msg(hb);
        if(msg.size()==3){
            id=msg[1];
            if(id==local->leaderID)
                flag=1;
        }    
    }
    time_t nowtime;
    time(&nowtime);

    if(flag==1) 
        conn_time_check=nowtime;
    else{

    */

    time_t nowtime;
    time(&nowtime);
    /*检查是否超时*/
    /*第一次选举时不考虑宕机*/
    if(election_cnt>=1&&nowtime-conn_time_check>HEART_BEAT){
        /* 超时，认为该leader宕机 *//* 发起选举 */
        if(sys_state==0)/*如果已经处于选举状态了就不再执行以下*/
        {
            cout<<local->hostID<<":"<< "I have detected leader is down\n";
            Log(local->hostID,-2,"I have detected leader is down");
            start_election();
        }
            
    }              
}

/*周期性的发送心跳消息给所有人*/
void leader_action(){
    string beat;
    map<int,pid_t>::iterator it;
    for(it=local->OthersID.begin();it!=local->OthersID.end();it++){
        //cout << local->hostID <<":"<<"heatbeat" << endl;
        Log(local->hostID,-2,"heartbeat");
        beat=gen_msg(3,local->hostID,it->first);/*构造心跳消息*/
        send(it->first,beat.c_str());
    }   
}

/*定时器*/
/*每n秒执行一次的函数*/
void prompt_info(int signo){
    /*如果自己不是leader:执行no_leader_action()*/
    /*如果自己是leader:执行leader_action*/
    if(local->isLeader==1)
        leader_action();
    else
        no_leader_action();
   
}

void uninit_time()  
{  
    struct itimerval value;  
    value.it_value.tv_sec = 0;  
    value.it_value.tv_usec = 0;  
    value.it_interval = value.it_value;  
    setitimer(ITIMER_REAL, &value, NULL);  
}

/*初始化定时器*/
void init_time(){
    struct itimerval value;
    value.it_value.tv_sec=2;    /*初始时间计数为2秒0微秒*/
    value.it_value.tv_usec=0;
    value.it_interval= value.it_value;/*执行任务的时间间隔也为5秒0微秒*/
    setitimer(ITIMER_REAL,&value, NULL);/*设置计时器ITIMER_REAL*/
}

void gotosleep(int signo){
    cout<<"im sleeping\n";
    uninit_time();
    pau=1;
    sys_state=-1;
    pause();
    pau=0;
    sys_state=0;
    init_time();
    local->isLeader=0;
    local->leaderID=-1;
    time(&conn_time_check);
    start_election();
}
// 建立信号处理机制
void init_sigaction(void)
{
    struct sigaction tact;
    
    struct sigaction tact2;
    /*信号到了要执行的任务处理函数*/
    tact.sa_handler= prompt_info;
    tact.sa_flags=0;
    /*初始化信号集*/
    sigemptyset(&tact.sa_mask);//todo

    tact2.sa_handler= gotosleep;
    tact2.sa_flags=0;
    /*初始化信号集*/
    sigemptyset(&tact2.sa_mask);//todo
    
    /*建立信号处理机制*/
    sigaction(SIGALRM,&tact, NULL);
    sigaction(SIGRTMAX,&tact2, NULL);
}


/*处理新加入的结点*/
void add_peer(int id,int pid){
    local->OthersID.insert(pair<int,pid_t>(id,pid));
}


void * election(void * arg)
{
    pthread_detach(pthread_self());
    if(sys_state==0)
        start_election();
}

void * produce(void * arg)
{
    pthread_detach(pthread_self());
    while(1){    
        if(pau==1)continue;
        int i=0;
        vector<string> tmp;
        vector<string>::iterator it;  
        recv(local->hostID,tmp);
        time_t nowtime;
        time(&nowtime);
        for(it=tmp.begin();it!=tmp.end();it++){
            put_queue(&msgqueue, *it,nowtime);/*把当前的时间传进去 p1*/
        }
    }
}

void *consume(void *arg)
{
    pthread_detach(pthread_self());
    string msg="";  
    time_t msgtime;
    while(1){
        if(pau==1)continue;
        msg = get_queue(&msgqueue,&msgtime); /*每次取出队列的第一个进行处理*/
        if(msg.length()==0)continue;
        mesg_s msg_info=parse_msg(msg);
        if(msg_info.size()<3)
            continue;
        int type=msg_info[0];
        if(type==4){
            /*加入消息*/
            if(msg_info.size()<4)
                continue;
            add_peer(msg_info[1],msg_info[3]);
        }
        else if(type==2){
            /*胜利消息*/
            //cout<<local->hostID<<":"<<"received victory message :"<<msg<<endl;
            Log(local->hostID,-2,"received victory message");
            /* 
            若收到更晚到达的胜利消息,
            更新leader_update_time并更改leader
            */
            if(msgtime>leader_update_time){ 
                Log(local->hostID,-2,"victory message updated");
                if(sys_state==4){
                    int n = msg.length() + 1;
                    /*用管道通知另一个线程中的select*/
                    if(write(pipefd[1], msg.c_str(), n) != n){
                        cout<<"write error:"<<strerror(errno)<<endl;
                        continue;
                    }
                }
                else
                    set_leader(msg_info[1]);
            }
        }
        else if(type==0){
            /*选举消息*/
            send_im_alive(msg_info[1]);
            if(sys_state==0){
                pthread_t pid;
                //cout<<local->hostID<<" : received elsect msg,start election\n";
                Log(local->hostID,-2,"received elsect msg,start election");
                pthread_create(&pid, 0, election, 0);
            }
        }
        else if(type==1){
            /*存活消息
            只在系统处于“等待存活消息”的状态期间处理该消息，否则直接丢弃
            */
            //cout<<local->hostID<<":"<<"received alive message:"<<msg<<endl;
            //cout<<local->hostID<<":sys state="<<sys_state<<endl;
            Log(local->hostID,-2,"received alive message");
            if(sys_state==3){
                int n = msg.length() + 1;
                if(write(pipefd[1], msg.c_str(), n) != n)
                {
                    cout<<"write error: "<<strerror(errno)<<endl;
                    continue;
                }
            }
        }   
        else if(type==3){
            /*心跳消息*/
            if(sys_state==0){
                /*在正常状态下收到了更新的心跳消息才更新时间*/  
                if(msgtime>conn_time_check)
                    conn_time_check=msgtime;
            }        
        }    
    }
}

/*编译时的第一个参数为hostid*/
void self_init(char **argv){
    local=new Hosts;
    vector<string> msg;
    recv(getpid(), msg);
    string sigmsg;
    for(it_v = msg.begin(); it_v != msg.end(); it_v++){
        //cout << "recv::" << *it_v << endl;
        sigmsg = *it_v;
    } 
    local->OthersID = GetOtherID(sigmsg);
    local->hostID = atoi(argv[1]);
    local->pid = getpid();
    local->isLeader = 0;
    local->leaderID = -1;
    time(&leader_update_time);
    // map<int,int>::iterator it;
    // for(it=local->OthersID.begin();it!=local->OthersID.end();it++){
    //     cout<<local->hostID<<":" << it->first<<"-" << it->second << endl;
    // }
    cout << "host：" << local->hostID <<"(pid:"<<local->pid<< ")Initialization is complete" << endl;
    Log(local->hostID,-2,"Initialization is complete");
}


int main(int argc,char **argv)
{   
    sleep(3);//change
    self_init(argv); 
    sleep(2);


    /*两个线程，一负责接收消息，一负责处理消息*/
    pthread_t pid1,pid2,pid3;

    msgqueue.size=1000;
    msgqueue.lget=msgqueue.lput=msgqueue.nData=msgqueue.nEmptyThread=0;
    msgqueue.arrivetime=(time_t *)malloc(msgqueue.size * sizeof(time_t));
    pthread_mutex_init(&mux, 0);
    pthread_mutex_init(&mux_sys_state, 0);
    pthread_cond_init(&cond_get, 0);
    pthread_cond_init(&cond_put, 0);

    /*开启定时器*/
    init_sigaction();
    init_time();
    time(&conn_time_check);

    if(pipe(pipefd) < 0)
    {
        printf("pipe error: %s\n", strerror(errno));
        return -1;
    }

    pthread_create(&pid1, 0, produce, 0);
    pthread_create(&pid2, 0, consume, 0);

    start_election();

    while(1){}
    //sleep(60);
    //线程退出的处理
    /*
    
    close(pipefd[1]);
    pthread_mutex_destroy(&mux);
    pthread_cond_destroy(&cond_get);
    pthread_cond_destroy(&cond_put);
    free(local);

    */
    return 0;
}