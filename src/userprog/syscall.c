#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <devices/shutdown.h>
#include <filesys/filesys.h>
#include <filesys/file.h>
#include <devices/input.h>
#include "userprog/process.h"
#include "threads/synch.h"
#include "threads/malloc.h"

static void syscall_handler (struct intr_frame*);
void check_address(void *addr);
void get_argument(void *sp, int *arg, int count);
void halt (void);
void exit (int status);
bool create (const char *file, unsigned int initial_size);
bool remove (const char *file);
tid_t exec (char *cmd_line);
int wait (tid_t tid);
int write(int fd, void *buffer, unsigned size);
int read(int fd, void *buffer, unsigned size);
int filesize(int fd);
int open(const char *file_name);
void seek(int fd, unsigned int position);
unsigned tell(int fd);
void close(int fd);

void syscall_init (void)
{
	/* initialize filesys_lock */
	lock_init (&filesys_lock);
	intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler (struct intr_frame *f UNUSED)
{
	unsigned int *sp = (unsigned int*)(f -> esp);
	int number = *(int*)sp;
	int arg[4];

	check_address(sp);
	sp++;
	check_address(sp);

	switch(number)
	{
		case SYS_HALT:
			halt();		
			break;

		case SYS_EXIT:	
			get_argument(sp, arg, 1);		
			exit(arg[0]);
			break;		

		case SYS_CREATE:
			get_argument(sp, arg, 2);
			f -> eax = create((const char*)arg[0], (unsigned int)arg[1]); 
			break;		

		case SYS_REMOVE:
			get_argument(sp, arg, 1);	
			f -> eax = remove((const char*)arg[0]);	
			break;	

		case SYS_EXEC:
			get_argument(sp, arg, 1);
			f -> eax = exec((char*)arg[0]);
			break;

	 	case SYS_WAIT:
			get_argument(sp, arg, 1);
			f -> eax = wait(arg[0]);
			break;

	        case SYS_OPEN:
	         	get_argument(sp, arg, 1);
	                f -> eax = open((char*)arg[0]);
		        break;

	        case SYS_FILESIZE:
			get_argument(sp, arg, 1);
	                f -> eax = filesize(arg[0]); 
			break;

	        case SYS_READ:
	  		get_argument(sp, arg, 3);
	                f -> eax = read(arg[0], (void*)arg[1], arg[2]);
			break;

		case SYS_WRITE:
			get_argument(sp, arg, 3);
	                f -> eax = write(arg[0], (void*)arg[1], arg[2]);	
			break;

		case SYS_SEEK:
			get_argument(sp, arg, 2);
	                seek(arg[0], (unsigned int)arg[1]);
			break;

		case SYS_TELL:
			get_argument(sp, arg, 1);
			f -> eax = tell(arg[0]);
			break;

		case SYS_CLOSE:
			get_argument(sp, arg, 1);
			close(arg[0]);
			break;
			           
		default:
			break;
	}
}

/* check address whether kernel space or user space(user space : 0x8048000 ~ 0xc0000000)
if user space, end current process */
void check_address (void *addr)
{
	if ((unsigned int)addr <= 0x8048000 || (unsigned int)addr >= 0xc0000000) exit(-1);
}

/* bring argument from stack and increase stack pointer */
void get_argument (void *esp, int *arg, int count)
{
	int i;

	for (i = 0; i < count; i++)
	{
		check_address(esp);
		arg[i] = *(int*)(esp);
		esp += 4;
	}
}

/* end pintOS */
void halt(void)
{
	shutdown_power_off();
}

/* end current process */
void exit (int status)
{
	struct thread *current = thread_current();

	printf ("%s: exit(%d)\n", current -> name, status);
	current -> exit_status = status;

	thread_exit();
}

/* create new file */
bool create (const char *file, unsigned int initial_size)
{
	bool result;

	check_address((void*)file);

	/* result == true : create success
	result == false : create fail */
	result = filesys_create(file, initial_size);

	return result;
}

/* remove file */
bool remove (const char *file)
{
	bool result;

	/* result == true : remove success
	result == false : remove fail */
	result = filesys_remove(file);

	return result;
}

/* create child process and execute
if create fail, return -1 */
tid_t exec (char *cmd_line)
{
	tid_t child_tid = process_execute(cmd_line);
	struct thread *child_thread = get_child_process(child_tid);

	if (child_thread)
	{
		sema_down(&child_thread -> load_sema);
		if (child_thread -> is_embark_memory) return child_tid;
		else return -1;
	}
	else return -1;
}

/* return child process's exit status */
int wait (tid_t tid)
{
	int ret = process_wait(tid);

	return ret;
}

/* open file
if fail, return -1 */
int open (const char *file_name)
{
	struct file *file;
	int fd;

	if(file_name == NULL) exit(-1);

	lock_acquire(&filesys_lock);
	file = filesys_open(file_name);
	if (file == NULL)
	{
		lock_release(&filesys_lock);

		return -1;
	}

	fd = process_add_file(file);	//add file in file descripter table

	lock_release(&filesys_lock);

	return fd;
}

/* return filesize
if there is no file, return -1 */
int filesize (int fd)
{
	struct file *file;
	int size;

	file = process_get_file(fd);

	if (file == NULL) return -1;

	size = file_length(file);


	return size;
}

/* get file and read it
if fail, return -1 */
int read (int fd, void *buffer, unsigned size)
{
	struct file *file = process_get_file(fd);
	int ret;

	check_address(buffer);
	lock_acquire(&filesys_lock);

	/* fd == 0 : stdin */
	if (fd == 0)
	{
		unsigned int i;

		for (i = 0; i < size; i++) ((char*)buffer)[i] = input_getc();

		lock_release(&filesys_lock);

		return size;
	}

	if(!file)
	{
		lock_release(&filesys_lock);

		return -1;
	}

	ret = file_read(file, buffer, size);

	lock_release(&filesys_lock);

	return ret;
}

/* get file and write on it
if fail, return -1 */
int write (int fd, void *buffer, unsigned size)
{
	struct file *file = process_get_file(fd);
	int ret;

	check_address(buffer);
	lock_acquire(&filesys_lock);

	/* fd == 1 : stdout */
	if (fd == 1)
	{
		putbuf(buffer, size);
		
		lock_release(&filesys_lock);

		return size;
	}

	if(!file)
	{
		lock_release(&filesys_lock);

		return -1;
	}

	ret = file_write(file, buffer, size);

	lock_release(&filesys_lock);

	return ret;
}

/* get file and change position */
void seek (int fd, unsigned int position)
{
	struct file *file;

	file = process_get_file(fd);
	if (!file) return;

	file_seek(file, position);

}

/* return file's position
if fail, return -1 */
unsigned tell (int fd)
{
	struct file *file;
	off_t ret;

	file = process_get_file(fd);

	if (!file) return -1;

	ret = file_tell(file);

	return ret;
}

/* close file and free file descripter */
void close (int fd)
{
	process_close_file(fd);
}
