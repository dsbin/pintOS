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

static void syscall_handler (struct intr_frame *);

void check_address(void *addr);
void get_argument(unsigned int *esp, int *arg, int argc);

void halt (void);
void exit (int status);
bool create (const char *file, unsigned int initial_size);
bool remove (const char *file);
tid_t exec (char *process_name);
int wait (tid_t tid);
int write(int fd, void* buffer, unsigned size);
int read(int fd, void* buffer, unsigned size);
int file_size(int fd);
int open(const char* file);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);

void
syscall_init (void) 
{
  lock_init (&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  unsigned int *esp = (unsigned int*)(f->esp);  //stack pointer
  check_address(esp);
  int sys_n = *(int*)esp;  //store system call number
  int argument[4];
  
  esp++; //스택 값 증가
  check_address(esp);
  switch(sys_n)
  {
      //get_argument를 통해 각 함수에 필요한 인자의 갯수 리턴 받음
      case SYS_HALT:
          halt();
          break;
      case SYS_EXIT:
          {
            get_argument(esp, argument, 1);
            int status = argument[0];

            exit(status);
          }
          break;

      case SYS_EXEC:
          {
            get_argument(esp, argument, 1);

            char *exec_filename = (char*)argument[0];

            f->eax = exec(exec_filename);
          }
          break;

      case SYS_WAIT:
          {
            get_argument(esp, argument, 1);

            int tid = argument[0];
            
            f->eax = wait(tid);
          }
          break;

      case SYS_CREATE:
          {
            get_argument(esp, argument, 2);

            char *filename = (char*)argument[0];
            unsigned int initial_size = (unsigned int)argument[1];
            
            f->eax = create(filename, initial_size);
          }
          break;

      case SYS_REMOVE:
          {
            get_argument(esp, argument, 1);

            char *filename = (char*)argument[0];

            f->eax = remove(filename);
          }
          break;

      case SYS_OPEN:
          {
            get_argument(esp, argument, 1);
          
            char *filename = (char*)argument[0];

            f->eax = open(filename);
          }
          break;

      case SYS_FILESIZE:
          {
            get_argument(esp, argument, 1);

            int fd = argument[0];
          
            f->eax = file_size(fd);
          }
          break;

      case SYS_READ:
          {
            get_argument(esp, argument, 3);

            //printf("read!!\n");
            int fd = argument[0];
            void *buffer = (void*)argument[1];
            unsigned int size = (unsigned int)argument[2];

            f->eax = read(fd, buffer, size);
          }
          break;

      case SYS_WRITE:
          {
            get_argument(esp, argument, 3);

            int fd = argument[0];
            void *buffer = (void*)argument[1];
            unsigned int size = (unsigned int)argument[2];
            //printf("esp : %x, fd : %d, buffer : %x, size : %d\n", esp, fd, buffer, size);

            f->eax = write(fd, buffer, size);
          }
          break;

      case SYS_SEEK:
          {
            get_argument(esp, argument, 2);
            //printf("seek!!!\n");

            int fd = argument[0];
            unsigned int position = (unsigned int)argument[1];

            seek(fd, position);
          }
          break;

      case SYS_TELL:
          {
            get_argument(esp, argument, 1);
            //printf("tell!!!\n");

            int fd = argument[0];

            f->eax = tell(fd);
          }
          break;

      case SYS_CLOSE:
          {
            get_argument(esp, argument, 1);
            //printf("close!!!\n");

            int fd = argument[0];

            close(fd);
          }
          break;
  }

  //thread_exit ();
}

void
check_address (void *addr)
{ 
  //check address is in user address range
  if ((unsigned int)addr <= 0x8048000 || (unsigned int)addr >= 0xc0000000)
      exit(-1);
  //printf("checking address!!!!!!\n");
}

void
get_argument (unsigned int *esp, int *arg, int argc)
{
  int i;
  for (i = 0; i < argc; i++)
  {
    check_address((void*)esp);
    arg[i] = (int)*(esp);
    esp++;  //insert esp address to kernel stack
  }
}

void
halt(void)
{
  //shutdown system
  shutdown_power_off();
}

void
exit (int status)
{
  //exit thread
  struct thread *thread_cur = thread_current();  //현재 thread 를 받아옴
  printf ("%s: exit(%d)\n", thread_cur->name, status);  //종료상태 출력
  thread_cur->exit_status = status;  //종료상태 저장
  thread_exit();
}

bool
create (const char *file, unsigned int initial_size)
{
  check_address((void*)file);  //if argument is pointer

  lock_acquire(&filesys_lock);  //lock을 걸 어줌
  bool is_success = filesys_create(file, initial_size);  //create 성공 여부
  lock_release(&filesys_lock);  //lock을 풀어줌

  return is_success;
}

bool
remove (const char *file)
{
  check_address((void*)file);  //if argument is pointer
  
  lock_acquire(&filesys_lock);  //lock을 걸 어줌
  bool is_success = filesys_remove(file);  //remove성공여부
  lock_release(&filesys_lock);  //lock을 풀어줌

  return is_success;
}

tid_t
exec (char *process_name)
{
  tid_t exec_process_tid = process_execute(process_name);  //exec되는 process tid 
  struct thread *exec_process = get_child_process(exec_process_tid);

  if (exec_process)
    {
      sema_down(&exec_process->load_sema);
      
        if (exec_process->is_load)
        {
          return exec_process_tid;
        }
        else
        {
          return -1;
        }
    }
  else
  {
    return -1;
  }
}

int
wait (tid_t tid)
{
  return process_wait(tid);
}

int
open (const char *file_name)
{
  check_address((void*)file_name);

  lock_acquire(&filesys_lock);  //lock을 걸어줌
  struct file *open_file_name = filesys_open(file_name);  //open할 파일

  if (!open_file_name)
  {
    lock_release(&filesys_lock);  //lock을 풀어줌
    return -1;
  }

  int open_file_fd = process_add_file(open_file_name);
  lock_release(&filesys_lock);  //lock을 풀어줌

  return open_file_fd;
}

int 
file_size (int fd)
{
  lock_acquire(&filesys_lock);  //lock을 걸어줌
  struct file *check_file = process_get_file(fd);  //size를 확인할 파일
  if (!check_file)
  {
    lock_release(&filesys_lock);  // lock을 풀어줌
    return -1;
  }
  int file_size = file_length(check_file);
  lock_release(&filesys_lock);  //lock을 풀어줌

  return file_size;
}

int
read (int fd, void *buffer, unsigned size)
{
  check_address(buffer);
  lock_acquire(&filesys_lock);  //lock을 걸어줌

  if (fd == 0)  //stdin
  {
    unsigned int i;
    
    for (i = 0; i < size; i++)
    {
      ((char*)buffer)[i] = input_getc();
    }

    lock_release(&filesys_lock);  //lock을 풀어줌

    return size;
  }

  struct file *file_name = process_get_file(fd);
  if(!file_name)
  {
    lock_release(&filesys_lock);  //lock을 풀어줌
    return -1;  
  }
  
  int file_size = file_read(file_name, buffer, size);  //읽어올 파일의 크기
  lock_release(&filesys_lock);
  
  return file_size;
}

int
write (int fd, void *buffer, unsigned size)
{
    //printf("write before check\n");
  check_address(buffer);
  //printf("write after check\n");

  lock_acquire(&filesys_lock);  //lock을 걸어줌
  

  if (fd == 1)
  {
    putbuf(buffer, size);
    lock_release(&filesys_lock);  //lock을 풀어줌

    return size;
  }

  struct file *file_name = process_get_file(fd);
  if(!file_name)
  {
    lock_release(&filesys_lock);  //lock을 풀어줌
    return -1;
  }

  int file_size = file_write(file_name, buffer, size);
  lock_release(&filesys_lock);  //lock을 풀어줌 

  return file_size;
}

void
seek (int fd, unsigned int position)
{
  lock_acquire(&filesys_lock);  //lock을 걸어줌

  struct file *file_name = process_get_file(fd);
  if (!file_name)
  {
    lock_release(&filesys_lock);  //lock을 풀어줌
    return;
  }
  file_seek(file_name, (off_t)position);
  lock_release(&filesys_lock);  //lock을 풀어줌
}

unsigned
tell (int fd)
{
  lock_acquire(&filesys_lock);  //lock을 걸어줌

  struct file *file_name = process_get_file(fd);
  
  if (!file_name)
  {
    lock_release(&filesys_lock);  //lock을 풀어줌
    return -1;
  }

  off_t offset = file_tell(file_name);
  lock_release(&filesys_lock);  //lock을 풀어줌
  
  return offset; 
}

void
close (int fd)
{
  lock_acquire(&filesys_lock);  //lock을 걸어줌
  process_close_file(fd);
  lock_release(&filesys_lock);  //lock을 풀어줌
}
