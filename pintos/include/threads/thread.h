#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#ifdef VM
#include "vm/vm.h"
#endif

/* 스레드의 생명 주기 상태. */
enum thread_status
{
	THREAD_RUNNING, /* 0: 현재 실행 중인 스레드. */
	THREAD_READY,	/* 1: 실행 준비가 되었지만, CPU를 할당받지 못한 스레드 (Ready List에 있음). */
	THREAD_BLOCKED, /* 2: 특정 이벤트를 기다리며 대기 중인 스레드 (Sleep, Lock 대기 등). */
	THREAD_DYING	/* 3: 종료될 예정인 스레드. */
};

/* 스레드 식별자 타입 (Thread identifier type).
   원하는 타입으로 재정의할 수 있습니다. */
typedef int tid_t;
#define TID_ERROR ((tid_t) - 1) /* tid_t의 에러 값. */

/* 스레드 우선순위. */
#define PRI_MIN 0	   /* 가장 낮은 우선순위. */
#define PRI_DEFAULT 31 /* 기본 우선순위. */
#define PRI_MAX 63	   /* 가장 높은 우선순위. */

/* 커널 스레드 또는 유저 프로세스.
 *
 * 각 스레드 구조체는 자신만의 4 kB 페이지에 저장됩니다.
 * 스레드 구조체 자체는 페이지의 맨 아래(오프셋 0)에 위치합니다.
 * 페이지의 나머지 부분은 스레드의 커널 스택을 위해 예약되어 있으며,
 * 이 스택은 페이지의 맨 위(오프셋 4 kB)에서 아래쪽으로 자랍니다.
 * * (중략)...
 *
 * 1. 'struct thread'가 너무 커지면 안 됩니다.
 * (커널 스택을 위한 공간이 부족해짐)
 *
 * 2. 커널 스택이 너무 커지면 안 됩니다.
 * (스택 오버플로우가 발생하면 스레드 상태를 망가뜨림)
 *
 * ... 스택 오버플로우가 발생하면 'magic' 멤버를 건드려서
 * thread_current()에서 assertion failure를 발생시킬 것입니다.
 */

/* 'elem' 멤버는 이중 목적을 가집니다.
 * run 큐 (thread.c)의 요소가 될 수도 있고,
 * 세마포어 대기 리스트 (synch.c)의 요소가 될 수도 있습니다.
 * 이 두 가지 방식은 상호 배타적이기 때문에(mutually exclusive) 가능합니다:
 * READY 상태의 스레드만 run 큐에 있고,
 * BLOCKED 상태의 스레드만 세마포어 대기 리스트에 있기 때문입니다.
 *
 * [Project 1: Alarm Clock 관련]
 * 이 'elem'은 timer_sleep() 구현 시 'sleeping_threads' 리스트에
 * 스레드를 추가할 때도 사용됩니다. (BLOCKED 상태)
 */
struct thread
{
	/* thread.c가 소유 */
	tid_t tid;				   /* 스레드 식별자 (TID). */
	enum thread_status status; /* 스레드 상태 (RUNNING, READY, BLOCKED, DYING). */
	char name[16];			   /* 스레드 이름 (디버깅용). */
	int priority;			   /* 우선순위. */

	/* thread.c와 synch.c 간에 공유됨 */
	struct list_elem elem; /* 리스트 요소 (Ready List, Sleep List, Wait List 등에 사용됨). */

	/* ----- [Project 1: Alarm Clock] ----- */
	/* 가이드에 따라 스레드가 깨어나야 할 절대적인 틱 시간을 저장하기 위해 추가합니다. */
	/* timer_sleep()에서 이 값을 설정하고, timer_interrupt()에서 이 값을 검사합니다. */
	int64_t wakeup_tick;
	/* ------------------------------------ */

#ifdef USERPROG
	/* Project 2 (User Programs)에서 사용됨. */
	uint64_t *pml4; /* 페이지 맵 레벨 4 (가상 메모리용) */
#endif
#ifdef VM
	/* Project 3 (Virtual Memory)에서 사용됨. */
	struct supplemental_page_table spt; /* 보조 페이지 테이블 */
#endif

	/* thread.c가 소유 */
	struct intr_frame tf; /* 컨텍스트 스위칭을 위한 정보 (레지스터 값 등). */
	unsigned magic;		  /* 스택 오버플로우 감지를 위한 매직 넘버 (THREAD_MAGIC). */
};

/* false (기본값)이면 라운드 로빈 스케줄러 사용.
   true이면 멀티 레벨 피드백 큐(MLFQS) 스케줄러 사용.
   커널 명령줄 옵션 "-o mlfqs"로 제어됨. */
extern bool thread_mlfqs;

/* 스레드 시스템 초기화 */
void thread_init(void);
/* 스레딩 시작 (초기 스레드 생성 후) */
void thread_start(void);

/* 매 타이머 틱마다 호출됨 (MLFQS 스케줄러 관련) */
void thread_tick(void);
/* 스레드 통계 출력 */
void thread_print_stats(void);

/* 스레드 실행 함수 타입 정의 */
typedef void thread_func(void *aux);
/* 새 스레드를 생성. */
tid_t thread_create(const char *name, int priority, thread_func *, void *);

/* [Project 1: Alarm Clock 관련]
 * 현재 스레드를 BLOCKED 상태로 만들고 다른 스레드를 스케줄링.
 * timer_sleep() 함수가 스레드를 재우기 위해 이 함수를 호출합니다.
 */
void thread_block(void);

/* [Project 1: Alarm Clock 관련]
 * BLOCKED 상태의 스레드(t)를 READY 상태로 변경하여 Ready List에 추가.
 * timer_interrupt()가 잠자는 스레드를 깨울 때 이 함수를 호출합니다.
 */
void thread_unblock(struct thread *);

/* [Project 1: Alarm Clock 관련]
 * 현재 실행 중인 스레드('struct thread'의 포인터)를 반환.
 * timer_sleep()이 현재 스레드의 wakeup_tick을 설정하기 위해 호출합니다.
 */
struct thread *thread_current(void);
/* 현재 스레드의 TID 반환 */
tid_t thread_tid(void);
/* 현재 스레드의 이름 반환 */
const char *thread_name(void);

/* 현재 스레드를 종료 (파괴). */
void thread_exit(void) NO_RETURN;
/* 현재 스레드가 CPU를 양보 (Ready List의 다음 스레드에게). */
void thread_yield(void);

/* 현재 스레드의 우선순위 반환 */
int thread_get_priority(void);
/* 현재 스레드의 우선순위 설정 */
void thread_set_priority(int);

/* (MLFQS 관련) nice 값 반환 */
int thread_get_nice(void);
/* (MLFQS 관련) nice 값 설정 */
void thread_set_nice(int);
/* (MLFQS 관련) recent_cpu 값 반환 */
int thread_get_recent_cpu(void);
/* (MLFQS 관련) load_avg 값 반환 */
int thread_get_load_avg(void);

/* 유저 모드에서 커널 모드로의 복귀를 처리 (인터럽트 프레임 복원) */
void do_iret(struct intr_frame *tf);

#endif /* threads/thread.h */