#include "bully_al.h"
#include "sr.h"
struct sigaction tact;

/* 创建一个host */
int Create_host(map
<int,int> allID){
    srand((int)getpid());
    /*生成不重复的hostID*/
    int flag = 1;
    int id = random(65535);
    map<int,int>::iterator it;
    while(flag){
        for(it = allID.begin(); it != allID.end(); it++){
            if(id == it->first){
                id = random(65535);
                flag = 2;
                break;
            }
        }    
        if(flag != 2)
            flag = 0;
    }
    return id;
}

void Update(){
    /* 串行化AllID */
    string msg;
    /* 更新host的OthersID */
    map<int,int>::iterator it;
    for(it = AllID.begin(); it != AllID.end(); it++){
        if(it != AllID.begin())
            msg += ",";
        msg = msg + to_string(it->first) + "-" + to_string(it->second); 
    }
    //输入type和你要发的信息作为参数,type不要设为0
    for(it = AllID.begin(); it != AllID.end(); it++){
        pid_t pid = it->second;
        // cout << it->second << endl;
        send(it->second, (char*)msg.c_str());
    }
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

void killachild(int signo){
    //cout<<"hahhhhhhhhhhhhhhhhhhh"<<endl;
    
}
// 建立信号处理机制
void init_sigaction(void)
{
    /*信号到了要执行的任务处理函数*/
    tact.sa_handler=killachild;
    tact.sa_flags=0;
    /*初始化信号集*/
    sigemptyset(&tact.sa_mask);//todo
    sigprocmask(SIG_SETMASK,&tact.sa_mask,NULL);
    /*建立信号处理机制*/
    sigaction(SIGUSR1,&tact, NULL);
}


int main(){
    Log(-1,-2,"--------------------------[NEW BULLY]---------------------------");
    cout<<"i'm main,my pid is :"<<getpid()<<endl;
    init_sigaction();

    int status = 0,fd[2];//tokillfd[2]; 
    for(int cnt = 0; cnt < 5; cnt++){
        if(pipe(fd) < 0){
            perror("pipe");
            exit(1);
        }
        // if(pipe(tokillfd) < 0){
        //     perror("pipe");
        //     exit(1);
        // }
        pid_t pid = fork();
        if(pid < 0){
            perror("error\n");
        } 
        /* child */
        else if(pid == 0){ 
            char hostid[8];
            
            Hosts host;
            int ID = Create_host(AllID); 
            close(fd[0]);
            char snd[8];
            sprintf(snd,"%d\n", ID);
            write(fd[1], snd, sizeof(snd));

            sprintf(hostid,"%d",ID);
            /*OtherID*/
            //host.OthersID = GetOtherID(sigmsg);
            //char tm[6];
            //sprintf(tm,"%d",15-3*cnt);
            execl("./hoste","hoste",hostid,NULL);
        }
        /* parent */
        else{
            //close(tokillfd[0]);//change
            close(fd[1]);
            char rcv[8];
            read(fd[0], rcv, 8);
            int id = atoi(rcv);
            
            int flag = 0;
            map<int,int>::iterator it;
            for(it = AllID.begin(); it != AllID.end(); it++){
                if(it->first == id){/* 如果hostID重复 */
                    flag = 1;
                    cnt--;
                }
            }
            if(flag == 0)
                AllID.insert(pair<int, int>(id, pid));
        }
    } 
    // map<int,int>::iterator it;
    // for(it = AllID.begin(); it != AllID.end(); it++){
    //     cout << it->first << ":" << it->second << endl;
    // }
    /* 更新所有子进程中的map */
    Update();
    // wait(&status);
    while(1){
        string oper;
        int tokill;
        cin>>oper>>tokill;
        cout<<"input:"<<oper<<" "<<tokill<<endl;
        if(oper=="kill"){
            cout<<"send sigrtmax to "<<tokill<<endl;
            kill(tokill,SIGRTMAX);
        }
            
        //write(tokillfd[1], "sleep", 6);
        //if(AllID.find(pair<int,pid_t>(tokill1,tokill2))!=NULL)
        if(oper=="recover"){
            cout<<"send sigalrm to "<<tokill<<endl;
            kill(tokill,SIGALRM);
        }    
    }
    pid_t wpid;
    while((wpid = wait(&status))>0){ 
    }
    return 0;
}