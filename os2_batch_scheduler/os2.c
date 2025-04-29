//https://yjg-lab.tistory.com/122
//https://m.blog.naver.com/PostView.naver?isHttpsRedirect=true&blogId=holy_joon&logNo=221583745847

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <limits.h>

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

#define LIST_POISON1  ((void *) 0x00100100)
#define LIST_POISON2  ((void *) 0x00200200)
//헤드 추가 & 초기화
struct list_head {
    struct list_head* next, * prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)

#define INIT_LIST_HEAD(ptr) do { \
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

static inline void __list_add(struct list_head* new,
    struct list_head* prev,
    struct list_head* next) {
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

static inline void list_add(struct list_head* new, struct list_head* head) {
    __list_add(new, head, head->next);
}

static inline void list_add_tail(struct list_head* new, struct list_head* head) {
    __list_add(new, head->prev, head);
}

static inline void __list_del(struct list_head* prev, struct list_head* next) {
    next->prev = prev;
    prev->next = next;
}

static inline void list_del(struct list_head* entry) {
    __list_del(entry->prev, entry->next);
    entry->next = LIST_POISON1;
    entry->prev = LIST_POISON2;
}

static inline void list_del_init(struct list_head* entry) {
    __list_del(entry->prev, entry->next);
    INIT_LIST_HEAD(entry);
}

static inline void list_move(struct list_head* list, struct list_head* head) {
    __list_del(list->prev, list->next);
    list_add(list, head);
}

static inline void list_move_tail(struct list_head* list,
    struct list_head* head) {
    __list_del(list->prev, list->next);
    list_add_tail(list, head);
}

static inline int list_empty(const struct list_head* head) {
    return head->next == head;
}

#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

#define list_for_each(pos, head) \
  for (pos = (head)->next; pos != (head);	\
       pos = pos->next)

#define list_for_each_prev(pos, head) \
	for (pos = (head)->prev; prefetch(pos->prev), pos != (head); \
        	pos = pos->prev)

#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)
//순회
#define list_for_each_entry(pos, head, member)				\
	for (pos = list_entry((head)->next, typeof(*pos), member);	\
	     &pos->member != (head);					\
	     pos = list_entry(pos->member.next, typeof(*pos), member))
//역순회
#define list_for_each_entry_reverse(pos, head, member)			\
	for (pos = list_entry((head)->prev, typeof(*pos), member);	\
	     &pos->member != (head); 	\
	     pos = list_entry(pos->member.prev, typeof(*pos), member))
//중간에 삭제해도 안전하게 순회
#define list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = list_entry((head)->next, typeof(*pos), member),	\
		n = list_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))
