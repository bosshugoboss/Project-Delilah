#include "vm/frame.h"
#include <random.h>
#include <debug.h>
#include <stdio.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/page.h"

static struct hash frame_table; /* Frame table*/

static struct lock eviction_lock;

void
frame_init (void)
{
  hash_init (&frame_table, &frame_hash_func, &frame_less_func, NULL);
  lock_init (&eviction_lock);
}

struct frame *
frame_lookup (void *addr)
{
  struct frame f;
  struct hash_elem *e;

  f.addr = addr;
  e = hash_find (&frame_table, &f.hash_elem);
  return e != NULL ? hash_entry (e, struct frame, hash_elem) : NULL;
}

void
frame_insert (void *faddr, void *uaddr, bool write)
{
  struct frame *f = malloc (sizeof (struct frame));
  f->addr = faddr;
  f->uaddr = uaddr;
  f->write = write;
  f->owner = thread_current ();
  
  hash_insert (&frame_table, &f->hash_elem);
}

struct frame *
frame_remove (void *kpage)
{
  struct frame *removing = frame_lookup (kpage);
  hash_delete (&frame_table, &removing->hash_elem);
  return removing;
}

void
frame_remove_by_upage (void *upage)
{
  struct frame *f = frame_find_upage (upage);
  if (f != NULL)
    {
      hash_delete (&frame_table, &f->hash_elem);
      free (f);
    }
}

unsigned
frame_hash_func (const struct hash_elem *e, void *aux UNUSED) 
{
  const struct frame *f = hash_entry (e, struct frame, hash_elem);
  return hash_bytes (&f->addr, sizeof &f->addr);
}

bool
frame_less_func (const struct hash_elem *a, const struct hash_elem *b,
                 void *aux UNUSED)
{
  const struct frame *fa = hash_entry (a, struct frame, hash_elem);
  const struct frame *fb = hash_entry (b, struct frame, hash_elem);
  return fa->addr < fb->addr;
}

void
frame_evict ()
{
  lock_acquire (&eviction_lock);

  struct pool *user_pool = get_user_pool ();
  struct frame *evictee;  
  int frame_table_size = hash_size (&frame_table);
  int index;
  
  do {
    index = (random_ulong () % (frame_table_size - 1)) + 1;
    void *i = user_pool->base + PGSIZE * index;
    evictee = frame_lookup (i);
  } while (evictee == NULL || !evictee->evictable);

  struct page *upage = page_lookup (&evictee->owner->sup_page_table, evictee->uaddr);
  //TODO - remove this shit
  //lock_acquire (upage->access_lock);
  //lock_release (upage->access_lock);
  pagedir_clear_page (evictee->owner->pagedir, evictee->uaddr);
  
  if (upage->write)
    upage->saddr = swap_write_page (upage);
  frame_remove (evictee->addr);
  
  palloc_free_page (evictee->addr);
  free (evictee);

  lock_release (&eviction_lock);
}

void
frame_destroy (struct hash_elem *e, void *aux UNUSED)
{
  struct frame *f = hash_entry (e, struct frame, hash_elem);
  free (f);
}

void
frame_table_destroy ()
{
  hash_destroy (&frame_table, &frame_destroy);
}

struct frame *
frame_find_upage (uint8_t *uaddr)
{
  struct hash_iterator i;
  hash_first (&i, &frame_table);
  while (hash_next (&i))
    {
      struct frame *f = hash_entry (hash_cur(&i), struct frame, hash_elem);
      if (f->uaddr == uaddr)
        return f;
    }
  return NULL;
}

