#include<unistd.h>
#include<signal.h>
#include <fcntl.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/param.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<time.h>
#include <string.h>
#include <stdlib.h>
#define FIFO "/tmp/myfifo"


void init_daemon() {
    int pid;
    int i;
    pid=fork();
    if(pid<0)    
        exit(1);
    else if(pid>0)
        exit(0);

    setsid();
    pid=fork();
    if(pid>0)
        exit(0);
    else if(pid<0)    
            exit(1);

    for(i=0;i<NOFILE;i++)
        close(i);
    chdir("/root/test");
    umask(0);
    return;
}


int main()
{
    // create daemon
    FILE *fp;
    time_t t;
    init_daemon();


    //fifo
    char buf_r[100];
    char test[100];
    strcpy(test,"OK");
    int  fd;
    int  nread;
    int i = 0;

    if((mkfifo(FIFO,O_CREAT|O_EXCL)<0))//&&(errno!=EEXIST))
    {
        printf("cannot create fifoserver\n");
    }
    printf("Preparing for reading bytes...\n");

    memset(buf_r,0,sizeof(buf_r));

    fd=open(FIFO,O_RDONLY|O_NONBLOCK,0);
    if(fd==-1)
    {
        perror("open");
        exit(1);
    }

    while(1) {
        fp=fopen("shouhu_text.log","a");
        time(&t);
        
        memset(buf_r,0,sizeof(buf_r));

        if((nread=read(fd,buf_r,100))==-1) {
                fprintf(fp,"there is no data:%s\n",asctime(localtime(&t)));
        }
        fprintf(fp,"the data is:%s\n\n",buf_r);
        if(strcmp(buf_r,test) == 0) {
            i = 0;
            fprintf(fp,"SUCCESS!!!!!!!!!!     %s\n\n",buf_r);
        } else {
            i++;
            fprintf(fp,"ERROR!!!!!!!!!!!!     %s\n\n",buf_r);
        }
        if(i == 3){
            fprintf(fp,"!!    DELAY    !!%s\n\n",buf_r);
            system("cd Daemon");
            system("./write");
        }
        fclose(fp);
        sleep(3);
    }
    pause();
    unlink(FIFO);
    return 0;
}
