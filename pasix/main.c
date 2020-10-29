#include <avr/io.h>
#include <avr/interrupt.h>

extern void _dump_stack();
extern void _restore_stack();

#define dp_stack() \
	asm("in r16,0x3d");\
	asm("in r17,0x3e");\
	asm("sts 0x0060,r16");\
	asm("sts 0x0061,r17");\

static inline void dump_stack()
{
	asm("in r16,0x3d");
	asm("in r17,0x3e");
	asm("sts 0x0060,r16");
	asm("sts 0x0061,r17");
}

static inline void restore_stack()
{
	asm("lds r16,0x0060");
	asm("lds r17,0x0061");
	asm("out 0x3d,r16");
	asm("out 0x3e,r17");
}

struct process_s
{
	int pid;
	int state;
	
	uint16_t tss;
};

// native dump for entry.s
static uint16_t current_tss;

struct process_s *current;
struct process_s processes[2];

enum
{
	TASK_ZOMBIE = 0,
	TASK_RUNNABLE  = 1,
	TASK_INTERRUPTIBLE = 2
};

struct process_s* get_next()
{
	if (processes[1].state == TASK_RUNNABLE)
		return processes + 1;

	// fall back to idle process
	return processes;
}

void do_schedule()
{
	// dump current stack pointer
	asm("in r16,0x3d");
	asm("in r17,0x3e");
	asm("sts 0x0060,r16");
	asm("sts 0x0061,r17");
	// copy dump to process related struct
	memcpy(&current->tss, &current_tss, sizeof(current_tss));
	
	struct process_s *next = get_next();
	
	//if (current != next)
	{
		// switch process
		current = next;
		
		// load dump from process struct
		memcpy(&current_tss, &current->tss, sizeof(current_tss));
		// restore current stack SP only		
		asm("lds r16,0x0060");
		asm("lds r17,0x0061");
		asm("out 0x3d,r16");
		asm("out 0x3e,r17");
	}
}

typedef void (*user_main)();

extern void userland();

void do_fork(struct process_s *process, user_main main)
{
	current = process;
	
	process->pid = 0;
	process->state = TASK_RUNNABLE;

	main();
}

void do_idle()
{
	current = processes;
	current->pid = -1;
	
	while (1)
	{
	
		// begin with do schedule to set tss
		do_schedule();
		
		struct process_s *user = processes + 1;

		if (user->state == TASK_ZOMBIE)
		{
			do_fork(processes + 1, &userland);
		}

		// wakeup user
		user->state = TASK_RUNNABLE;
	}
}

void sys_fork(struct process_s *process, uint16_t *address)
{
	cli();
	do_fork(process, address);
	sei();
}

void sys_wait(long const sleep)
{
	current->state = TASK_INTERRUPTIBLE;
	do_schedule();
}

int main(void)
{
	// ensure current tss address
	if (&current_tss != 0x0060)
		return;
	
	// setup timer
	// checkout https://www.mikrocontroller.net/articles/AVR-GCC-Tutorial/Die_Timer_und_Z%C3%A4hler_des_AVR
	//TCCR0 = (0 << CS02 | 1 << CS01 | 0 << CS00);
	//TIMSK |= (1 << TOIE0); // enable overflow interrupt

	//sei(); // enable global interrupt
	
	// idle process will schedule userland process
	do_idle();
}

void userland()
{
	// setup stack of new process
	asm("ldi r16,0xff");
	asm("ldi r17,0x00");
	asm("out 0x3d,r16");
	asm("out 0x3e,r17");
	
	while (1)
	{
		// set state to TASK_INTERRUPTIBLE
		// this will invoke idle process
		// which will set state to TASK_RUNNING
		sys_wait(0);
	}
	
	// sys_exit() must be called in case of termination
	// for now we know user_main is never returning
}

ISR(TIMER0_OVF_vect, ISR_NAKED)
{
	
	reti();
}


