#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGESIZE (32)
#define PAS_FRAMES (256) 
#define PAS_SIZE (PAGESIZE * PAS_FRAMES)
#define VAS_PAGES (64)
#define PTE_SIZE (4)
#define PAGE_INVALID (0)
#define PAGE_VALID (1)
#define MAX_REFERENCES (256)
#define MAX_PROCESSES (10)
#define L1_PT_ENTRIES (8)
#define L2_PT_ENTRIES (8)

typedef struct {
    unsigned char frame;
    unsigned char vflag;
    unsigned char ref;
    unsigned char pad;
} pte;

typedef struct {
    int pid;
    int ref_len;
    unsigned char *references;
    pte *L1_page_table;
    int page_faults;
    int ref_count;
} process;

unsigned char pas[PAS_SIZE];
int allocated_frame_count = 0;

int allocate_frame() {
    if (allocated_frame_count >= PAS_FRAMES) 
        return -1;
    return allocated_frame_count++;
}

// 페이지 테이블 프레임을 하나 할당하고, 해당 프레임을 0으로 초기화하여 반환하는 함수
// 2단계 페이지 테이블 구조에서 1단계/2단계 모두 8개 엔트리만 필요하므로 프레임 하나만 할당
// 반환값: 할당된 페이지 테이블의 시작 주소(실패 시 NULL)
pte *allocate_pagetable_frame() {
    int frame = allocate_frame(); // 사용 가능한 프레임 번호 할당
    if (frame == -1) 
        return NULL; // 프레임 할당 실패 시 NULL 반환
    pte *page_table_ptr = (pte *)&pas[frame * PAGESIZE]; // 프레임 시작 주소를 pte 포인터로 변환
    memset(page_table_ptr, 0, PAGESIZE); // 해당 프레임(32B)을 0으로 초기화
    return page_table_ptr; // 페이지 테이블 포인터 반환
}

int load_process(FILE *fp, process *proc) {
    if (fread(&proc->pid, sizeof(int), 1, fp) != 1) 
        return 0;
    if (fread(&proc->ref_len, sizeof(int), 1, fp) != 1) 
        return 0;
    proc->references = malloc(proc->ref_len);
    if (fread(proc->references, 1, proc->ref_len, fp) != proc->ref_len) 
        return 0;

    printf("%d %d\n", proc->pid, proc->ref_len);
    for (int i = 0; i < proc->ref_len; i++) {
        printf("%02d ", proc->references[i]);
    }
    printf("\n");

    proc->page_faults = 0;
    proc->ref_count = 0;
    if ((proc->L1_page_table = allocate_pagetable_frame()) == NULL)
        return -1;
    return 1;
}

//HW3-2
void simulate(process *procs, int proc_count) {

    printf("simulate() start\n");

    int active = 1;
    // 모든 프로세스의 참조가 끝날 때까지 반복
    while (active) {
        active = 0;

        for (int i = 0; i < proc_count; i++) {
            process *p = &procs[i];

            // 현재 프로세스의 참조 시퀀스가 모두 처리되었으면 skip
            if (p->ref_count >= p->ref_len)
                continue;

            active = 1; // 아직 참조할 페이지가 남아있는 프로세스가 있음

            unsigned char page = p->references[p->ref_count];
            unsigned int l1_idx = page / L2_PT_ENTRIES; // L1 페이지 테이블 인덱스 계산
            unsigned int l2_idx = page % L2_PT_ENTRIES; // L2 페이지 테이블 인덱스 계산

            pte *l1_pte = &p->L1_page_table[l1_idx];

            printf("[PID %02d IDX:%03d] Page access %03d: ", p->pid, p->ref_count, page);

            // L1 페이지 테이블 접근
            if (l1_pte->vflag == PAGE_INVALID) {
                // L2 페이지 테이블용 프레임 할당
                int l2_frame = allocate_frame();
                if (l2_frame == -1) {
                    printf("Out of physical memory (L1)!\n");
                    exit(1);
                }
                // L1 PTE 갱신
                l1_pte->frame = l2_frame;
                l1_pte->vflag = PAGE_VALID;

                // 새로 할당한 L2 페이지 테이블 초기화
                pte *l2_page_table = (pte *)&pas[l2_frame * PAGESIZE];
                memset(l2_page_table, 0, PAGESIZE);

                printf("(L1PT) PF -> Allocated Frame %03d(PTE %03d), ", l2_frame, l1_idx);

                p->page_faults++;
            } else {
                printf("(L1PT) Frame %03d, ", l1_pte->frame);
            }

            // L2 페이지 테이블 접근
            pte *l2_page_table = (pte *)&pas[l1_pte->frame * PAGESIZE];
            pte *l2_pte = &l2_page_table[l2_idx];

            if (l2_pte->vflag == PAGE_INVALID) {
                // 실제 페이지용 프레임 할당
                int page_frame = allocate_frame();
                if (page_frame == -1) {
                    printf("Out of physical memory (L2)!\n");
                    exit(1);
                }

                // L2 PTE 갱신
                l2_pte->frame = page_frame;
                l2_pte->vflag = PAGE_VALID;
                l2_pte->ref = 1;

                printf("(L2PT) PF -> Allocated Frame %03d\n", page_frame);

                p->page_faults++;
            } else {
                // 이미 valid한 경우 참조 카운트만 증가
                l2_pte->ref++;
                printf("(L2PT) Frame %03d\n", l2_pte->frame);
            }

            // 다음 참조로 이동
            p->ref_count++;
        }
    }

    printf("simulate() end\n");
}