//중간에 삭제해도 안전하게 역순회
#define list_for_each_entry_safe_reverse(pos, n, head, member)		\
	for (pos = list_entry((head)->prev, typeof(*pos), member),	\
		n = list_entry(pos->member.prev, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = n, n = list_entry(n->member.prev, typeof(*n), member))

#if 0    //DEBUG
#define debug(fmt, args...) fprintf(stderr, fmt, ##args)
#else
#define debug(fmt, args...)
#endif


typedef struct {
    unsigned char operation; //operation
    unsigned char length;   //length
} code; //코드 구조체 


typedef struct {
    int pid; //ID
    int arrival_time; //도착시간
    int code_bytes; //코드길이(바이트)
    code* operations;
    struct list_head job, ready, wait;
} process; //프로세스 구조체

int totalclock = 0;
int Total_Clock(int Clock, process* cur, int i) { //현재 시간에 현재 명령의 소요시간을 더하는 함수
    totalclock = cur->operations[i].length + Clock;
    return totalclock;
}


int main(int argc, char* argv[]) {
    //이중 연결리스트는 각 노드가 선행 노드와 후속노드에 대한 링크를 가지는 리스트이다.
    //헤드노드: 데이터를 가지지 않고 오로지 삽입, 삭제코드를 간단하게 할 목적으로 만들어진 노드이다
    int present_pid = 0;
    int PC = 0; // Program counter 
    int Clock = 0;
    int Idle = 0;
    int Procs_cnt = 0; //process count
    int procs_cnt=0;   //현재까지 종료된 프로세스 수를 누적하는 용도
    int before_clk = 0;
    int switch_cnt=0;
    int next_pid=0;
    int pc1=0;
    int total_Idle =0;

    process* cur, * next;
    LIST_HEAD(job_q);
    LIST_HEAD(ready_q);
    LIST_HEAD(wait_q);

    // 리스트의 시작을 가리키는 head는 데이터에 포함되지 않은 단일 구조체여야 함
    // 선언과 초기화를 동시에 진행

    cur = malloc(sizeof(*cur));

    while (fread(cur, 12, 1, stdin) == 1) { //프로세스 구조체 크기만큼 읽어들임
        cur->operations = malloc((cur->code_bytes / 2) * sizeof(code));
        fread(cur->operations, ((cur->code_bytes / 2) * sizeof(code)), 1, stdin);
        INIT_LIST_HEAD(&cur->job);                // job node 초기화 (과제#1)
		list_add_tail(&cur->job, &job_q);         // job queue에 cur->job 추가 (과제#1)
        cur = malloc(sizeof(*cur));
    }

    //Idle 프로세스 추가
    cur = malloc(sizeof(*cur));
    cur->pid = 100;
    cur->arrival_time = 0;
    cur->code_bytes = 2;
    cur->operations = malloc((cur->code_bytes / 2) * sizeof(code));
    cur->operations[0].operation = 255; // 0xFF
    cur->operations[0].length = 0;
    INIT_LIST_HEAD(&cur->job);                 // job node 초기화 (과제#1)
    list_add_tail(&cur->job, &job_q);          // job queue에 cur->job 추가 (과제#1)

    //프로세스 추가 끝

    list_for_each_entry(cur, &job_q, job) { //프로세스 개수 세기
        if (cur->pid != 100) {
            Procs_cnt++; 
        }
    }
    /*
    한바퀴 돌면서 arrival_time이 0인걸 찾아야함 : 0번 프로세스, IDLE
    0인 프로세스들을 ready_q에 집어넣음
    ready_q에 먼저 들어가있는 프로세스의 operation을 실행
    operation의 길이만큼 clock 증가
    clock증가 하는 도중 clock == arrival_time이면 프로세스를 ready_q에 집어넣음
    그리고 length 만큼 clock이 다 차면 그 다음 operation 을 실행
    만약 실행하는게 I/O일경우 거기에 해당하는 process를 wait q에 보냄
    I/O작업이 끝나면 다시 ready_q로 복귀
    만약에 끝이나면 삭제
    */
    while(Clock <=totalclock){
        //1. job queue -> ready queue
        list_for_each_entry(cur,&job_q, job) {
            if (Clock == cur->arrival_time) {
                INIT_LIST_HEAD(&cur->ready);                // ready node 초기화 (과제#2)
                list_add_tail(&cur->ready, &ready_q);       // ready queue에 cur->ready 추가 (과제#2)       
                //로드된 프로세스 정보 출력 (과제#2)
                printf("%04d CPU: Loaded PID: %03d\tArrival: %03d\tCodesize:%03d\tPC: %03d\n",
                        Clock, cur->pid, cur->arrival_time, cur->code_bytes, 0);                            
            }//clock과 arrival_time이 같으면 ready_q에 추가
        }
        // 2. wait queue -> ready queue
        if(Clock == totalclock){ // IO 작업이 끝났다는 의미
            list_for_each_entry(cur,&wait_q,wait){
                if(present_pid == cur->pid){ // wait_q 프로세스 중 지금 실행 마친 cur 대상
                    INIT_LIST_HEAD(&cur->ready);             // ready 노드 초기화 (과제#2)
                    list_add_tail(&cur->ready, &ready_q);    // ready_q에 추가 (과제#2)
                    list_del_init(&cur->wait);               // wait_q에서 제거 및 초기화 (과제#2)
                    break;
                }
                break;
            }// Clock과 Totalclock이 같으면 wait_q에 있던 프로세스를 ready_q로 다시 추가(wait queue에서는 프로세스 제거)
            
        }
        
        // 3. ready queue -> CPU
        list_for_each_entry_safe(cur,next,&ready_q,ready){
            if(Clock == totalclock){
                // 현재 pid 저장 
                if(present_pid == -1 && cur->pid != 100){ // 빈 노드도 아니고, IDLE 프로세스도 아닐 때 
                    present_pid = cur->pid;
                    if(present_pid == cur->pid){
                        next_pid = next -> pid;
                    }
                }
                
                if(present_pid == cur->pid && cur->operations[PC].length == Clock-before_clk){
                    PC++; // CPU 작업이 끝나면 PC 증가
                    // context switching
                    if(PC == cur->code_bytes/2){ // 프로세스의 연산이 끝났음 
                        list_del_init(&cur->ready);                   //프로세스가 끝나면 ready_q에서 삭제 (과제#2)
                        procs_cnt +=1;
                        // 4. CPU -> terminated 
                        if(Procs_cnt == procs_cnt){
                            break;
                        }//프로세스의 개수와 삭제될때마다 추가된 프로세스 개수가 같으면(모든 프로세스 종료됨)->시뮬레이션 끝! 
                        present_pid = -1;
                        totalclock = Clock + 10;    // context switch 되기 때문에 Clock에 10 추가 (과제#2)
                                                    // Total clock 증가 (과제#2)
                        switch_cnt++;               //몇번 switching 됐나 카운트
                        PC=0;
                    }
                }
                //CPU작업인 경우
                if(present_pid == cur->pid && cur->operations[PC].operation == 0 && cur->operations[PC].length != Clock-before_clk){
                    Total_Clock(Clock,cur,PC);
                    before_clk = Clock;//CPU 작업   
                }
                // 5. CPU -> wait queue
                //IO작업인 경우
                if(present_pid == cur->pid && cur->operations[PC].operation == 1 && cur->operations[PC].length != Clock-before_clk){
                    Total_Clock(Clock, cur, PC);
                    printf("%04d CPU: OP_IO START len: %03d ends at: %04d\n",
                            Clock, cur->operations[PC].length, cur->operations[PC].length+Clock);   //IO작업 문구 출력
                    INIT_LIST_HEAD(&cur->wait);              //wait노드 초기화
                    list_add_tail(&cur->wait, &wait_q);      //wait queue에 wait노드 추가
                    list_del_init(&cur->ready);              //ready_q에서 삭제
                    Idle += cur->operations[PC].length;      //Idle 시간 확인
                    
                    before_clk = Clock;
                }
            }
        }

        Clock++;
    }
    total_Idle = Idle+switch_cnt*10;
    double u =0.0; 
    u =(totalclock-total_Idle);
    //TOTAL CLOCK 정보 출력 (과제#2)
    printf("***TOTAL CLOCKS: %04d IDLE: %04d UTIL: %2.2f%%\n",
            totalclock, total_Idle, (u/totalclock)*100);

}


