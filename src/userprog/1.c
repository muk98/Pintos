#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include <string.h>
#include "threads/synch.h"


static struct lock file_lock;

static void syscall_handler (struct intr_frame *);  /* called in the process of servicing the the requested systemcall */
static void check_valid_pointer (const void *);   /*checks if the given pointer is valid within the available physical memory in the frame*/
static void check_valid_string (const char*);
static void check_valid_space (const void *, size_t size);
static void close_file (int fd);
static int is_valid_fd (int fd);


static int (*syscalls []) (void *) =  /*returns the functiom to which the certain syscall_func is assigned */
  {
    syscall_halt,
    syscall_exit,
    syscall_exec,
    syscall_wait,
    syscall_create,
    syscall_remove,
    syscall_open,
    syscall_filesize,
    syscall_read,
    syscall_write,
    syscall_seek,
    syscall_tell,
    syscall_close,

    syscall_mmap,
    syscall_munmap,

    syscall_chdir,
    syscall_mkdir,
    syscall_readdir,
    syscall_isdir,
    syscall_inumber
  };


const int TOTAL_SYSCALLS = sizeof (syscalls) / sizeof (syscalls[0]);  /*This gives the total size of the array syscalls[] which has the entire list of different kind of syscall functions */

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall"); 
}

static void
syscall_handler (struct intr_frame *f UNUSED) /*This function is called when the any system call is made */ 
{
  void *esp = f->esp;               /*esp pointer in frame has the address of the  user process stack pointer*/
  check_valid_space (esp, sizeof(int));           /*checking the validiy of pointer provided by the user process*/
  int syscall_num = *((int *) esp);         /*deferencing to find out the index of the syscall made by typecasing the known pointer*/
  esp  += sizeof(int);                
  check_valid_space (esp, sizeof(int));               /*checking the validity of the new pointer value*/
 
  if(syscall_num >=0 && syscall_num < TOTAL_SYSCALLS){              /*checking if the call is in the list*/
    int (*syscall_function) (void *) = syscalls[syscall_num];         /*making the system call function */
    int return_value = syscall_function(esp);
    f->eax = return_value;                            /*register 'eax' in a frame stores the value retured after syscall is completed */
  }
  else{
    printf("\nError! System Call is not valid");    /*If the requested syscall function is not available in the list defined then that syscall is shown as invalid and that thread requesting for that particular systemcall is exited*/
    thread_exit();
  }
}


static void
check_valid_pointer (const void *ptr)                       /*checks if the given pointer is in the valid considerable range */
{
  uint32_t *pd = thread_current ()->pagedir;                    /*has page of the running thread*/
  if ( ptr == NULL || !is_user_vaddr (ptr) || pagedir_get_page (pd, ptr) == NULL)   /*exit if given pointer is not below the physical address or PD doesnot have corressponding kernal virtual addresss */
  {
    syscall_exit (NULL);
  }
}

static void
close_file (int fd)
{
   struct thread *t = thread_current ();

  if (t->files[fd] != NULL)
  {
    lock_acquire (&file_lock);
    file_close (t->files[fd]);
    t->files[fd] = NULL;
    lock_release (&file_lock);
  }
}


static void
check_valid_space (const void *ptr, size_t size)
{
  check_valid_pointer (ptr);
  if(size != 1)
    check_valid_pointer (ptr + size - 1);
}



static void
check_valid_string (const char *s)
{
  check_valid_space (s, sizeof(char));
  while (*s != '\0')
    check_valid_space (s++, sizeof(char));
}


static int
is_valid_fd (int fd)
{
  return fd >= 0 && fd < MAX_FILES; 
}





static int
syscall_halt (void *esp UNUSED )             /*function for the systemcall 'halt'*/
{
  power_off();
}


int
syscall_exit (void *esp)                /*This function when called prints the name and status of the present thread in the console and exits from it*/
{
  int status = 0;
  if (esp != NULL){                   /*checking if the given pointer is nonempty and then finding its status accordingly*/
    check_valid_space (esp, sizeof(int));
    status = *((int *)esp);
    esp += sizeof (int);
  }
  else {
    status = -1;
  }

  struct thread *t = thread_current ();

  int i;
  for (i = 2; i<MAX_FILES; i++)
  {
    if (t->files[i] != NULL){
      close_file (i);
    }
  }
  
  char *name = thread_current ()->name, *save;      /*extracting the name of the present thread*/
  name = strtok_r (name, " ", &save);           /*this function splits the name when " " occurs */
  
  printf ("%s: exit(%d)\n", name, status);        /*printing on console*/
  thread_exit ();                   /*calling for thread exit*/
}


static int
syscall_exec (void *esp )
{
  check_valid_space (esp, sizeof (char *));
  const char *file_name = *((char **) esp);
  esp += sizeof (char *);
  check_valid_string (file_name);
  thread_exit ();
}

static int
syscall_wait (void *esp )
{
  check_valid_space (esp, sizeof (int));
  int pid = *((int *) esp);
  esp += sizeof (int);

  
  if (pid >=0 && pid < 128)
  {
    //TODO:: implement wait
    // return pid;
  }
  else
    syscall_exit(NULL);

  thread_exit ();
}

static int
syscall_create (void *esp )
{
  check_valid_space (esp, sizeof(char *));
  const char *file_name = *((char **) esp);
  esp += sizeof (char *);

  check_valid_string (file_name);

  check_valid_space (esp, sizeof(unsigned));
  unsigned initial_size = *((unsigned *) esp);
  esp += sizeof (unsigned);

  lock_acquire (&file_lock);
  int status = filesys_create (file_name, initial_size);
  lock_release (&file_lock);

  return status;

}

