#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include "chat_struct.h"

#define USER_BUF_SIZE 30
#define TYPE_CHAT 0
#define TYPE_FILE 'f'
#define TYPE_INVITE 'i'
#define TYPE_BACK 'b'
#define TYPE_READ_FILE 'r'
#define TYPE_FIN -1
#define F_BUF_SZ 1000

//전역변수 data
int new_group_id;

//함수
void send_group_id_to_user(struct User_information* service_user, int sock);
int receive_group_id(int sock);
struct group_data_buffer* find_group_buffer_addr(int group_id);
//struct
struct user_write_thread_parameter {
	int sock;
	int group_id;
	int user_id;
	FILE** fp;
};
//
void* user_send_thread(void* argv) {
	struct user_write_thread_parameter input = *(struct user_write_thread_parameter*)argv;
	int counting_chat = 0;
	int sock = input.sock;
	int group_id = input.group_id;
	int len;
	int f_judge;
	size_t fsize;
	char buffer[1000];
	struct group_data_buffer* buf = find_group_buffer_addr(group_id);
	struct User_information* service_user = find_user_information(input.user_id);
	printf("user_send_thread run\n");
	while (1) {
		if (counting_chat < buf->size) {//체팅방의 변화가 있을시
			for (; counting_chat < buf->size; counting_chat++) {
				if (buf->buffer[counting_chat]->user_name != service_user->name) {//???
					len = strlen(buf->buffer[counting_chat]->user_name);
					if(write(sock, &len, sizeof(len)) == -1){
						error_handling("txt send error1");
					}
					if (write(sock, buf->buffer[counting_chat]->user_name, strlen(buf->buffer[counting_chat]->user_name)) == -1) {//체팅 작성자의 이름 전송
						error_handling("txt send error2");
						return NULL;
					}
					len = strlen(buf->buffer[counting_chat]->text);
					if(write(sock, &len, sizeof(len)) == -1){
						error_handling("txt send error3");
					}
					if (write(sock, buf->buffer[counting_chat]->text, strlen(buf->buffer[counting_chat]->text)) == -1) {//체팅 내용 전송
						error_handling("txt send error4");
						return NULL;
					}
				}
			}
			pthread_mutex_lock(&mutex);
			if (counting_chat == buf->size) {
				buf->buffer_check_user_count++;
			}
			pthread_mutex_unlock(&mutex);
		}
		counting_chat = counting_chat % (buf->size+1);

		//파일전송 추가???????????
		//옜 옜
		if(*(input.fp) != NULL){
			f_judge = -1;
			write(sock, &f_judge, sizeof(f_judge));
			f_judge = -2;
			write(sock, &f_judge, sizeof(f_judge));

			fseek(*(input.fp), 0, SEEK_END);
			fsize = ftell(*(input.fp));
			fseek(*(input.fp), 0, SEEK_SET);
			printf("fsize : %ld", fsize);
			while(0 < fsize){
				len = fread(buffer, 1, F_BUF_SZ, *(input.fp));
				fsize -= len;
				write(sock, &len, sizeof(len));
				write(sock, buffer,len);
			}
			fclose(*(input.fp));
			*(input.fp) = NULL;
		}
	}
	return NULL;
}
void* user_thread(void* argv) {

	FILE* fp = NULL;
	int flen;
	char* fbuffer;
	int sock = *(int*)argv;
	int receive_data_len;
	int user_id;
	int group_id;
	int i;
	char login_success;
	int invite_user_id;
	char chat_type;
	char is_send;
	int text_sz;
	pthread_t send_thread_id;

	chat_data* chat;
	struct User_information* service_user;
	struct User_information* invite_user;
	struct group_data_buffer* group_buffer_addr;
	struct user_write_thread_parameter write_prameter;//전송 스레드에 전달한 변수를 저장한 변수

	char* buffer = malloc(USER_BUF_SIZE * sizeof(char));//데이터를 임시적으로 받는 공간
	char* chat_text_buffer;
	printf("user thread run\n");
	for (i = 0; i < 5; i++) {
		//id
		receive_data_len = read(sock, &user_id, sizeof(user_id));//사용자의 아이디는 user_buf_size 를 초과해서는 안된다.
		if (receive_data_len <= 0) {
			error_handling("login_ID_error");
			close(sock);
			return NULL;
		}
		service_user = find_user_information(user_id);
		if (service_user == NULL) {//로그인 실패
			printf("user information search error\n");
			login_success = 0;
			write(sock, &login_success, sizeof(login_success));
			continue;
		}
		login_success = 1;
		write(sock, &login_success, sizeof(login_success));
		//password
		receive_data_len = read(sock, buffer, USER_BUF_SIZE);
		if (receive_data_len <= 0) {//로그인 실패
			write(sock, &login_success, sizeof(login_success));
			continue;
		}
		if (strcmp(service_user->PW, buffer) == 0) {//비밀번호가 맞는경우
			printf("connect user : %s\n", service_user->name);
			login_success = 1;
			write(sock, &login_success, sizeof(login_success));
			break;
		}
		else {//입력된 비밀번호가 틀린경우
			login_success = 0;
			write(sock, &login_success, sizeof(login_success));
			continue;
		}
	}
	if (i == 5) {//5회 실패시 로그인 실패를 알리는 메세지 송신       
		printf("로그인 실패\n");
		login_success = 2;
		free(buffer);
		close(sock);
		return NULL;
	}
	send_group_id_to_user(service_user, sock);
	//유저 체팅방 정보 읽고 송신
	while (1) {
		//체팅방 선택 수신
		group_id = receive_group_id(sock); //체팅방 id 수신    , -2 : 에러, -1 : 종료, 0 : 새로운 채팅방 생성, 0초과 : 체팅방 id
		if (group_id < -1) {//에러 발생시
			error_handling("receive group id error");
			continue;
		}
		else if (group_id == 0) {//새로운 방 생성시?????????????????????
			group_id = new_group_id;
			write(sock, &group_id, sizeof(group_id));
			new_group_id++;
		}
		else if (group_id == -1){
			break;
		}
		group_buffer_addr = find_group_buffer_addr(group_id);//채팅방 생성하거나 선택하는 경우, 새로운 채팅방 0 이 입력되면 새로운 체팅방을 만들고 데이터를 업데이트한다.
		//전송스레드 인자들 초기화
		write_prameter.sock = sock;
		write_prameter.group_id = group_id;
		write_prameter.user_id = user_id;
		write_prameter.fp = malloc(sizeof(FILE*));
		//송신 쓰레드 생성
		if (pthread_create(&send_thread_id, NULL, user_send_thread, (void*)&write_prameter) != 0) {
			error_handling("pthread_create_error");//실패시 에러 메시지 출력후 통신 종료
			break;
		}
		printf("check\n");
		//0초과시
		while (1) {
			if (read(sock, &chat_type, 1) != 1) //들어오는 정보의 타입을 체크한다. 체팅 : 0, 파일 : 1, 초대 : 2, 뒤로가기(체팅방 재설정) : 3
				error_handling("수신 오류 : in user thread(chat_type)");

			if (chat_type == TYPE_BACK) {
				pthread_mutex_lock(&mutex);
				group_buffer_addr->user_count--;
				pthread_mutex_unlock(&mutex);
				pthread_cancel(send_thread_id);//전송 스레드 강제 종료
				printf("<OPTION> : BACK\n");
				break;//뒤로가기
			}
			switch (chat_type)
			{
				case TYPE_CHAT://뮤택스 락설정 고려
					//chat_data 생성
					if ((read(sock, &text_sz, sizeof(text_sz)) != 4)) {
						error_handling("수신 오류1 : in user thread(chat_type)");
					}
					chat_text_buffer = (char*)malloc(text_sz);
					if (read(sock, chat_text_buffer, text_sz) != text_sz) {
						error_handling("수신 오류2 : in user thread(chat_type)");
					}
					chat = make_chat(service_user->name, chat_text_buffer);
					if (group_buffer_addr->size == MAX_CHAT_BUF_SIZE) {//채팅방 버퍼가 가득찼을때
						//접속한 사용자가 모두 체팅기록은 읽는지 확인
						while (group_buffer_addr->user_count/2 != group_buffer_addr->buffer_check_user_count);
						pthread_mutex_lock(&mutex);
						clear_buffer(group_buffer_addr);
						pthread_mutex_unlock(&mutex);
					}
					pthread_mutex_lock(&mutex);
					write_chat(group_buffer_addr, chat);
					pthread_mutex_unlock(&mutex);
					break;
				case TYPE_READ_FILE:
					fbuffer = (char*)malloc(100);//옜옜 100옜� 옜�
					printf("file send\n");
					read(sock, &flen, sizeof(flen));
					read(sock, fbuffer, flen);
					printf("file name : %s\n", fbuffer);
					fbuffer[flen] = '\0';

					*(write_prameter.fp) = fopen(fbuffer, "rb");
					free(fbuffer);
					break;
				case TYPE_FILE: //파일받기?????????????????????????????
					if(fp != NULL){
						//옜 옜 옜� 옜옜�
						//옜 옜 옜 옜 옜옜옜옜?	
					}
					fbuffer = (char*)malloc(F_BUF_SZ);
					printf("file receive\n");
					read(sock, &flen, sizeof(flen));
					read(sock, fbuffer, flen);
					printf("file name : %s\n", fbuffer);
					fbuffer[flen] = '\0';
					fp = fopen(fbuffer, "wb");
						
					do{
						read(sock, &flen, sizeof(flen));
						read(sock, fbuffer, flen);
						fwrite(fbuffer,1, flen, fp);
					}while(flen == F_BUF_SZ);
					free(fbuffer);
					fclose(fp);

					printf("file receive complete\n");

					break;
				case TYPE_INVITE://체팅방 초대
					//초대하는 상대방의 고유 id전송 받음
					if (read(sock, &invite_user_id, sizeof(int)) != sizeof(int)) {
						error_handling("수신 오류 : in user thread(chat_type)");
					}
					//해당하는 유저의 정보공간을 탐색
					invite_user = find_user_information(invite_user_id);
					//유저의 그룹 정보를 업데이트한다.
					invite_user->group_ID = (int*)realloc(invite_user->group_ID, sizeof(int) * (invite_user->group_count + 1));
					invite_user->group_ID[invite_user->group_count] = group_buffer_addr->group_id;
					invite_user->group_count++;
					break;
				default:
					break;
			}
		}
	}
	//
	printf("user_thread exit\n");
	free(write_prameter.fp);
	free(buffer);
	close(sock);
	return NULL;
}

