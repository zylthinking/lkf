# lkf
an lock free list implement

It is wait free at case of multi writers and single reader.
It is kind of wait free at case of multi writers and multi readers.

# compare with boost spsc_queue
* single writer and reader, it has higher read/write perfprmance
* multi readers and writers, boost spsc_queue can't be used at this case,  while lkf works well
