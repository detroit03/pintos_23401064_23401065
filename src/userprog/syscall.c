#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"

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

/* Syscall handler kaydedilir. */
void syscall_init (void) 
{
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
