#define REQUEST_QUEUE_MAX_SIZE 100
#define MAX_CHAT_BUF_SIZE 10
#define DELIMITER "&"


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;//���ý� ����

void error_handling(char* message);

struct User_information {//�׷� id�� ������ � ������ ����Ұǰ�?
	int ID;
	char* PW;
	char* name;
	int group_count;
	int* group_ID;//�迭
	struct User_information* next;
};
struct User_information* user_information_root;//���������� ã�� ���� ���� �ּ�
//���Ϸ� ���� ���� ������ �а� �����ϴ� �Լ�
void setting_user_information() {
	user_information_root = (struct User_information*)malloc(sizeof(struct User_information));
	struct User_information* p = user_information_root;
	p->next = NULL;
	struct User_information* prior = NULL;

	FILE* fp;
	fp = fopen("data/user_information.txt", "r");
	int i;

	int pw_len;
	int name_len;

	printf("---------USER_INFORMATION------------\n");
	while (fscanf(fp, "%d %d %d %d", &p->ID, &pw_len, &name_len, &p->group_count) != EOF) {
		p->PW = (char*)malloc(sizeof(char) * pw_len);
		p->name = (char*)malloc(sizeof(char) * name_len);
		p->group_ID = (int*)malloc(sizeof(int) * p->group_count);
		if (fscanf(fp, "%s %s", p->PW, p->name) == EOF) {
			printf("file open error\n");
		}
		printf("name : %s => id : %d, pw : %s\ngroup_id : ", p->name, p->ID, p->PW);
		for (i = 0; i < p->group_count; i++) {
			if (fscanf(fp, "%d", p->group_ID + i) == EOF) {
				break;
			}
			printf("%d,", p->group_ID[i]);
		}
		printf("\n");
		p->next = (struct User_information*)malloc(sizeof(struct User_information));
		prior = p;
		p = p->next;
	}
	printf("-------------------------------------\n");
	if (prior != NULL) {
		prior->next = NULL;
	}
	free(p);
}
//���� ������ ���� �ÿ� ���Ͽ� �����ϴ� �Լ�
void save_user_data() {
	FILE* fp;
	fp = fopen("data/user_information.txt", "w+");
	int i;
	struct User_information* p = user_information_root;
	while(p != NULL){
		fprintf(fp, "%d %ld %ld %d %s %s ", p->ID, strlen(p->PW), strlen(p->name), p->group_count, p->PW, p->name);
		for(i = 0; i< p->group_count; i++){
			fprintf(fp, "%d ", p->group_ID[i]);
		}
		fprintf(fp, "\n");
		p= p->next;
	}
}
//������ �ش� ������ Ž���ϰ� �����ϴ� �Լ�
struct User_information* find_user_information(int id) {
	struct User_information* start = user_information_root;
	while (start->ID != id) {		
		start = start->next;
		if (start == NULL) return NULL;
	}
	return start;
}
//ü�ù� ������ ���� ����
typedef struct chat_data {
	char* user_name;
	char* text;
}chat_data;
chat_data* make_chat(char* user, char* t) {
	chat_data* c = malloc(sizeof(chat_data));
	c->user_name = user;
	c->text = t;
	return c;
}
struct group_data_buffer {
	FILE* fp;
	int group_id;
	int size;
	int user_count;
	int buffer_check_user_count;
	chat_data* buffer[MAX_CHAT_BUF_SIZE];
};
void clear_buffer(struct group_data_buffer* buf){//���� ������ �ʼ�
	int i;
	printf("buffer clear\n");//����
	//���Ͽ� ��� ü�� ����� ���
	for (i = 0; i < MAX_CHAT_BUF_SIZE; i++) {
		printf("file input\n");
		fprintf(buf->fp, "%s %s\n", buf->buffer[i]->user_name, buf->buffer[i]->text);
		free(buf->buffer[i]->text);//ü�� �ؽ�Ʈ ���� ���� �̸��� ���� ���� ������ �����ϴ� ���̱� ������ ��������
		free(buf->buffer[i]);//ü������ ���� �Ҵ� ����
		buf->buffer_check_user_count = buf->user_count;
		buf->size = 0;
		buf->buffer[i] = NULL;
	}
}
void write_chat(struct group_data_buffer* buf, chat_data* chat) {//������
	//�׷���ۿ� �����͸� �Է�
	buf->buffer[buf->size] = chat;
	buf->size++;
	buf->buffer_check_user_count = 0;
}

