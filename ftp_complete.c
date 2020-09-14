#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <getch.h>
#include <arpa/inet.h>
#include <stdbool.h>

typedef struct sockaddr* SP;


bool snd_rcv(char* buf,int sock,int ret,const char* warn)
{
	if(strlen(buf))
	{
		send(sock,buf,strlen(buf),0);
	}
	recv(sock,buf,4096,0);
	printf("recv: %s\n",buf);
	int len;
	sscanf(buf,"%d",&len);
	if(len != ret)
	{
		printf("%s\n",warn);
		return false;
	}
	return true;
}
int order_pasv(char* buf,int sock)
{
	sprintf(buf,"PASV\n");
	if(!snd_rcv(buf,sock,227,"下载通道连接失败"))
	{
		return -1;
	}
	unsigned short int ip1,ip2,ip3,ip4,port1,port2;
	sscanf(buf,"227 Entering Passive Mode (%hu,%hu,%hu,%hu,%hu,%hu)",&ip1,&ip2,&ip3,&ip4,&port1,&port2);
	char ip[16] = {};
	sprintf(ip,"%hu.%hu.%hu.%hu",ip1,ip2,ip3,ip4);
	short port = port1*256+port2;
	printf("ip=%s port=%hd\n",ip,port);

	int cli_pasv = socket(AF_INET,SOCK_STREAM,0);
	if(0 > cli_pasv)
	{
		perror("socket");
		return -1;
	}
	struct sockaddr_in cli_addr;
	cli_addr.sin_family = AF_INET;
	cli_addr.sin_port = htons(port);
	cli_addr.sin_addr.s_addr = inet_addr(ip);
	size_t addrlen = sizeof(cli_addr);
	if(connect(cli_pasv,(SP)&cli_addr,addrlen))
	{
		perror("connect");
		return -1;
	}
	return cli_pasv;
}

bool order_ls(char* buf,int sock,int pasv)
{
	pasv = order_pasv(buf,sock);
	sprintf(buf,"LIST -al\n");
	send(sock,buf,strlen(buf),0);
	if(!snd_rcv(buf,sock,150,"上传失败"))
	{
		return false;
	}
	int len;
	while(len = recv(pasv,buf,4096,0))
	{
		printf("%s",buf);
	}
	printf("\n");
	close(pasv);
	bzero(buf,4096);
	bzero(buf,4096);
	if(!snd_rcv(buf,sock,226,"上传失败"))
	{
		return false;
	}
	return true;
}

bool order_mkdir(char* buf,const char* file_name,int sock)
{
	sprintf(buf,"MKD %s\n",file_name);
	if(!snd_rcv(buf,sock,257,"目录创建失败"))
	{
		return false;
	}
}

bool order_cd(char* buf,const char* file_name,int sock)
{
	sprintf(buf,"CWD %s\n",file_name);
	if(!snd_rcv(buf,sock,250,"目录进入失败"))
	{
		return false;
	}
}

bool order_pwd(char* buf,int sock)
{
	sprintf(buf,"PWD\n");
	if(!snd_rcv(buf,sock,257,"找不到目录路径"))
	{
		return false;
	}
	int len;
	char road[20] = {};
	sscanf(buf,"%d %s",&len,road);
	printf("路径：%s\n",road);
}

bool order_put(char* buf,const char* file_name,int sock,int pasv)
{
	pasv = order_pasv(buf,sock);
	sprintf(buf,"STOR %s\n",file_name);
	send(sock,buf,strlen(buf),0);

	if(!snd_rcv(buf,sock,150,"上传失败"))
	{
		return false;
	}
	int fd = open(file_name,O_RDONLY);
	if(0 > fd)
	{
		perror("open");
		return false;
	}
	
	int len;
	while(len = read(fd,buf,4096))
	{
		send(pasv,buf,len,0);
	}
	close(pasv);
	close(fd);

	bzero(buf,4096);
	if(!snd_rcv(buf,sock,226,"上传失败"))
	{
		return false;
	}
}

