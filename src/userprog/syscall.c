#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/input.h"
#include "threads/synch.h"
#include "userprog/process.h"

static void syscall_handler (struct intr_frame *);

static void exit_process (int status);
static void validate_pointer (const void *pointer);
static void validate_buffer (const void *buffer, unsigned size);
static void validate_string (const char *string);

static int get_argument (struct intr_frame *frame, int index);
static void *get_pointer_argument (struct intr_frame *frame, int index);
static unsigned get_unsigned_argument (struct intr_frame *frame, int index);

static void sys_halt (void);
static void sys_exit (int status);
static int sys_write (int file_descriptor, const void *buffer, unsigned size);
static int sys_open (const char *file);
static void sys_close (int fd);
static bool sys_create (const char *file, unsigned initial_size);
static bool sys_remove (const char *file);
static int sys_read (int fd, void *buffer, unsigned size);
static int sys_filesize (int fd);
static void sys_seek (int fd, unsigned p);
static unsigned sys_tell (int fd);

struct lock filesys_lock;
/* Syscall handler kaydedilir. */
void syscall_init (void) 
{
  lock_init (&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Numaraya gore siradaki islem cagrilir. */
static void syscall_handler (struct intr_frame *frame) 
{
  int syscall_number = get_argument (frame, 0);

  switch (syscall_number)
    {
      case SYS_HALT:
        sys_halt ();
        break;

      case SYS_EXIT:
        {
          int status = get_argument (frame, 1);
          sys_exit (status);
          break;
        }

      case SYS_WRITE:
        {
          int file_descriptor = get_argument (frame, 1);
          const void *buffer = get_pointer_argument (frame, 2);
          unsigned size = get_unsigned_argument (frame, 3);

          frame->eax = sys_write (file_descriptor, buffer, size);
          break;
        }

      case SYS_OPEN:
      {
        frame->eax = sys_open ((const char *) get_pointer_argument (frame, 1));
        break;
      }

      case SYS_CLOSE:
      {
        sys_close (get_argument (frame, 1));
        break;
      }

      case SYS_CREATE:
      {
        frame->eax = sys_create ((const char *) get_pointer_argument (frame, 1),
                                  get_unsigned_argument (frame, 2));
        break;
      }

      case SYS_REMOVE:
      {
        frame->eax = sys_remove ((const char *) get_pointer_argument (frame, 1));
        break;
      }

      case SYS_READ:
      {
        frame->eax = sys_read (get_argument (frame, 1),
                               get_pointer_argument (frame, 2),
                               get_unsigned_argument (frame, 3));
        break;
      }

      case SYS_FILESIZE:
      {
        frame->eax = sys_filesize (get_argument (frame, 1));
        break;
      }

      case SYS_SEEK:
      {
        sys_seek (get_argument (frame, 1), get_unsigned_argument (frame, 2));
        break;
      }

      case SYS_TELL:
      {
        frame->eax = sys_tell (get_argument (frame, 1));
        break;
      } 

      case SYS_EXEC:
      {
        const char *cmd_line = (const char *) get_pointer_argument (frame, 1);
        validate_string (cmd_line);
        frame->eax = (uint32_t) process_execute (cmd_line);
        break;
      }

      case SYS_WAIT:
      {
        tid_t child_pid = (tid_t) get_argument (frame, 1);
        frame->eax = (uint32_t) process_wait (child_pid);
        break;
      }

      default:
        sys_exit (-1);
    }
}

/* Sistem tamamen kapatilir. */
static void sys_halt (void)
{
  shutdown_power_off ();
}

/* Surecin exit kodu ayarlanir. */
static void sys_exit (int status)
{
  exit_process (status);
}

/* file descriptor degeri eger 1 ise konsola yazilir. */
static int sys_write (int file_descriptor, const void *buffer, unsigned size)
{
  validate_buffer (buffer, size);

  if (file_descriptor == 1)
    {
      putbuf (buffer, size);
      return size;
    }

  return -1;
}

/* Surecin exit kodu ayarlanir ve thread_exit fonksiyonu cagrilir. */
static void exit_process (int status)
{
  thread_current ()->exit_code = status;
  thread_exit ();
}

/* Kullanici tarafindan gelen pointerin durumu kontrol edilir. */
static void validate_pointer (const void *pointer)
{
  if (pointer == NULL || !is_user_vaddr (pointer))
    exit_process (-1);

  if (pagedir_get_page (thread_current ()->pagedir, pointer) == NULL)
    exit_process (-1);
}

/* Buffer'in icinde yer alan her byte teker teker kontrol edilir. */
static void validate_buffer (const void *buffer, unsigned size)
{
  const char *start = (const char *) buffer;
  unsigned offset;

  for (offset = 0; offset < size; offset++)
    validate_pointer (start + offset);
}

/* Stringin son gecersiz byte'ina kadar tum byte'lar kontrol edilir. */
static void validate_string (const char *string)
{
  validate_pointer (string);
  while (*string != '\0')
    {
      string++;
      validate_pointer (string);
    }
}

/* Stack tarafindan gelen indexin argumenti okunur. */
static int get_argument (struct intr_frame *frame, int index)
{
  int *argument_pointer = (int *) frame->esp + index;

  validate_pointer (argument_pointer);
  validate_pointer ((char *) argument_pointer + 3);
  return *argument_pointer;
}

/* Stack tarafinda pointer olarak yer alan argument okunur. */
static void *get_pointer_argument (struct intr_frame *frame, int index)
{
  return (void *) get_argument (frame, index);
}

/* Stack tarafinda yer alan unsigned tipindeki argument okunur. */
static unsigned get_unsigned_argument (struct intr_frame *frame, int index)
{
  return (unsigned) get_argument (frame, index);
}

static int sys_open (const char *file)
{
  validate_string (file);
  lock_acquire (&filesys_lock);
  struct file *f = filesys_open (file);
  lock_release (&filesys_lock);
  if (f == NULL)
    return -1;
  struct thread *t = thread_current ();
  int fd;
  for (fd = 2; fd < 128; fd++)
    {
      if (t->fd_table[fd] == NULL)
        {
          t->fd_table[fd] = f;
          return fd;
        }
    }
  file_close (f);
  return -1;
} 

static void sys_close (int fd)
{
  struct thread *t = thread_current ();
  if (fd < 2 || fd >= 128 || t->fd_table[fd] == NULL)
    return;
  lock_acquire (&filesys_lock);
  file_close (t->fd_table[fd]);
  lock_release (&filesys_lock);
  t->fd_table[fd] = NULL;
}

static bool sys_create (const char *file, unsigned initial_size)
{
  validate_string (file);
  lock_acquire (&filesys_lock);
  bool ret = filesys_create (file, initial_size);
  lock_release (&filesys_lock);
  return ret;
}

static bool sys_remove (const char *file)
{
  validate_string (file);
  lock_acquire (&filesys_lock);
  bool ret = filesys_remove (file);
  lock_release (&filesys_lock);
  return ret;
}

static int sys_read (int fd, void *buffer, unsigned size)
{
  validate_buffer (buffer, size);
  if (fd == 0)
    {
      unsigned i;
      uint8_t *b = buffer;
      for (i = 0; i < size; i++)
        b[i] = input_getc ();
      return size;
    }
  struct thread *t = thread_current ();
  if (fd < 2 || fd >= 128 || t->fd_table[fd] == NULL)
    return -1;
  lock_acquire (&filesys_lock);
  int ret = file_read (t->fd_table[fd], buffer, size);
  lock_release (&filesys_lock);
  return ret;
}

static int sys_filesize (int fd)
{
  struct thread *t = thread_current ();
  if (fd < 2 || fd >= 128 || t->fd_table[fd] == NULL)
    return -1;
  lock_acquire (&filesys_lock);
  int ret = file_length (t->fd_table[fd]);
  lock_release (&filesys_lock);
  return ret;
}

static void sys_seek (int fd, unsigned p)
{
  struct thread *t = thread_current ();
  if (fd < 2 || fd >= 128 || t->fd_table[fd] == NULL)
    return;
  lock_acquire (&filesys_lock);
  file_seek (t->fd_table[fd], p);
  lock_release (&filesys_lock);
}

static unsigned sys_tell (int fd)
{
  struct thread *t = thread_current ();
  if (fd < 2 || fd >= 128 || t->fd_table[fd] == NULL)
    return -1;
  lock_acquire (&filesys_lock);
  unsigned ret = file_tell (t->fd_table[fd]);
  lock_release (&filesys_lock);
  return ret;
}