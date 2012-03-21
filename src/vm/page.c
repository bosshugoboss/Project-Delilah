#include "vm/page.h"
#include <debug.h>
#include <stddef.h>
#include "devices/block.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "userprog/process.h"
#include "vm/frame.h"
#include "vm/swap.h"


/* Loads a page from the file system into memory */
void page_filesys_load (struct page *upage, void *kpage);


void
page_load (struct page *upage)
{
  /*TODO - Get kpage.*/
  void *kpage = palloc_get_page (PAL_USER);
  install_page (upage->uaddr, kpage, upage->write);

  /* Load the page into memory again.*/
  if (upage->saddr != -1)
    {
      swap_read_page (upage);
    }
  else
    PANIC ("WE DON KNOW");
    //page_filesys_load (upage, kpage);
}


void
page_create (struct frame *frame)
{
  /* Create the page struct */
  struct page *page = malloc (sizeof (struct page));
  page->saddr = -1;
  page->uaddr = frame->uaddr;
  page->write = frame->write;
 
  /* Write the page to swap or filesys */
  page_write (page, frame);

  /* Destroy the frame */
  uninstall_page (frame->addr);
  free (frame);
}

void
page_write (struct page *upage, struct frame *frame)
{ 
  struct hash_elem *e = hash_insert (&frame->owner->sup_page_table, &upage->hash_elem);
  //if (e == NULL) 
    //printf ("inserted addr: %p\n", upage->uaddr);
  //else
    //printf ("arse: %p", upage->uaddr);

  //if (upage->saddr != -1)
  //upage->saddr = swap_write_page (upage);
  //printf ("it worked");
  //else
    //PANIC ("AAAEFFFFH");
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
