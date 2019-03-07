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


#define FIFO_SERVER "/tmp/myfifo"


void init_daemon()
{
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




int main(int argc,char **argv)
{
    
    int fd;
    int i;
    char test[100];
    strcpy(test,"OK");
    int nwrite;


    FILE *fp;
    time_t t;
    init_daemon();


    fd=open(FIFO_SERVER,O_WRONLY|O_NONBLOCK,0); 
    
    


    printf("pid = %d\n",getppid());
    while(1)
//    for(i = 0;i < 6;i++)
    {
        fp=fopen("write_text.log","a");
        time(&t);


        fd=open(FIFO_SERVER,O_WRONLY|O_NONBLOCK,0); 
        if((nwrite = write(fd,test,100)) == -1)
        {
            //printf("The FIFO has not been read yet.Please try later\n");
            fprintf(fp,"The FIFO has not been read yet.Please try later    :%s\n",asctime(localtime(&t)));
        }
        else
        {
            //printf("write %s to the FIFO\n",test);
            fprintf(fp,"write %s to the FIFO                               :%s\n",test,asctime(localtime(&t)));
        }
//        close(fd);
        fclose(fp);
        sleep(3);
    }    
}
