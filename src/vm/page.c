#include "vm/page.h"
#include <debug.h>
#include <stddef.h>
//TODO - remove stdio
#include <stdio.h>
#include <string.h>
#include "devices/block.h"
#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "vm/frame.h"
#include "vm/swap.h"


/* Loads a page from the file system into memory */
void page_filesys_load (struct page *upage, void *kpage);

/* Tries to load a page that would be at FAULT_ADDR from
   a memory-mapped file. */
static bool page_load_from_mapped_file (struct page *upage, void *fault_addr);

bool
page_load (struct page *upage, void *fault_addr)
{
  /* Load the page into memory again.*/
  if (upage->saddr != -1)
    {
      /* Load from swap. */
      void *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        {
          frame_evict ();
          kpage = palloc_get_page (PAL_USER);
        }
      install_page (upage->uaddr, kpage, upage->write);
      swap_read_page (upage);
    }
  else
    {
      /* Load from a file. */
      if (upage->file == NULL)
        {
          /* Memory-mapped file. */
          return page_load_from_mapped_file (upage, fault_addr);
        }

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      while (kpage == NULL)
        {
          frame_evict ();
          kpage = palloc_get_page (PAL_USER);
        }

      /* Load this page. */
      if (file_read_at (upage->file, kpage, upage->file_read_bytes,
              upage->file_start_pos)
            != (int) upage->file_read_bytes)
        {
          palloc_free_page (kpage);
          printf ("file not read properly (page.c:58)\n");
          thread_exit ();
        }
      memset (kpage + upage->file_read_bytes, 0, PGSIZE - upage->file_read_bytes);
      
      /* Add the page to the process's address space. */
      if (!install_page (upage->uaddr, kpage, upage->write)) 
        {
          palloc_free_page (kpage);
          printf ("page not installed properly (page.c:67)\n");
          thread_exit ();
        } 
    }
  return true;
}

static bool
page_load_from_mapped_file (struct page *upage, void *fault_addr)
{
  //TODO - renaming things
  void *orig_fault_addr = fault_addr;
  //printf ("MF: %p\n", fault_addr);

  struct mapped_file *mapped_file = thread_get_mapped_file (orig_fault_addr);
  if (mapped_file == NULL)
  {
    printf ("no mapped file from %p (page.c:84)\n", orig_fault_addr);
    return false;
  }

  void *in_file_addr = (void *) (orig_fault_addr - mapped_file->addr);
  uint8_t *buffer = palloc_get_page (PAL_USER);
  if (buffer == NULL)
    {
      printf ("palloc didn't palloc (page.c:92)\n");
      return false;
    }
  int bytes_read = file_read_at (mapped_file->file, buffer, PGSIZE,
                                 (int) pg_round_down (in_file_addr));

  memset (buffer + bytes_read, 0, PGSIZE - bytes_read);

  /* Add the page to the process's address space. */
  if (!install_page (upage->uaddr, buffer, upage->write)) 
    {
      palloc_free_page (buffer);
      printf ("page not installed correctly (page.c:104)\n");
      return false;
    }

  return true;
}

void
page_write_to_mapped_file (struct file *file, void *addr, int file_size)
{
  int i;
  for (i = 0; i < file_size; i += PGSIZE)
    {
      if (pagedir_is_dirty (thread_current ()->pagedir, addr + i))
        file_write_at (file, addr + i, PGSIZE, i);
    }
}

void
page_create (struct frame *frame)
{
  struct page *upage = page_lookup (&frame->owner->sup_page_table, frame->uaddr);
  if (upage->write)
    upage->saddr = swap_write_page (upage);
  uninstall_page (frame->addr);
  palloc_free_page (frame->addr);
  free (frame);
}

void
page_filesys_load (struct page *upage UNUSED, void *kpage UNUSED)
{
  //TODO - page_filesys_load
}

struct page *
page_lookup (struct hash *page_table, void *uaddr)
{
  struct page p;
  struct hash_elem *e;

  p.uaddr = uaddr;
  e = hash_find (page_table, &p.hash_elem);
  return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}

unsigned
page_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
  const struct page *p = hash_entry (e, struct page, hash_elem);
  return hash_bytes (&p->uaddr, sizeof &p->uaddr);
}

bool
page_less_func (const struct hash_elem *a, const struct hash_elem *b,
                void *aux UNUSED)
{
  const struct page *pa = hash_entry (a, struct page, hash_elem);
  const struct page *pb = hash_entry (b, struct page, hash_elem);
  return pa->uaddr < pb->uaddr;
}
