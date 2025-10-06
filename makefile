TARGET = libmemchk.so memchk
TEST = mctest
MCOBJS = memchk_init.o memchk_hook.o memchk_allocator.o memchk_alloc_blk.o memchk_manage_memblk.o memchk_callstack.o memchk_hashtable.o memchk_log.o memchk_signal.o memchk_snapshot.o memchk_filemap.o memchk_symbol.o memchk_buffer.o memchk_virtmem.o memchk_sort.o memchk_util.o
CLOBJS = memchk_client.o

all: $(TARGET) $(TEST)

CFLAGS += -Wall -fPIC -MMD -g -O

libmemchk.so : $(MCOBJS)
	$(CC) -shared -Wl,-soname,$@ -o $@ $(MCOBJS) -lbfd

memchk : $(CLOBJS)
	$(CC) -o $@ $^

mctest : mctest.c
	$(CC) -o $@ $^ -Wall -g

clean:
	rm -f $(TARGET) $(TEST) *.o *.d *.so

-include *.d