//HW3-2
void print_page_tables(process *procs, int proc_count) {

    int total_pf = 0, total_ref = 0, total_alloc_frames = 0;

    for (int i = 0; i < proc_count; i++) {
        process *p = &procs[i];
        int proc_alloc_frames = 0;

        printf("** Process %03d: Allocated Frames=", p->pid);

        // 프로세스별 프레임 개수 계산
        // L1 PT 프레임은 아래에서 전체에서 따로 보정해줌

        // L2 PT 프레임 + 실제 페이지 프레임 개수 계산
        for (int l1_idx = 0; l1_idx < L1_PT_ENTRIES; l1_idx++) {
            pte *l1_pte = &p->L1_page_table[l1_idx];

            if (l1_pte->vflag == PAGE_VALID) {
                proc_alloc_frames += 1; // L2 PT 프레임 1개

                pte *l2_page_table = (pte *)&pas[l1_pte->frame * PAGESIZE];
                for (int l2_idx = 0; l2_idx < L2_PT_ENTRIES; l2_idx++) {
                    pte *l2_pte = &l2_page_table[l2_idx];
                    if (l2_pte->vflag == PAGE_VALID) {
                        proc_alloc_frames += 1; // 실제 페이지 프레임 1개
                    }
                }
            }
        }

        // 프로세스별 결과 출력
        printf("%03d PageFaults/References=%03d/%03d\n", proc_alloc_frames, p->page_faults, p->ref_count);

        // L1 페이지 테이블 엔트리 출력
        for (int l1_idx = 0; l1_idx < L1_PT_ENTRIES; l1_idx++) {
            pte *l1_pte = &p->L1_page_table[l1_idx];

            if (l1_pte->vflag == PAGE_VALID) {
                printf("(L1PT) [PTE] %03d -> [FRAME] %03d\n", l1_idx, l1_pte->frame);

                pte *l2_page_table = (pte *)&pas[l1_pte->frame * PAGESIZE];
                for (int l2_idx = 0; l2_idx < L2_PT_ENTRIES; l2_idx++) {
                    pte *l2_pte = &l2_page_table[l2_idx];
                    if (l2_pte->vflag == PAGE_VALID) {
                        unsigned int page_num = l1_idx * L2_PT_ENTRIES + l2_idx;
                        printf("(L2PT) [PAGE] %03d -> [FRAME] %03d REF=%03d\n", page_num, l2_pte->frame, l2_pte->ref);
                    }
                }
            }
        }

        // 전체 통계 누적
        total_pf += p->page_faults;
        total_ref += p->ref_count;
        total_alloc_frames += proc_alloc_frames;
    }
    
    // L1 PT 프레임은 각 프로세스마다 1개씩 별도로 존재하므로 총 proc_count만큼 추가
    total_alloc_frames += proc_count;

    // 전체 결과 출력
    printf("Total: Allocated Frames=%03d Page Faults/References=%03d/%03d\n",
           total_alloc_frames, total_pf, total_ref);
}


int main() {
    process procs[MAX_PROCESSES];
    int count = 0;

    printf("load_process() start\n");
    while (count < MAX_PROCESSES) {
        int ret = load_process(stdin, &procs[count]);
        if (ret == 0) 
            break;
        if (ret == -1) {
            printf("Out of memory!!\n");
            return 1;
        }
        count++;
    }
    printf("load_process() end\n");

    simulate(procs, count);
    print_page_tables(procs, count);

    for (int i = 0; i < count; i++) {
        free(procs[i].references);
    }

    return 0;
}
