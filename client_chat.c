#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUFFER_SIZE 1000
#define CHAT_BUF_SZ 1000
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
FILE* read_fp = NULL;
void error_handling(char *message);
void* write_thread(void *argv){
	int sock = *(int*)argv;
	int len;
	int flen;
	char* fbuffer;
	char buffer[CHAT_BUF_SZ];

	while(1){
		read(sock, &len, sizeof(len));
		if(len == -1){//¿¿¿¿¿¿¿¿¿ ¿¿¿
			printf("receive file\n");
			read(sock, &len, sizeof(len));
			fbuffer = (char*)malloc(CHAT_BUF_SZ);
			do{
				read(sock, &flen, sizeof(flen));
				read(sock, fbuffer, flen);
				fwrite(fbuffer, 1, flen, read_fp);
			}while(flen == CHAT_BUF_SZ);
			free(fbuffer);
			fclose(read_fp);
			read_fp = NULL;
			continue;
		}
		else if(len == -2){
			printf("receive file\n");
 			fbuffer = (char*)malloc(CHAT_BUF_SZ);
                        do{
                                read(sock, &flen, sizeof(flen));
                                read(sock, fbuffer, flen);
                                fwrite(buffer, 1, flen, read_fp);
                        }while(flen == CHAT_BUF_SZ);
                        free(fbuffer);
                        fclose(read_fp);
                        read_fp = NULL;
			continue;
		}
		read(sock, buffer, len);
		buffer[len] = '\0';
		printf("%s : ", buffer);
		read(sock, &len, sizeof(len));
		read(sock, buffer, len);
		buffer[len-1] = '\0';
		printf("%s\n", buffer);
	}
}
int main(int argc, char* argv[])
{	
	FILE* fp;
	size_t fsize;
	int flen;
	int sock;
	char len;
	struct sockaddr_in serv_addr;

	char buffer[BUFFER_SIZE];

	int str_len;
	int i;

	pthread_t tid;

	if(argc!=3){
		printf("Usage : %s <IP> <port>\n", argv[0]);
		exit(1);
	}

	sock=socket(PF_INET, SOCK_STREAM, 0);
	if(sock == -1)
		error_handling("socket() error");

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_addr.s_addr=inet_addr(argv[1]);
	serv_addr.sin_port=htons(atoi(argv[2]));

	if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1) 
		error_handling("connect() error!");

	int user_id;
	char pw[10];
	char* user_name;
	char login_success;

	//½ÃÀÛ ·Î±×ÀÎ
	printf("---------------KPU chating program---------------\n");
	for(i =0; i<5; i++){
		printf("input your information to use program\n");
		printf("id : ");
		scanf("%d", &user_id);//À¯Àú ¾ÆÀÌµð ÀÔ·Â
		write(sock, (void*)&user_id, sizeof(user_id));
		read(sock, &login_success, sizeof(login_success));
		if(login_success == 0){//fail
			continue;
		}
		printf("password : ");
		scanf("%s", pw);//À¯Àú ÆÐ½º¿öµå ÀÔ·Â
		write(sock, (void*)pw, strlen(pw));
		read(sock, &login_success, sizeof(login_success));
		if(login_success == 1){//success
			break;
		}
	}
	if(i==5){
		printf("login_fail\n");
		close(sock);
		return 0;
	}
	//send_group_id_to_user syn
	int count_group;
	int* group;
	read(sock, &count_group, sizeof(count_group));
	group = (int*)malloc(sizeof(int)*count_group);
	for(i = 0; i < count_group; i++){
		read(sock, group+i, sizeof(int));
	}
	//communication
	int group_id;
	while(1){
		printf("-------------choice group--------------\n");
		printf("if you want to create new group, input 0\n");
		printf("if you want to exit program, input -1\n");
		printf("group :");
		for(i =0; i< count_group; i++){
			printf("%3d", group[i]);
			if(i+1 != count_group)
				printf(",");
			else
				printf("\n");
		}
		//receive_group_id
		char is_collect;
		scanf("%d",&group_id);
		for(i = 0; i< count_group; i++){
			if(group_id == group[i]){
				is_collect = 1;
				break;
			}
			else
				is_collect = 0;
		}
		//not exit group id
		if(is_collect == 0 && group_id > 0){
			group_id = -2;
			printf("rechoice group id\n");
			continue;
		}

		write(sock, (void*)&group_id, sizeof(group_id));
		if(group_id == -1){
			break;
		}
		else if(group_id == 0){
			read(sock, &group_id, sizeof(group_id));
		}
		//create thread
		if(pthread_create(&tid, NULL, write_thread, (void*)&sock) != 0){
			printf("pthread create error\n");
			break;
		}
		//check server create read thread?
		//2st while
		char data_type ;
		char is_receieve = 0;
		int text_sz;
		int other_user_id;
		char filename[100];
		printf("<system> max chat size : 100byte\n");
		printf("<system> if you input '$', you can choice other option\n");
		printf("<chat mode>\n");
		while(1)
		{	
			data_type = 0;
			fgets(buffer, CHAT_BUF_SZ, stdin);
			if(strcmp("\n", buffer) == 0){
				continue;			
			}
			if(*(buffer) == '$'){
				printf("<option>\n");
				printf("Back : b, invite : i, File : f, File down : r\n");
				fgets(buffer, CHAT_BUF_SZ, stdin);
				data_type = *buffer;
			}
			write(sock, &data_type, sizeof(data_type));
			if(data_type == 0){
				text_sz = strlen(buffer);
				write(sock, &text_sz, sizeof(text_sz));
				write(sock, buffer, text_sz);
			}
			else if(data_type == 'b'){
				printf("option b select\n");
				pthread_cancel(tid);
				break;
			}
			else if(data_type == 'i'){
				printf("option i select\n");
				printf("please input other user id : ");
				scanf("%d", &other_user_id);
				write(sock, (void*)&other_user_id, sizeof(other_user_id));
				printf("<chat mode>\n");
				data_type = 0;
			}
			else if(data_type == 'f'){
				printf("option f select\n");
				printf("input send filename : ");
				fgets(filename, 100, stdin);
				filename[strlen(filename)-1] = '\0';
				printf("select filename : %s\n", filename);
				fp = fopen(filename, "rb");//¿¿ ¿¿ ¿¿¿ ¿¿ ¿¿¿¿¿¿¿.???????
				//
				text_sz = strlen(filename);
				write(sock, &text_sz, sizeof(text_sz));
				write(sock, filename, strlen(filename));	

				fseek(fp, 0, SEEK_END);
				fsize = ftell(fp);
				fseek(fp, 0, SEEK_SET);	
				while(0 < fsize){
					flen = fread(buffer, 1, CHAT_BUF_SZ, fp);
					fsize -= flen;
					write(sock, &flen, sizeof(flen));
					write(sock, buffer,flen);
				}
				fclose(fp);
				data_type = 0;
				write(sock, &data_type, sizeof(data_type));
				write(sock, &text_sz, sizeof(text_sz));
				write(sock, filename, text_sz);
				
				printf("send filename : %d, %s\n", text_sz, filename);
				printf("<chat mode>\n");

			}
			else if(data_type == 'r'){//¿¿¿ ¿¿ ¿¿
				printf("option f select\n");
				printf("input send filename : ");
				fgets(buffer, CHAT_BUF_SZ, stdin);
				buffer[strlen(buffer)-1] = '\0';
				printf("select filename : %s\n", buffer);

				read_fp = fopen(buffer, "wb");

				flen = strlen(buffer);
				write(sock, &flen, sizeof(flen));
				write(sock, buffer, strlen(buffer));
				printf("<chat mode>\n");	
			}
		}
	}
	close(sock);
	return 0;
}

void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}
