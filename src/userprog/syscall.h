#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
// int syscall_exit (void *);


static int syscall_halt (void *);
int syscall_exit (void *);
static int syscall_exec (void *);
static int syscall_wait (void *);
static int syscall_create (void *);
static int syscall_remove (void *);
static int syscall_open (void *);
static int syscall_filesize (void *);
static int syscall_read (void *);
static int syscall_write (void *);
static int syscall_seek (void *);
static int syscall_tell (void *);
static int syscall_close (void *);
static int syscall_mmap (void *);
static int syscall_munmap (void *);
static int syscall_chdir (void *);
static int syscall_mkdir (void *);
static int syscall_readdir (void *);
static int syscall_isdir (void *);
static int syscall_inumber (void *);

#endif /* userprog/syscall.h */