bool order_get(char* buf,const char* file_name,int sock,int pasv)
{
	pasv = order_pasv(buf,sock);
	sprintf(buf,"SIZE %s\n",file_name);
	if(!snd_rcv(buf,sock,213,"文件大小读取失败"))
	{
		return false;
	}

	sprintf(buf,"MDTM %s\n",file_name);
	if(!snd_rcv(buf,sock,213,"文件修改时间读取失败"))
	{
		return false;
	}

	sprintf(buf,"RETR %s\n",file_name);
	send(sock,buf,strlen(buf),0);

	bzero(buf,4096);
	if(!snd_rcv(buf,sock,150,"下载失败"))
	{
		return false;
	}

	int fd = open(file_name,O_WRONLY|O_CREAT|O_TRUNC,0644);
	if(0 > fd)
	{
		perror("open");
		return false;
	}
	
	int len;
	while(len = recv(pasv,buf,4096,0))
	{
		write(fd,buf,len);
	}
	close(pasv);
	close(fd);


	bzero(buf,4096);
	if(!snd_rcv(buf,sock,226,"下载失败"))
	{
		return false;
	}
}

int main(int argc,const char* argv[])
{
	int cli_sock = socket(AF_INET,SOCK_STREAM,0);
	if(0 > cli_sock)
	{
		perror("socket");
		return EXIT_FAILURE;
	}
	struct sockaddr_in cli_addr;
	cli_addr.sin_family = AF_INET;
	cli_addr.sin_port = htons(21);
	cli_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	socklen_t addrlen = sizeof(cli_addr);

	if(connect(cli_sock,(SP)&cli_addr,addrlen))
	{
		perror("connect");
		return EXIT_FAILURE;
	}

	char buf[4096] = {};
	if(!snd_rcv(buf,cli_sock,220,"连接服务器失败"))
	{
		return EXIT_FAILURE;
	}
	printf("连接服务器成功\n");

	char name[20] = {};
	printf("请输入用户名:");
	scanf("%s",name);
	sprintf(buf,"USER %s\n",name);
	if(!snd_rcv(buf,cli_sock,331,"用户名发送错误！"))
	{
		return EXIT_FAILURE;
	}
	char* pass;
	pass = malloc(20);
	printf("请输入密码：");
	pass = getpass("");
	sprintf(buf,"PASS %s\n",pass);
	if(!snd_rcv(buf,cli_sock,230,"密码发送错误"))
	{
		return EXIT_FAILURE;
	}
	printf("登录成功!\n");
	free(pass);

	
	int cli_pasv;
	for(;;)
	{
		printf("FTP>");
		char order[20] = {};
		char file_name[20] = {};
		scanf("%s",order);
		if(0 == strcmp(order,"ls"))
		{	
			order_ls(buf,cli_sock,cli_pasv);
			continue;
		}
		else if(0 == strcmp(order,"mkdir"))
		{
			scanf("%s",file_name);
			order_mkdir(buf,file_name,cli_sock);
			continue;
		}
		else if(0 == strcmp(order,"cd"))
		{
			scanf("%s",file_name);
			order_cd(buf,file_name,cli_sock);
			continue;
		}
		else if(0 == strcmp(order,"pwd"))
		{
			order_pwd(buf,cli_sock);
			continue;
		}
		else if(0 == strcmp(order,"put"))
		{
			scanf("%s",file_name);
			order_put(buf,file_name,cli_sock,cli_pasv);
			continue;
		}
		else if(0 == strcmp(order,"get"))
		{
			scanf("%s",file_name);
			order_get(buf,file_name,cli_sock,cli_pasv);
			continue;
		}
		else if(0 == strcmp(order,"bye"))
		{
			printf("退出成功\n");
			break;
		}
		else
		{
			printf("命令有误\n");
			continue;
		}
	}
	close(cli_pasv);
}