//�׷��� ������ ���� �ڷᱸ��
struct group_table {//���� ���
	int group_id;
	struct group_data_buffer* addr;
	struct group_table* next;
};

struct group_table* table;

void init_table() {
	table = (struct group_table*)malloc(sizeof(struct group_table));
	table->group_id = -1;//root_id -1 setting
	table->addr = NULL;
	table->next = NULL;
}
struct group_table* make_group_table_entry(struct group_data_buffer* addr) {
	struct group_table* p = (struct group_table*)malloc(sizeof(struct group_table));
	p->group_id = addr->group_id;
	p->addr = addr;
	p->next = NULL;
	printf("make buffer -> group id : %d, size : %d, user : %d\n", addr->group_id, addr->size, addr->user_count);
	return p;
}
void add_group_table_entry(struct group_table* item){
	struct group_table* tem = table;
	while (tem->next != NULL) {
		tem = tem->next;
	}
	tem->next = item;
}
void delete_group_table_entry(int group_id) {//???
	struct group_table* p = table;
	struct group_table* n = table->next;
	if (table->group_id == group_id) {
		table = n;
		free(p);
		return;
	}
	while (n->group_id != group_id) {
		if (n == NULL) {
			error_handling("delete error : ����� ���� �׷���Դϴ�.");
			return;
		}
		p = n;
		n = n->next;
	}
	n = n->next;
	free(p->next);
	p->next = n;
}
struct group_data_buffer* find_group_buffer_addr(int group_id) {// ���ο� ä�ù� 0 �� �ԷµǸ� ���ο� ü�ù��� ����� �����͸� ������Ʈ�Ѵ�.????
	struct group_table* p = table;
	int isRequest = 0;
	if (group_id == -1) {
		return NULL;
	}
	while (p->group_id != group_id) {
		p = p->next;
		if (p == NULL) {//�׷� ���̺� �������� �ʴ� ��� �ٽ� ã�´�.
			if (isRequest == 0) {
				printf("group buffer %d : request\n", group_id);
				request(group_id);//�޸� ���� �����忡�� �޸� ���� �Ҵ��� ��û�Ѵ�.	
				isRequest = 1;
			}
			p = table;
		}
		printf("next\n");
	}
	pthread_mutex_lock(&mutex);
	p->addr->user_count += (1-isRequest);//�ش� ������ ����� ����� �� ���ϱ�
	pthread_mutex_unlock(&mutex);
	return p->addr;
}
//�׷� ���� ��û ť
struct request_Queue {
	int front;
	int rear;
	int data[REQUEST_QUEUE_MAX_SIZE];
};
struct request_Queue queue;
void init_queue() {
	queue.rear = 0;
	queue.front = 0;
}
int is_full() {
	return ((queue.rear + 1) % REQUEST_QUEUE_MAX_SIZE == queue.front);
}
int is_empty() {
	return (queue.front == queue.rear);
}
void request(int group_id) {
	if (is_full()) {
		error_handling("��û ť ��ȭ ����");
		return;
	}
	queue.rear = (queue.rear + 1) % REQUEST_QUEUE_MAX_SIZE;
	queue.data[queue.rear] = group_id;
}
int check_request() {
	if (is_empty()) {
		return -1;
	}
	queue.front = (queue.front + 1) % REQUEST_QUEUE_MAX_SIZE;
	return queue.data[queue.front];
}
//
void error_handling(char* message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}
