#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include <string.h>
#include "threads/synch.h"
#include "threads/thread.h"
#include "userprog/process.h"

/* Lock for file system calls. */
static struct lock file_lock;

static void syscall_handler (struct intr_frame *);
static void check_valid_pointer (const void *);
static void check_valid_space (const void *, size_t);
static void check_valid_string (const char *);
static void close_file (int);
static int is_valid_fd (int);




/*returns the system call (system call number as defined in lib/syscall-nr.h) */
static int (*syscalls []) (void *) =
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


/*This gives the total number of the system calls */
const int num_calls = sizeof (syscalls) / sizeof (syscalls[0]);

/*Initializing the system call and system call handler*/
void
syscall_init (void) 
{
  lock_init (&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall"); 
}


/*System call handler to service the system calls*/ 
static void
syscall_handler (struct intr_frame *f ) 
{

  /*esp pointer in frame has the address of the top of the stac*/
  void *esp = f->esp;

  /*checking the validity of the stack pointer provided by the user program*/
  check_valid_space (esp, sizeof(int));
  
  /*deferencing to find out the index of the syscall made by typecasing the known pointer*/
  int syscall_num = *((int *) esp);
  esp += sizeof(int);
  check_valid_space (esp, sizeof(void *));


  /*checking if the call is in the list*/
  if (syscall_num >= 0 && syscall_num < num_calls)
  {

    /*making the system call function */
    int (*function) (void *) = syscalls[syscall_num];

    /*calling the above refrenced system call function*/
    int ret = function (esp);

    /*register 'eax' in a frame stores the value retured after syscall is requested from that frame */
    f->eax = ret;
  }
  else
  {
    printf ("\nError, invalid syscall number.");
    thread_exit ();
  }
}

/* 
  Closes the file numbered 'fd' if it exists
  and sets the pointer of the thread to it to be equal to NULL
*/
static void
close_file (int fd)
{
  struct thread *t = thread_current ();
  if (t->files[fd] != NULL)
  {
    lock_acquire (&file_lock);

    /*function to close the file*/
    file_close (t->files[fd]);
    t->files[fd] = NULL;
    lock_release (&file_lock);
  }
}

/* 
  Checks whether 'fd' is a valid file no. or not
*/
static int
is_valid_fd (int fd)
{
  return fd >= 0 && fd < MAX_FILES; 
}

/* 
  Checks whether the character string elements occupy a valid space or not
*/
static void
check_valid_string (const char *s)
{
  check_valid_space (s, sizeof(char));
  while (*s != '\0')
    check_valid_space (s++, sizeof(char));
}

/* 
  Checks whether the pointer occupy a valid space or not
*/
static void
check_valid_space (const void *ptr, size_t size)
{
  check_valid_pointer (ptr);
  if(size != 1)
    check_valid_pointer (ptr + size - 1);
}

/* 
  Checks whether the given pointer (user's virtual address) is mapped to a valid physical address 
  and is not NULL and is less than the PHYSICAL BASE address
*/
static void
check_valid_pointer (const void *ptr)
{
  uint32_t *pd = thread_current ()->pagedir;
  if ( ptr == NULL || !is_user_vaddr (ptr) || pagedir_get_page (pd, ptr) == NULL)
  {
    syscall_exit (NULL);
  }

}





/*function for the systemcall 'halt'*/
static int
syscall_halt (void *esp UNUSED)
{

  /*turn of the system*/
  power_off ();
}


/*This function when called prints the name of the present thread in the console, free all the open files and exits.*/
int
syscall_exit (void *esp)
{
  int status = 0;
  if (esp != NULL){

    /*checking if the given pointer is nonempty and then finding its status accordingly*/

    check_valid_space (esp, sizeof(int));
    status = *((int *)esp);
    esp += sizeof (int);
  }
  else {
    status = -1;
  }

  struct thread *t = thread_current ();

  
  /*closing the opened files by the thread.*/
  int i;
  for (i = 2; i<MAX_FILES; i++)
  {
    if (t->files[i] != NULL){
      close_file (i);
    }
  }
  
  /*extracting the name of the present thread*/
  char *name = t->name, *save;
  name = strtok_r (name, " ", &save);
  
  printf ("%s: exit(%d)\n", name, status);

  array3[t->tid] = status;
  enum intr_level old_level = intr_disable ();

  /*do the sema up to show that thread has exited*/
  sema_up(&t->sema_exit);
  intr_set_level (old_level);

  /*do sema down to check whether wait has been called or not for this thread*/
  sema_down(&t->sema_ack);
  
  thread_exit ();

}

/* Runs the executable and Checks whether the filename is a valid string or not
*/
static int
syscall_exec (void *esp)
{
  check_valid_space (esp, sizeof (char *));

  /*Get the poniter to the file*/
  const char *file_name = *((char **) esp);
  esp += sizeof (char *);

  check_valid_string (file_name);

  tid_t tid;

  lock_acquire(&file_lock);

  /*execute the child*/
  tid = process_execute(file_name);

  lock_release(&file_lock);

  
  /*get the child's thread pointer*/
  struct thread *child = array1[tid];


  /*if child is not valid*/
  if(child==NULL){
    return -1;
  }

  /*call the sema down to check whether the loading of the child has already done or not*/
  sema_down(&child->thread_load_done);

  /*check whether the child is properly loaded or not, if not return -1 */
  if(array2[tid]==0) tid=-1;

  
  /*if child is properly loaded return the tid*/
  return tid;
  
  
}


/* 
  Waits for a child process pid and retrieves the child’s exit status.
*/
static int
syscall_wait (void *esp)        {
  check_valid_space (esp, sizeof (int));  

  /*This pointer represents child whose status has to be returned */
  int pid = *((int *) esp);       
  esp += sizeof (int);  

  /* This case occurs when the obtained pid is invalid(since there is a limit on number of pids) and is thus reported by returning '-1'*/
  if(pid<0 || pid>2040) return -1;    
  
  /*Obtaining child since array1 maps the pointer of the child with its pid*/
  struct thread *child = array1[pid]; 

  
  /* If the child called is NULL ... which clearly is an error we return the status of this call as '-1' */
  if(child==NULL)   
    return -1;

  int status;
  
  
  /*Since this function can be called several times by the same parent, this condition is to check if there has been a returned value already*/
  if(array3[pid]==-1) status = -1;    
  else{

    /*This is for waiting till the child has exited*/
    sema_down(&child->sema_exit); 

    /*Exit status of the child is stored in array3 indexed by its respective pid after its completion*/
    status = array3[pid];     
    
    /* This indicates that this call has returned */
    array3[pid]= -1;        
    
  }

  /*Do the sema up showing that child is executed and status is not returned*/
  sema_up(&child->sema_ack);
  
  return status;    
 
}




/*Create a file and the returns the status whether the file is created or not*/
static int
syscall_create (void *esp)
{
  check_valid_space (esp, sizeof(char *));

  /*popping out the file name to be create from the stack*/
  const char *file_name = *((char **) esp);
  esp += sizeof (char *);


  /*Checking the string provided is in valid user space or not*/
  check_valid_string (file_name);

  check_valid_space (esp, sizeof(unsigned));

  /*popping out the size of the file*/
  unsigned initial_size = *((unsigned *) esp);
  esp += sizeof (unsigned);

  lock_acquire (&file_lock);

  /*creating the file*/
  int status = filesys_create (file_name, initial_size);
  lock_release (&file_lock);

  return status;
}


/*system call to remove the file and returns the status whether the file is removed or not*/
static int
syscall_remove (void *esp)
{

  check_valid_space (esp, sizeof(char *));

  /*popping out the file name to be removed from the stack*/
  const char *file_name = *((char **) esp);
  esp += sizeof (char *);

  /*Checking the string provided is in valid user space or not*/
  check_valid_string (file_name);

  lock_acquire (&file_lock);

  /*removing the file*/
  int status = filesys_remove (file_name);
  lock_release (&file_lock);

  return status;
}

/*system call to open the file and return the file descriptor of the file*/
static int
syscall_open (void *esp)
{
  check_valid_space (esp, sizeof(char *));

  /*popping out the file name to be opened from the stack*/
  const char *file_name = *((char **) esp);
  esp += sizeof (char *);

  /*Checking the string provided is in valid user space or not*/
  check_valid_string (file_name);
  
  lock_acquire (&file_lock);

  /*opening the file*/
  struct file *f = filesys_open (file_name);
  lock_release (&file_lock);


  if (f == NULL)
    return -1;
  
  struct thread *t = thread_current ();

  /*adding the file to the list of the open file of the thread*/
  int i;
  for (i = 2; i<MAX_FILES; i++)
  {
    if (t->files[i] == NULL){
      t->files[i] = f;
      break;
    }
  }

  /*return the fd of the file , -1 if not added successfully*/
  if (i == MAX_FILES)
    return -1;
  else
    return i;
}


/*system call to return the size of the file*/
static int
syscall_filesize (void *esp)
{
  check_valid_space (esp, sizeof(int));

  /*popping out the fd of the file*/
  int fd = *((int *) esp);
  esp += sizeof (int);

  struct thread *t = thread_current ();

  /*Chekcing the validity of the provided file descriptor*/
  if (is_valid_fd (fd) && t->files[fd] != NULL)
  {  
    lock_acquire (&file_lock);

    /*getting the size of the file*/
    int size = file_length (t->files[fd]);
    lock_release (&file_lock);

    return size;
  }
  return -1;
}

/*system call to read the data from the file or from STDOUT*/
static int
syscall_read (void *esp)
{
  check_valid_space (esp, sizeof(int));

  /*popping out the fd of the file*/
  int fd = *((int *)esp);
  esp += sizeof (int);

  check_valid_space (esp, sizeof(void *));

  /*Popping the pointer of the buffer where to store the read data*/
  const void *buffer = *((void **) esp);
  esp += sizeof (void *);

  check_valid_space (esp, sizeof(unsigned));

  /*popping the size to be read*/
  unsigned size = *((unsigned *) esp);
  esp += sizeof (unsigned);
  
  check_valid_space (buffer, size);

  struct thread *t = thread_current ();

  /* if fd==0, read from the STANDARD OUTPUT i.e. terminal or console*/
  if (fd == STDIN_FILENO)
  {
    lock_acquire (&file_lock);

    int i;
    for (i = 0; i<size; i++)

    /*get the inut from terminal*/
      *((uint8_t *) buffer+i) = input_getc ();

    lock_release (&file_lock);
    return i;
  }

  /*else read from the file*/
  else if (is_valid_fd (fd) && fd >=2 && t->files[fd] != NULL)
  {
    lock_acquire (&file_lock);

    int read = file_read (t->files[fd], buffer, size);
    lock_release (&file_lock);
    return read;
  }
  return 0;
}

/*
Writes 'size' bytes from 'buffer' to the open file 'fd'. 
Returns the number of bytes actually written
*/
static int
syscall_write (void *esp)
{
  check_valid_space (esp, sizeof(int));

  /*popping out the fd of the file*/
  int fd = *((int *)esp);
  esp += sizeof (int);

  check_valid_space (esp, sizeof(void *));

  /*popping the pointer to the buffer from where data is to be written*/
  const void *buffer = *((void **) esp);
  esp += sizeof (void *);

  check_valid_space (esp, sizeof(unsigned));

  /*size of the buffer*/
  unsigned size = *((unsigned *) esp);
  esp += sizeof (unsigned);
  
  check_valid_space (buffer, size);
  
  struct thread *t = thread_current ();

  /* if fd==1, write to the STANDARD INPUT*/
  if (fd == STDOUT_FILENO)
  {
        lock_acquire (&file_lock);

    int i;
    for (i = 0; i<size; i++)

    /*write to the console*/
      putchar (*((char *) buffer + i));

    lock_release (&file_lock);
    return i;
  }

  /*else write to the file*/
  else if (is_valid_fd (fd) && fd >=2 && t->files[fd] != NULL)
  {
    lock_acquire (&file_lock);
    int written = file_write (t->files[fd], buffer, size);
    lock_release (&file_lock);
    return written;
  }
  return 0;
}

/*Changes the next byte to be read or written in an opened file numbered 'fd' 
  to the value of 'position'
*/
static int
syscall_seek (void *esp)
{
  check_valid_space (esp, sizeof(int));

  /*popping out the fd of the file*/
  int fd = *((int *)esp);
  esp += sizeof (int);

  check_valid_space (esp, sizeof(unsigned));

  /* gets value of position from stack */
  unsigned position = *((unsigned *) esp);
  esp += sizeof (unsigned);

  struct thread *t = thread_current ();

  if (is_valid_fd (fd) && t->files[fd] != NULL)
  {
    lock_acquire (&file_lock);

    /*relocate to the new positon*/
    file_seek (t->files[fd], position);
    lock_release (&file_lock);
  }
}

/*
  Returns the position of the next byte to be read or written in opened file fd,
  from the beginning of the file
*/
static int
syscall_tell (void *esp)
{
  check_valid_space (esp, sizeof(int));
  
  /*popping out the fd of the file*/
  int fd = *((int *)esp);
  esp += sizeof (int);

  struct thread *t = thread_current ();

  if (is_valid_fd (fd) && t->files[fd] != NULL)
  {
    lock_acquire (&file_lock);

    /*getting the position of the next byte to be read*/
    int position = file_tell (t->files[fd]);
    lock_release (&file_lock);
    return position;
  }
  return -1;
}

/*Closes the file numbered fd*/
static int
syscall_close (void *esp)
{
  check_valid_space (esp, sizeof(int));
  
  /*popping out the fd of the file*/
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

