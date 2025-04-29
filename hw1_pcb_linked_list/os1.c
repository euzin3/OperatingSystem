//https://yjg-lab.tistory.com/122
//https://m.blog.naver.com/PostView.naver?isHttpsRedirect=true&blogId=holy_joon&logNo=221583745847

/*
    리눅스 커널 스타일의 이중 연결 리스트를 사용자 공간에서 구현하고,
    process 구조체로 구성된 프로세스 목록을 입력 파일로부터 읽어 들여 리스트에 저장한 뒤,
    역순 출력하고 메모리를 해제하는 프로그램. 
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <limits.h>

/* 리스트 구현부 */ 
// 구조체 TYPE 안에 있는 MEMBER의 Byte offset 구함. (구조체 시작 주소~멤버 사이)
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER) 

// 구조체 멤버의 포인터로부터 전체 구조체 포인터 구함(역추적). 
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})
		// __mptr은 member 멤버의 포인터>> 구조체 시작 주소 = member 주소 - member까지의 offset
		// (type *)(...) : 구조체 시작 주소를 type *으로 형변환 > 원래의 구조체 전체 포인터 리턴
#define LIST_POISON1  ((void *) 0x00100100) // 삭제된 노드의 next에 넣는 이상한 주소 (삭제된 노드 실수로 접근 시 오류 나도록.)
#define LIST_POISON2  ((void *) 0x00200200) // 삭제된 노드의 prev에 넣는 이상한 주소


/* 리스트 구조체 및 초기화 */
//헤드 추가 & 초기화
struct list_head {
	struct list_head *next, *prev;
};

// name이라는 list_head 구조체 있을 때, next, prev 모두 자기자신 가리키도록 초기화 (빈 리스트로 세팅)
#define LIST_HEAD_INIT(name) { &(name), &(name) }

// list_head 변수 선언과 동시에 초기화
#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name) 

// 이미 선언되어 있는 list_head의 포인터 ptr 초기화
#define INIT_LIST_HEAD(ptr) do { \
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)


/* 리스트 삽입,삭제 함수 */
// 리스트의 prev, next 사이에 new 노드 추가 함수 
static inline void __list_add(struct list_head *new,
			      struct list_head *prev,
			      struct list_head *next) {
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}
// 리스트 맨 앞(head 바로 뒤)에 new 노드 추가
static inline void list_add(struct list_head *new, struct list_head *head) {
	__list_add(new, head, head->next);
}

/* 
	queue 구현을 위해 꼭 필요한 부분 !! 
	= Queue를 연결리스트로 구현할 때는 새로운 노드를 tail (끝)에 추가해야 함. 

	[Queue의 동작 원리 (FIFO)]
	- 삽입 : 뒤에서 추가 -> tail에 노드 추가 
	- 삭제 : 앞에서 제거 -> head에서 노드 제거

*/ 
// 리스트 맨 뒤(head 바로 앞)에 new 노드 추가
static inline void list_add_tail(struct list_head *new, struct list_head *head) {
	__list_add(new, head->prev, head);
}

// 리스트 prev, next 사이 new 노드 제거 함수
static inline void __list_del(struct list_head * prev, struct list_head * next) {
	next->prev = prev;
	prev->next = next;
}
static inline void list_del(struct list_head *entry) {
	__list_del(entry->prev, entry->next);
	// 삭제한 entry 노드를 다시 사용하면 터지도록 보호용으로 POISON 주소로 덮어씀. 
	entry->next = LIST_POISON1;
	entry->prev = LIST_POISON2;
}
// 노드 삭제 후 재사용 노드로 생성
static inline void list_del_init(struct list_head *entry) {
	__list_del(entry->prev, entry->next); // 리스트에서 entry 제거 
	INIT_LIST_HEAD(entry); // 다시 빈 리스트로 초기화
}


