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

//�������� data
int new_group_id;

//�Լ�
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
		if (counting_chat < buf->size) {//ü�ù��� ��ȭ�� ������
			for (; counting_chat < buf->size; counting_chat++) {
				if (buf->buffer[counting_chat]->user_name != service_user->name) {//???
					len = strlen(buf->buffer[counting_chat]->user_name);
					if(write(sock, &len, sizeof(len)) == -1){
						error_handling("txt send error1");
					}
					if (write(sock, buf->buffer[counting_chat]->user_name, strlen(buf->buffer[counting_chat]->user_name)) == -1) {//ü�� �ۼ����� �̸� ����
						error_handling("txt send error2");
						return NULL;
					}
					len = strlen(buf->buffer[counting_chat]->text);
					if(write(sock, &len, sizeof(len)) == -1){
						error_handling("txt send error3");
					}
					if (write(sock, buf->buffer[counting_chat]->text, strlen(buf->buffer[counting_chat]->text)) == -1) {//ü�� ���� ����
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

		//�������� �߰�???????????
		//�� ��
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
	struct user_write_thread_parameter write_prameter;//���� �����忡 ������ ������ ������ ����

	char* buffer = malloc(USER_BUF_SIZE * sizeof(char));//�����͸� �ӽ������� �޴� ����
	char* chat_text_buffer;
	printf("user thread run\n");
	for (i = 0; i < 5; i++) {
		//id
		receive_data_len = read(sock, &user_id, sizeof(user_id));//������� ���̵�� user_buf_size �� �ʰ��ؼ��� �ȵȴ�.
		if (receive_data_len <= 0) {
			error_handling("login_ID_error");
			close(sock);
			return NULL;
		}
		service_user = find_user_information(user_id);
		if (service_user == NULL) {//�α��� ����
			printf("user information search error\n");
			login_success = 0;
			write(sock, &login_success, sizeof(login_success));
			continue;
		}
		login_success = 1;
		write(sock, &login_success, sizeof(login_success));
		//password
		receive_data_len = read(sock, buffer, USER_BUF_SIZE);
		if (receive_data_len <= 0) {//�α��� ����
			write(sock, &login_success, sizeof(login_success));
			continue;
		}
		if (strcmp(service_user->PW, buffer) == 0) {//��й�ȣ�� �´°��
			printf("connect user : %s\n", service_user->name);
			login_success = 1;
			write(sock, &login_success, sizeof(login_success));
			break;
		}
		else {//�Էµ� ��й�ȣ�� Ʋ�����
			login_success = 0;
			write(sock, &login_success, sizeof(login_success));
			continue;
		}
	}
	if (i == 5) {//5ȸ ���н� �α��� ���и� �˸��� �޼��� �۽�       
		printf("�α��� ����\n");
		login_success = 2;
		free(buffer);
		close(sock);
		return NULL;
	}
	send_group_id_to_user(service_user, sock);
	//���� ü�ù� ���� �а� �۽�
	while (1) {
		//ü�ù� ���� ����
		group_id = receive_group_id(sock); //ü�ù� id ����    , -2 : ����, -1 : ����, 0 : ���ο� ä�ù� ����, 0�ʰ� : ü�ù� id
		if (group_id < -1) {//���� �߻���
			error_handling("receive group id error");
			continue;
		}
		else if (group_id == 0) {//���ο� �� ������?????????????????????
			group_id = new_group_id;
			write(sock, &group_id, sizeof(group_id));
			new_group_id++;
		}
		else if (group_id == -1){
			break;
		}
		group_buffer_addr = find_group_buffer_addr(group_id);//ä�ù� �����ϰų� �����ϴ� ���, ���ο� ä�ù� 0 �� �ԷµǸ� ���ο� ü�ù��� ����� �����͸� ������Ʈ�Ѵ�.
		//���۽����� ���ڵ� �ʱ�ȭ
		write_prameter.sock = sock;
		write_prameter.group_id = group_id;
		write_prameter.user_id = user_id;
		write_prameter.fp = malloc(sizeof(FILE*));
		//�۽� ������ ����
		if (pthread_create(&send_thread_id, NULL, user_send_thread, (void*)&write_prameter) != 0) {
			error_handling("pthread_create_error");//���н� ���� �޽��� ����� ��� ����
			break;
		}
		printf("check\n");
		//0�ʰ���
		while (1) {
			if (read(sock, &chat_type, 1) != 1) //������ ������ Ÿ���� üũ�Ѵ�. ü�� : 0, ���� : 1, �ʴ� : 2, �ڷΰ���(ü�ù� �缳��) : 3
				error_handling("���� ���� : in user thread(chat_type)");

			if (chat_type == TYPE_BACK) {
				pthread_mutex_lock(&mutex);
				group_buffer_addr->user_count--;
				pthread_mutex_unlock(&mutex);
				pthread_cancel(send_thread_id);//���� ������ ���� ����
				printf("<OPTION> : BACK\n");
				break;//�ڷΰ���
			}
			switch (chat_type)
			{
				case TYPE_CHAT://���ý� ������ ���
					//chat_data ����
					if ((read(sock, &text_sz, sizeof(text_sz)) != 4)) {
						error_handling("���� ����1 : in user thread(chat_type)");
					}
					chat_text_buffer = (char*)malloc(text_sz);
					if (read(sock, chat_text_buffer, text_sz) != text_sz) {
						error_handling("���� ����2 : in user thread(chat_type)");
					}
					chat = make_chat(service_user->name, chat_text_buffer);
					if (group_buffer_addr->size == MAX_CHAT_BUF_SIZE) {//ä�ù� ���۰� ����á����
						//������ ����ڰ� ��� ü�ñ���� �д��� Ȯ��
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
					fbuffer = (char*)malloc(100);//���� 100��� ���
					printf("file send\n");
					read(sock, &flen, sizeof(flen));
					read(sock, fbuffer, flen);
					printf("file name : %s\n", fbuffer);
					fbuffer[flen] = '\0';

					*(write_prameter.fp) = fopen(fbuffer, "rb");
					free(fbuffer);
					break;
				case TYPE_FILE: //���Ϲޱ�?????????????????????????????
					if(fp != NULL){
						//�� �� ��� �����
						//�� �� �� �� ��������?	
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
				case TYPE_INVITE://ü�ù� �ʴ�
					//�ʴ��ϴ� ������ ���� id���� ����
					if (read(sock, &invite_user_id, sizeof(int)) != sizeof(int)) {
						error_handling("���� ���� : in user thread(chat_type)");
					}
					//�ش��ϴ� ������ ���������� Ž��
					invite_user = find_user_information(invite_user_id);
					//������ �׷� ������ ������Ʈ�Ѵ�.
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

void* allocate_thread(void* argv) {//�Ҵ� ������
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
	pthread_mutex_init(&mutex, NULL);//���ý� �ʱ�ȭ

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

	setting_user_information();//�������� �б�
	init_table();//group table ����
	init_queue(); //��û ť ����
	if (pthread_create(&t_id, NULL, allocate_thread, NULL) != 0) {//�Ҵ� ������ ����
		error_handling("thread error");
		return 0;
	}
	if (pthread_detach(t_id) != 0) {//�Ҵ� ������ detach
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

		//���� ������ ����
		if (pthread_create(&t_id, NULL, user_thread, (void*)&clnt_sock) != 0) {
			error_handling("thread error");
			return 0;
		}
		if (pthread_detach(t_id) != 0) {//���� ������ detach
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
	write(sock, &service_user->group_count, sizeof(int));//����ڰ� �����ϰ� �ִ� ü�ù��� ������ ���� ������.
	for (i = 0; i < service_user->group_count; i++) {
		write(sock, service_user->group_ID + i, sizeof(int));//������� ä�ù� id �� ���������� �����Ѵ�.
	}
}
int receive_group_id(int sock) {//���õ� �׷��� id�� �ް� ���� //ü�ù� id ����, -2 : ����, -1 : ����, 0 : ���ο� ä�ù� ����, 0�ʰ� : ü�ù� id
	int read_sz;
	int selected_group_id;
	if ((read_sz = read(sock, &selected_group_id, sizeof(int))) == -1) {
		error_handling("read_error_1");
	}
	if (read_sz != sizeof(int)){
		return -2;//-2 �� ���� ���̴�.
	}
	return selected_group_id;
}
