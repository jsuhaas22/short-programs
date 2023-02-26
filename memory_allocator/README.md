# MEMALLOC:

The file implements my versions of malloc(), free(), calloc() and realloc().

It has been tested with a simple linked list program.

## TODO:

1. malloc() returns the first block that is equal to or more than the requested
   size, through get_free_blk() function, without sizing it down. For example,
   if 100 bytes are requested but the first block it finds is of 120 bytes, it
   returns all 120 bytes, thereby wasting 20 bytes. Make it more efficient by
   making malloc() cut down the block to the exact size requested.

2. Explore ways to make it faster, if possible.

3. Make it respect page boundaries.