void* allocate_thread(void* argv) {//할당 쓰레드
	int i;
	int request_group_id;
	struct group_data_buffer* buffer;
	struct group_table* entry;
	struct group_table* prior;
	FILE* fp;
	char buf[25];
	char* filename;
	int filename_sz;

	printf("allocate run\n");
	while(1){
		//check queue
		request_group_id = check_request();		
		if(request_group_id != -1){
			//buffer allocate table update
			buffer = (struct group_data_buffer*)malloc(sizeof(struct group_data_buffer));
			filename_sz = sprintf(buf, "data/group/%d.txt", request_group_id);
			filename = (char*)malloc(sizeof(char) * filename_sz);
			strcpy(filename, buf);
			buffer->fp = fopen(filename, "a+");
			if (buffer->fp == NULL) {
				printf("file open error\n");
			}
			buffer->group_id = request_group_id;
			buffer->size = 0;
			buffer->user_count = 1;
			buffer->buffer_check_user_count = 0;
			for (i = 0; i < MAX_CHAT_BUF_SIZE; i++) {
				buffer->buffer[i] = NULL;
			}

			entry = make_group_table_entry(buffer);
			add_group_table_entry(entry);
			free(filename);
		}
		//free buffer
		entry = table->next;
		prior = table;
		while(entry != NULL){
			if(entry->addr->user_count == 0){
				free(entry->addr);
				prior->next = entry->next;
				free(entry);
				entry=prior->next;
				continue;	
			}
			prior = entry;
			entry = entry->next;
		}
	}
}
int main(int argc, char* argv[])
{
	pthread_mutex_init(&mutex, NULL);//뮤택스 초기화

	FILE* fp;
	int serv_sock;
	int clnt_sock;
	int i;

	pthread_t t_id;

	struct sockaddr_in serv_addr;
	struct sockaddr_in clnt_addr;
	socklen_t clnt_addr_size = sizeof(clnt_addr);

	if (argc != 2) {
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}

	fp = fopen("data/new_group_id.txt", "r");
	fscanf(fp, "%d", &new_group_id);
	fclose(fp);

	setting_user_information();//유저정보 읽기
	init_table();//group table 생성
	init_queue(); //요청 큐 생성
	if (pthread_create(&t_id, NULL, allocate_thread, NULL) != 0) {//할당 쓰레드 생성
		error_handling("thread error");
		return 0;
	}
	if (pthread_detach(t_id) != 0) {//할당 스레드 detach
		error_handling("allocate thread detach error");
	}

	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (serv_sock == -1)
		error_handling("socket() error");

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(atoi(argv[1]));

	if (bind(serv_sock, (struct sockaddr*) & serv_addr, sizeof(serv_addr)) == -1)
		error_handling("bind() error");

	if (listen(serv_sock, 10) == -1)
		error_handling("listen() error");

	for (i = 0; i < 5; i++)
	{
		clnt_sock = accept(serv_sock, (struct sockaddr*) & clnt_addr, &clnt_addr_size);
		if (clnt_sock == -1)
			error_handling("accept() error");
		else
			printf("Connected client %d \n", i + 1);

		//유저 쓰레드 생성
		if (pthread_create(&t_id, NULL, user_thread, (void*)&clnt_sock) != 0) {
			error_handling("thread error");
			return 0;
		}
		if (pthread_detach(t_id) != 0) {//유저 스레드 detach
			error_handling("user thread detach error");
		}
	}
	fp = fopen("data/new_group_id.txt", "w");
	fprintf(fp, "%d", new_group_id);
	fclose(fp);
	free(table);
	save_user_data();
	close(serv_sock);
	pthread_mutex_destroy(&mutex);
	return 0;
}
void send_group_id_to_user(struct User_information* service_user, int sock) {
	int i;
	write(sock, &service_user->group_count, sizeof(int));//사용자가 참여하고 있는 체팅방의 개수를 먼저 보낸다.
	for (i = 0; i < service_user->group_count; i++) {
		write(sock, service_user->group_ID + i, sizeof(int));//사용자의 채팅방 id 를 순차적으로 전송한다.
	}
}
int receive_group_id(int sock) {//선택된 그룹의 id를 받고 리턴 //체팅방 id 수신, -2 : 에러, -1 : 종료, 0 : 새로운 채팅방 생성, 0초과 : 체팅방 id
	int read_sz;
	int selected_group_id;
	if ((read_sz = read(sock, &selected_group_id, sizeof(int))) == -1) {
		error_handling("read_error_1");
	}
	if (read_sz != sizeof(int)){
		return -2;//-2 는 에러 값이다.
	}
	return selected_group_id;
}
