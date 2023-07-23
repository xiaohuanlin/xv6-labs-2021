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


#define BUCKET_NUM 13

struct node {
  struct buf *buf;
  struct node *prev;
  struct node *next;
};


struct bucket {
  struct spinlock lock;
  struct node *head;
  struct node *tail;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct node nodes[NBUF];
  struct bucket buckets[BUCKET_NUM];
} bcache;


uint64
get_key(uint dev, uint blockno)
{
  return (((uint64)dev << 32) | (uint64)blockno) % BUCKET_NUM;
}


void
binit(void)
{
  struct buf *b;
  struct node *n;
  struct bucket *bucket;

  initlock(&bcache.lock, "bcache");

  for (int i = 0; i < BUCKET_NUM; i++) {
    bucket = &bcache.buckets[i];
    initlock(&bucket->lock, "bcache.bucket");
    bucket->head = 0;
    bucket->tail = 0;
  }

  for (int i = 0; i < NBUF; i++) {
    b = &bcache.buf[i];
    n = &bcache.nodes[i];
    initsleeplock(&b->lock, "buffer");
    b->refcnt = 0;
    b->last_use = 0;
    n->buf = b;
    n->prev = 0;
    n->next = 0;

    // init bucket
    uint64 key = get_key(b->dev, b->blockno);
    bucket = &bcache.buckets[key];
    if (bucket->head) {
      bucket->tail->next = n;
      n->prev = bucket->tail;
    } else {
      bucket->head = n;
    }
    bucket->tail = n;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  struct node *n;
  struct bucket *bucket;

  uint64 key = get_key(dev, blockno);
  bucket = &bcache.buckets[key];
  acquire(&bucket->lock);

  // Is the block already cached?
  for(n = bucket->head; n != 0; n = n->next){
    b = n->buf;
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      b->last_use = ticks;
      release(&bucket->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bucket->lock);

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  acquire(&bcache.lock);
  // check again
  for(n = bucket->head; n != 0; n = n->next){
    b = n->buf;
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      b->last_use = ticks;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  uint min_time = -1;
  struct node *min_n = 0;
  struct bucket *min_bucket = 0;

  for(int i = 0; i < BUCKET_NUM; i++){
    bucket = &bcache.buckets[i];
    acquire(&bucket->lock);
    int replaced = 0;
    for(n = bucket->head; n != 0; n = n->next){
      b = n->buf;
      if (b->refcnt == 0 && (min_time == -1 || b->last_use < min_time)) {
        replaced = 1;
        if (min_bucket != 0 && min_bucket != bucket) {
          release(&min_bucket->lock);
        }
        min_time = b->last_use;
        min_n = n;
        min_bucket = bucket;
      }
    }
    if (!replaced) {
      release(&bucket->lock);
    }
  }

  if (min_time != -1) {
    bucket = &bcache.buckets[key];

    if (min_bucket != bucket) {
      acquire(&bucket->lock);
      if (min_n->prev == 0 && min_n->next == 0) {
        // the only node
        min_bucket->head = 0;
        min_bucket->tail = 0;
      } else if (min_n->prev == 0) {
        // the first node
        min_n->next->prev = 0;
        min_bucket->head = min_n->next;
      } else if (min_n->next == 0) {
        // the last node
        min_n->prev->next = 0;
        min_bucket->tail = min_n->prev;
      } else {
        // in the middle
        min_n->prev->next = min_n->next;
        min_n->next->prev = min_n->prev;
      }

      // add it to the new bucket
      if (bucket->tail != 0) {
        bucket->tail->next = min_n;
        min_n->prev = bucket->tail;
        bucket->tail = min_n;
      } else {
        // the first node
        bucket->head = min_n;
        bucket->tail = min_n;
        min_n->prev = 0;
      }
      min_n->next = 0;
    }

    b = min_n->buf;
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    b->last_use = ticks;
    release(&min_bucket->lock);

    if (min_bucket != bucket) {
      release(&bucket->lock);
    }

    release(&bcache.lock);
    acquiresleep(&b->lock);
    return b;
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

  uint64 key = get_key(b->dev, b->blockno);
  acquire(&bcache.buckets[key].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->last_use = ticks;
  }
  release(&bcache.buckets[key].lock);
}

void
bpin(struct buf *b) {
  uint64 key = get_key(b->dev, b->blockno);
  acquire(&bcache.buckets[key].lock);
  b->refcnt++;
  release(&bcache.buckets[key].lock);
}

void
bunpin(struct buf *b) {
  uint64 key = get_key(b->dev, b->blockno);
  acquire(&bcache.buckets[key].lock);
  b->refcnt--;
  release(&bcache.buckets[key].lock);
}


