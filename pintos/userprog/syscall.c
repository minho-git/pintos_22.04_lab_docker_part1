#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
// void halt(void);
// void exit(int );
// pid_t fork (const char *);
// int wait (pid_t);
int write2 (int fd, const void *buffer, unsigned size);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void syscall_handler (struct intr_frame *f UNUSED) {

	int status = f->R.rax;
	
	switch (status) {
		case SYS_WRITE:
			size_t num = write2(f->R.rdi, (void *)f->R.rsi, f->R.rdx);
			f->R.rax = num;
			break;
		
		case SYS_EXIT:
			thread_exit();
		default:
			break;
	}



	// printf ("system call!\n");
	// thread_exit ();
}




int write2 (int fd, const void *buffer, unsigned size) {

	if (size == 0) {
		return -1;
	}

	if (fd == 1) {
		putbuf(buffer, size);
	}

	return size;
}

// void halt(void) {

// 	power_off();
// }

// void exit(int status) {

// 	// 현재 사용자 프로그램을 종료하고, 커널에 status를 반환합니다. 
// 	// 만약 프로세스의 부모가 이 프로세스를 wait한다면(아래 참조), 이 값이 반환될 상태 값입니다. 
// 	// 관례적으로 status가 0이면 성공을, 0이 아닌 값은 오류를 나타냅니다.


// 	return status;
// }