static int
syscall_remove (void *esp )
{
  check_valid_space (esp, sizeof(char *));
  const char *file_name = *((char **) esp);
  esp += sizeof (char *);

  check_valid_string (file_name);

  lock_acquire (&file_lock);
  int status = filesys_remove (file_name);
  lock_release (&file_lock);

  return status;
}

static int
syscall_open (void *esp )
{

  check_valid_space (esp, sizeof(char *));
  const char *file_name = *((char **) esp);
  esp += sizeof (char *);

  check_valid_string (file_name);
  
  lock_acquire (&file_lock);
  struct file *f = filesys_open (file_name);
  lock_release (&file_lock);

  if (f == NULL)
    return -1;
  
  struct thread *t = thread_current ();

  int i;
  // int flag =0;
  for (i = 2; i<MAX_FILES; i++)
  {
    if (t->files[i] == NULL){
      t->files[i] = f;
      break;
    }
  }

  if (i == MAX_FILES)
    return -1;
  else
    return i;
}


static int
syscall_filesize (void *esp )
{
  check_valid_space (esp, sizeof(int));
  int fd = *((int *) esp);
  esp += sizeof (int);

  struct thread *t = thread_current ();
  if (is_valid_fd (fd) && t->files[fd] != NULL)
  {  
    lock_acquire (&file_lock);
    int size = file_length (t->files[fd]);
    lock_release (&file_lock);

    return size;
  }
  return -1;
}


static int
syscall_read (void *esp )
{
  check_valid_space (esp, sizeof(int));
  int fd = *((int *)esp);
  esp += sizeof (int);

  check_valid_space (esp, sizeof(void *));
  const void *buffer = *((void **) esp);
  esp += sizeof (void *);

  check_valid_space (esp, sizeof(unsigned));
  unsigned size = *((unsigned *) esp);
  esp += sizeof (unsigned);
  
  check_valid_space (buffer, size);

  struct thread *t = thread_current ();
  if (fd == STDIN_FILENO)
  {
    lock_acquire (&file_lock);

    int i;
    for (i = 0; i<size; i++)
      *((uint8_t *) buffer+i) = input_getc ();

    lock_release (&file_lock);
    return i;
  }
  else if (is_valid_fd (fd) && fd >=2 && t->files[fd] != NULL)
  {
    lock_acquire (&file_lock);
    int read = file_read (t->files[fd], buffer, size);
    lock_release (&file_lock);
    return read;
  }
  return 0;
}



static int
syscall_write (void *esp)             /*This function is for writing from the buffer */
{
  check_valid_space (esp, sizeof(int));
  int fd = *((int *)esp);
  esp += sizeof (int);

  check_valid_space (esp, sizeof(void *));
  const void *buffer = *((void **) esp);
  esp += sizeof (void *);

  check_valid_space (esp, sizeof(unsigned));
  unsigned size = *((unsigned *) esp);
  esp += sizeof (unsigned);
  
  check_valid_space (buffer, size);
  
  struct thread *t = thread_current ();

  if (fd == STDOUT_FILENO)
  {
    /* putbuf (buffer, size); */
    lock_acquire (&file_lock);

    int i;
    for (i = 0; i<size; i++)
      putchar (*((char *) buffer + i));

    lock_release (&file_lock);
    return i;
  }
  else if (is_valid_fd (fd) && fd >=2 && t->files[fd] != NULL)
  {
    lock_acquire (&file_lock);
    int written = file_write (t->files[fd], buffer, size);
    lock_release (&file_lock);
    return written;
  }
   return 0;
}



static int
syscall_seek (void *esp )
{
  check_valid_space (esp, sizeof(int));
  int fd = *((int *)esp);
  esp += sizeof (int);

  check_valid_space (esp, sizeof(unsigned));
  unsigned position = *((unsigned *) esp);
  esp += sizeof (unsigned);

  struct thread *t = thread_current ();

  if (is_valid_fd (fd) && t->files[fd] != NULL)
  {
    lock_acquire (&file_lock);
    file_seek (t->files[fd], position);
    lock_release (&file_lock);
  }
}


static int
syscall_tell (void *esp )
{
  check_valid_space (esp, sizeof(int));
  int fd = *((int *)esp);
  esp += sizeof (int);

  struct thread *t = thread_current ();

  if (is_valid_fd (fd) && t->files[fd] != NULL)
  {
    lock_acquire (&file_lock);
    int position = file_tell (t->files[fd]);
    lock_release (&file_lock);
    return position;
  }
  return -1;
}


static int
syscall_close (void *esp )
{
  
  check_valid_space (esp, sizeof(int));
  int fd = *((int *) esp);
  esp += sizeof (int);

  if (is_valid_fd (fd))
    close_file (fd);


}



static int
syscall_mmap (void *esp UNUSED)
{
  thread_exit ();
}

static int
syscall_munmap (void *esp UNUSED)
{
  thread_exit ();
}

static int
syscall_chdir (void *esp UNUSED)
{
  thread_exit ();
}

static int
syscall_mkdir (void *esp UNUSED)
{
  thread_exit ();
}

static int
syscall_readdir (void *esp UNUSED)
{
  thread_exit ();
}

static int
syscall_isdir (void *esp UNUSED)
{
  thread_exit ();
}

static int
syscall_inumber (void *esp UNUSED)
{
  thread_exit ();
}


