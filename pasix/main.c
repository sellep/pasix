#include <avr/io.h>
#include <avr/interrupt.h>

//extern void _dump_stack();
//extern void _restore_stack();

#define DUMP_STACK() \
	asm("in r16,0x3d"); \
	asm("in r17,0x3e"); \
	asm("sts 0x0060,r16"); \
	asm("sts 0x0061,r17")

#define RESTORE_STACK() \
	asm("lds r16,0x0060"); \
	asm("lds r17,0x0061"); \
	asm("out 0x3d,r16"); \
	asm("out 0x3e,r17")
	
void dump_stack()
{
	DUMP_STACK();
}

void restore_stack()
{
	RESTORE_STACK();
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
	struct process_s *next = get_next();
	
	//if (current != next)
	{
		DUMP_STACK();
		current->tss = current_tss;
		
		// switch process
		current = next;
		
		// load process context
		current_tss = current->tss;
		RESTORE_STACK();
	}
	
	sei();
}

typedef void (*user_main)();

extern void userland();

void do_exit()
{
	current->state = TASK_ZOMBIE;
	do_schedule();
}

void do_fork(struct process_s *process, user_main main)
{
	// setup stack of new process
	asm("ldi r16,0xff");
	asm("ldi r17,0x00");
	asm("out 0x3d,r16");
	asm("out 0x3e,r17");
	
	current = process;
	
	process->pid = 0;
	process->state = TASK_RUNNABLE;

	main();
	
	do_exit();
}

void do_idle()
{
	current = processes;
	current->pid = -1;

	// dump stack pointer
	do_schedule();
	
	// enable global interrupts
	sei();

	while (1)
	{
		struct process_s *user = processes + 1;
		
		// disable interrupts
		cli();
		
		if (user->state == TASK_ZOMBIE)
		{
			do_fork(user, &userland);
		}
		
		do_schedule();
	}
}

void sys_wait(long const sleep)
{
	cli();
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
	TCCR0 = (0 << CS02 | 1 << CS01 | 0 << CS00);
	TIMSK |= (1 << TOIE0); // enable overflow interrupt

	// idle process will schedule userland process
	do_idle();
}

void userland()
{
	int i;
	
	for (i = 0; i < 10; i++)
	{
		sys_wait(0);
	}
}

ISR(TIMER0_OVF_vect, ISR_NAKED)
{
	struct process_s *user = processes + 1;
	
	if (user->state == TASK_INTERRUPTIBLE)
	{
		user->state = TASK_RUNNABLE;
	}
	
	reti();
}


