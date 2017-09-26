/**
*****************************************************************************
**
**  File        : syscalls.c
**
**  Abstract    : Atollic TrueSTUDIO Minimal System calls file
**
** 		          For more information about which c-functions
**                need which of these lowlevel functions
**                please consult the Newlib libc-manual
**
**  Environment : Atollic TrueSTUDIO
**
**  Distribution: The file is distributed “as is,” without any warranty
**                of any kind.
**
**  (c)Copyright Atollic AB.
**  You may use this file as-is or modify it according to the needs of your
**  project. This file may only be built (assembled or compiled and linked)
**  using the Atollic TrueSTUDIO(R) product. The use of this file together
**  with other tools than Atollic TrueSTUDIO(R) is not permitted.
**
*****************************************************************************
*/

/* Includes */
#include <stdint.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
//#include <sys/time.h>
#include <sys/times.h>


/* Variables */
#undef errno
extern int32_t errno;

register uint8_t * stack_ptr asm("sp");

uint8_t *__env[1] = { 0 };
uint8_t **environ = __env;


/* Functions */
void initialise_monitor_handles()
{
}

int _getpid(void)
{
	return 1;
}

int _kill(int32_t pid, int32_t sig)
{
	errno = EINVAL;
	return -1;
}

void _exit (int32_t status)
{
	_kill(status, -1);
	while (1) {}		/* Make sure we hang here */
}

int _write(int32_t file, uint8_t *ptr, int32_t len)
{
	int32_t i;

	/*if(file == 1)
	{*/
		for(i = 0; i < len; i++)
			fputc(ptr[i], stdout);
	/*}
	else if(file == 2)
	{
		for(i = 0; i < len; i++)
			fputc(ptr[i], stderr);
	}*/

	return len;
}

caddr_t _sbrk(int32_t incr)
{
	extern uint8_t end asm("end");
	static uint8_t *heap_end;
	uint8_t *prev_heap_end;
	extern uint8_t _max_heap;

	if (heap_end == 0)
		heap_end = &end;

	prev_heap_end = heap_end;
	if (heap_end + incr > &_max_heap)
	{
//		write(1, "Heap and stack collision\n", 25);
//		abort();
		errno = ENOMEM;
		return (caddr_t) -1;
	}

	heap_end += incr;

	return (caddr_t) prev_heap_end;
}

int _close(int32_t file)
{
	return -1;
}


int _fstat(int32_t file, struct stat *st)
{
	st->st_mode = S_IFCHR;
	return 0;
}

int _isatty(int32_t file)
{
	return 1;
}

int _lseek(int32_t file, int32_t ptr, int32_t dir)
{
	return 0;
}

int _read(int32_t file, uint8_t *ptr, int32_t len)
{
	return 0;
}

int _open(uint8_t *path, int32_t flags, ...)
{
	/* Pretend like we always fail */
	return -1;
}

int _wait(int32_t *status)
{
	errno = ECHILD;
	return -1;
}

int _unlink(uint8_t *name)
{
	errno = ENOENT;
	return -1;
}

int _times(struct tms *buf)
{
	return -1;
}

int _stat(uint8_t *file, struct stat *st)
{
	st->st_mode = S_IFCHR;
	return 0;
}

int _link(uint8_t *old, uint8_t *new)
{
	errno = EMLINK;
	return -1;
}

int _fork(void)
{
	errno = EAGAIN;
	return -1;
}

int _execve(uint8_t *name, uint8_t **argv, uint8_t **env)
{
	errno = ENOMEM;
	return -1;
}