/* 노드 이동, 빈 노드 점검 함수 */
// list라는 list_head 멤버 노드를 리스트에서 빼서 맨앞으로 붙임. 
static inline void list_move(struct list_head *list, struct list_head *head) {
        __list_del(list->prev, list->next);
        list_add(list, head); // head가 가리키는 리스트의 앞쪽(head 다음)으로 이동시킴.
}
// list라는 list_head 멤버 노드를 리스트에서 빼서 맨뒤로 붙임. 
static inline void list_move_tail(struct list_head *list,
				  struct list_head *head) {
        __list_del(list->prev, list->next);
        list_add_tail(list, head); // head가 가리키는 리스트의 뒤쪽(head 이전)으로 이동시킴.
}

static inline int list_empty(const struct list_head *head) {
	return head->next == head; // 리스트 비어있는지 확인
}

// 전체 구조체의 포인터 구함
#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)
// 리스트 앞에서부터 순회
#define list_for_each(pos, head) \
  for (pos = (head)->next; pos != (head);	\
       pos = pos->next)
// 리스트 역방향으로 순회
#define list_for_each_prev(pos, head) \
	for (pos = (head)->prev; prefetch(pos->prev), pos != (head); \
        	pos = pos->prev)
// 리스트 순회하면서 중간에 노드 삭제해도 안전하게 동작 (pos : 현재 노드, n : 다음 노드 백업)
#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)

// 순회 (리스트 노드 아닌, 구조체 전체를 순회)
#define list_for_each_entry(pos, head, member)				\
	for (pos = list_entry((head)->next, typeof(*pos), member);	\
	     &pos->member != (head);					\
	     pos = list_entry(pos->member.next, typeof(*pos), member))
// 역순회
#define list_for_each_entry_reverse(pos, head, member)			\
	for (pos = list_entry((head)->prev, typeof(*pos), member);	\
	     &pos->member != (head); 	\
	     pos = list_entry(pos->member.prev, typeof(*pos), member))
//중간에 삭제해도 안전하게 순회 (pos : 현재 구조체, n : 다음 구조체 백업)
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


typedef struct{
    unsigned char operation; //operation
    unsigned char length;   //length
} code; //코드 구조체 


typedef struct {
    int pid; //ID
    int arrival_time; //도착시간
    int code_bytes; //코드길이(바이트)
    code *operations;
    struct list_head job, ready, wait;
} process; //프로세스 구조체


int main(int argc, char* argv[]){
    //이중 연결리스트는 각 노드가 선행 노드와 후속노드에 대한 링크를 가지는 리스트이다.
    //헤드노드: 데이터를 가지지 않고 오로지 삽입, 삭제코드를 간단하게 할 목적으로 만들어진 노드이다

    process *cur, *next; //포인터 생성 

    LIST_HEAD(job_q); //job_q 헤드 생성
	LIST_HEAD(ready_q); //ready_q 헤드 생성
	LIST_HEAD(wait_q); //wait_q 헤드 생성
	// 리스트의 시작을 가리키는 head는 데이터에 포함되지 않은 단일 구조체여야 함
	// 선언과 초기화를 동시에 진행

    cur = malloc(sizeof(*cur)); // 새 process 구조체 공간 할당

    while(fread(cur,12,1,stdin) == 1){ //프로세스 구조체 크기만큼 읽어들임
        // operation 개수 = code_bytes/2 이므로, 그만큼 code 구조체 할당 후 읽어옴. 
		cur->operations = malloc((cur->code_bytes/2)*sizeof(code));
        fread(cur->operations,((cur->code_bytes/2)*sizeof(code)),1,stdin);
		
        INIT_LIST_HEAD(&cur->job); // job노드 초기화 (코드 작성)
		list_add_tail(&cur->job, &job_q); // job queue에 job노드 추가 (코드 작성)
        
		cur = malloc(sizeof(*cur)); // 다음 프로세스 읽기 위해 새 구조체 할당
    }

	//반대로 출력하는 코드 작성
    list_for_each_entry_reverse(cur, &job_q, job){
        printf("PID: %03d\t Arrival: %03d\t CODESIZE: %03d\n", 
              cur->pid, cur->arrival_time, cur->code_bytes);
        for(int i = 0; i < cur->code_bytes/2; i++) {
            printf("%d %d\n", cur->operations[i].operation, cur->operations[i].length);
        }
    }

    return 0;
	
}