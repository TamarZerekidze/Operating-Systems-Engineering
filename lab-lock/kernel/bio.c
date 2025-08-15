// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"


struct bufbucket{
  struct buf head;
  struct spinlock lock;
};

struct {
  struct buf buf[NBUF];
  struct bufbucket bufs[NBUCKETS];
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
} bcache;

void
binit(void)
{
  struct buf *b;

  // Create linked list of buffers
  for(int i = 0; i < NBUCKETS; i++) {
    initlock(&bcache.bufs[i].lock, "bcache");
    struct buf *head = &bcache.bufs[i].head;
    head->next = head;
    head->prev = head;
  }

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.bufs[0].head.next;
    b->prev = &bcache.bufs[0].head;
    initsleeplock(&b->lock, "buffer");
    bcache.bufs[0].head.next->prev = b;
    bcache.bufs[0].head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint hash = blockno % NBUCKETS;

  acquire(&bcache.bufs[hash].lock);
  // Is the block already cached?
  for(b = bcache.bufs[hash].head.next; b != &bcache.bufs[hash].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bufs[hash].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.bufs[hash].head.prev; b != &bcache.bufs[hash].head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.bufs[hash].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  for(int i = 0; i < NBUCKETS; i++) {
    if(hash != i) {
      acquire(&bcache.bufs[i].lock);
      for(b = bcache.bufs[i].head.prev; b != &bcache.bufs[i].head; b = b->prev){
        if(b->refcnt == 0) {
          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;
          b->refcnt = 1;
          b->next->prev = b->prev;
          b->prev->next = b->next;
          release(&bcache.bufs[i].lock);
          b->next = bcache.bufs[hash].head.next;
          b->prev = &bcache.bufs[hash].head;
          bcache.bufs[hash].head.next->prev = b;
          bcache.bufs[hash].head.next = b;

          release(&bcache.bufs[hash].lock);
          acquiresleep(&b->lock);
          return b;
        }
      }
      release(&bcache.bufs[i].lock);
    }
  }

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint hash = (b->blockno) % NBUCKETS;
  acquire(&bcache.bufs[hash].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.bufs[hash].head.next;
    b->prev = &bcache.bufs[hash].head;
    bcache.bufs[hash].head.next->prev = b;
    bcache.bufs[hash].head.next = b;
  }
  
  release(&bcache.bufs[hash].lock);
}

void
bpin(struct buf *b) {
  uint hash = (b->blockno) % NBUCKETS;
  acquire(&bcache.bufs[hash].lock);
  b->refcnt++;
  release(&bcache.bufs[hash].lock);
}

void
bunpin(struct buf *b) {
  uint hash = (b->blockno) % NBUCKETS;
  acquire(&bcache.bufs[hash].lock);
  b->refcnt--;
  release(&bcache.bufs[hash].lock);
}


