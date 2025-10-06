### Overview
This tool hooks and monitors functions that handle heap memory, such as malloc and free.
It provides the following features:

* Display memory usage
* Show call stack for memory blocks
* Display memory increase/decrease between snapshot creation and current time
* Detect memory overrun writes and underrun writes
* Detect double free
* Detect writes to freed memory

### Build
* git clone git@github.com:ohsawa1204/memchk.git
* cd memchk
* make

### Usage
* Launch the target process with LD_PRELOAD
  - Example: `LD_PRELOAD=./libmemchk.so ./mctest`
* Run `./memchk -u` to obtain the target process's PID
* Execute various commands
  - Command results are output to `./memchk/mc<pid>.txt`

### Command Description
* `-h` Display help
* `-a` Display all memory blocks
* `-A` Display all memory blocks per call stack
* `-b` Perform buffer checks on all memory blocks
* `-c` Compare snapshot with current memory
* `-C` Display snapshot vs. current memory comparison per call stack
* `-d` Delete snapshot
* `-g` Display memory blocks as a size-ordered histogram
* `-p` Set target process by pid
* `-m` Display simplified view of all memory blocks
* `-M` Display virtual memory usage for all memory blocks
* `-s` Create a snapshot
* `-u` Update the target process
* `-l` Delete all log files

### Example Test Program Execution
* Run the test program on Terminal 1 with `LD_PRELOAD=./libmemchk.so ./mctest`
```
Hit enter to start
```
* Run `./memchk -u` on Terminal 2
```
current target pid = 2143599
```
* Run `tail -f ~/memchk/mc<pid>.txt` on Terminal 3
```
memory checker (pid = 2143599) started
comm = mctest
cmdline = ./mctest

work_thread tid = 2143600
```
* Run `./memchk -s` in Terminal 2 to create a snapshot
  - Output indicating creation appears in `~/memchk/mc<pid>.txt`
```
creating snapshot...

snapshot created successfully.
```
* Press Enter on Terminal 1
```
memalign = 0x55950058d470
valloc = 0x55950058e000
posix_memalign = 0x55950058e510
malloc = 0x55950058e560
malloc = 0x55950058e5d0


Hit enter to call access_freed_area
```
* Run `./memchk -C` on Terminal 2 to display the difference between the snapshot and the current memory blocks
  - Differences are output to `~/memchk/mc<pid>.txt`
```
group 0: 64 64  (total 128 bytes)
---
malloc @ /home/takashiosawa/work/tools/memcheck/libmemchk.so [0x7f6696e04195 (4195)]
  |- malloc in /home/takashiosawa/work/tools/memcheck/memchk_hook.c:57
main @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe7594a5 (14a5)]
  |- main in /home/takashiosawa/work/tools/memcheck/mctest.c:54
__libc_start_call_main @ /usr/lib/x86_64-linux-gnu/libc.so.6 [0x7f6696a29d90 (29d90)]
  |- __libc_start_call_main in ./csu/../sysdeps/nptl/libc_start_call_main.h:58
call_init @ /usr/lib/x86_64-linux-gnu/libc.so.6 [0x7f6696a29e40 (29e40)]
  |- call_init in ./csu/../csu/libc-start.c:128
  |- __libc_start_main_impl in ./csu/../csu/libc-start.c:379
_start @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe7591e5 (11e5)]

group 1: 30  (total 30 bytes)
---
__aligned_allocator @ /home/takashiosawa/work/tools/memcheck/libmemchk.so [0x7f6696e040e7 (40e7)]
  |- __aligned_allocator in /home/takashiosawa/work/tools/memcheck/memchk_hook.c:170
memalign @ /home/takashiosawa/work/tools/memcheck/libmemchk.so [0x7f6696e04499 (4499)]
  |- memalign in /home/takashiosawa/work/tools/memcheck/memchk_hook.c:201
valloc @ /home/takashiosawa/work/tools/memcheck/libmemchk.so [0x7f6696e044bb (44bb)]
  |- valloc in /home/takashiosawa/work/tools/memcheck/memchk_hook.c:206
main @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe75942f (142f)]
  |- main in /home/takashiosawa/work/tools/memcheck/mctest.c:48
__libc_start_call_main @ /usr/lib/x86_64-linux-gnu/libc.so.6 [0x7f6696a29d90 (29d90)]
  |- __libc_start_call_main in ./csu/../sysdeps/nptl/libc_start_call_main.h:58
call_init @ /usr/lib/x86_64-linux-gnu/libc.so.6 [0x7f6696a29e40 (29e40)]
  |- call_init in ./csu/../csu/libc-start.c:128
  |- __libc_start_main_impl in ./csu/../csu/libc-start.c:379
_start @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe7591e5 (11e5)]

group 2: 30  (total 30 bytes)
---
__aligned_allocator @ /home/takashiosawa/work/tools/memcheck/libmemchk.so [0x7f6696e040e7 (40e7)]
  |- __aligned_allocator in /home/takashiosawa/work/tools/memcheck/memchk_hook.c:170
posix_memalign @ /home/takashiosawa/work/tools/memcheck/libmemchk.so [0x7f6696e0440f (440f)]
  |- posix_memalign in /home/takashiosawa/work/tools/memcheck/memchk_hook.c:180
main @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe759464 (1464)]
  |- main in /home/takashiosawa/work/tools/memcheck/mctest.c:50
__libc_start_call_main @ /usr/lib/x86_64-linux-gnu/libc.so.6 [0x7f6696a29d90 (29d90)]
  |- __libc_start_call_main in ./csu/../sysdeps/nptl/libc_start_call_main.h:58
call_init @ /usr/lib/x86_64-linux-gnu/libc.so.6 [0x7f6696a29e40 (29e40)]
  |- call_init in ./csu/../csu/libc-start.c:128
  |- __libc_start_main_impl in ./csu/../csu/libc-start.c:379
_start @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe7591e5 (11e5)]

group 3: 30  (total 30 bytes)
---
__aligned_allocator @ /home/takashiosawa/work/tools/memcheck/libmemchk.so [0x7f6696e040e7 (40e7)]
  |- __aligned_allocator in /home/takashiosawa/work/tools/memcheck/memchk_hook.c:170
memalign @ /home/takashiosawa/work/tools/memcheck/libmemchk.so [0x7f6696e04499 (4499)]
  |- memalign in /home/takashiosawa/work/tools/memcheck/memchk_hook.c:201
main @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe759406 (1406)]
  |- main in /home/takashiosawa/work/tools/memcheck/mctest.c:46
__libc_start_call_main @ /usr/lib/x86_64-linux-gnu/libc.so.6 [0x7f6696a29d90 (29d90)]
  |- __libc_start_call_main in ./csu/../sysdeps/nptl/libc_start_call_main.h:58
call_init @ /usr/lib/x86_64-linux-gnu/libc.so.6 [0x7f6696a29e40 (29e40)]
  |- call_init in ./csu/../csu/libc-start.c:128
  |- __libc_start_main_impl in ./csu/../csu/libc-start.c:379
_start @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe7591e5 (11e5)]
```
* Press Enter on Terminal 1 to execute the access_freed_area function
```
access_freed_area returned


Hit enter to call double_free
```
* Run `./memchk -b` on Terminal 2 to perform a buffer check
  - Detected errors are output to `~/memchk/mc<pid>.txt`
```
FREED area (0x55950058e640:10) was write-accessed!!
 current time = 2025/10/06/18:16:17.105490
 checked area = 16 + 10 + 16 bytes (leading red zone + user buffer + trailing red zone)
write-access was detected at offset 16 from the top of the leading red zone

This memory block was allocated from:
malloc @ /home/takashiosawa/work/tools/memcheck/libmemchk.so [0x7f6696e04195 (4195)]
  |- malloc in /home/takashiosawa/work/tools/memcheck/memchk_hook.c:57
access_freed_area @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe7592bf (12bf)]
  |- access_freed_area in /home/takashiosawa/work/tools/memcheck/mctest.c:9
main @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe759518 (1518)]
  |- main in /home/takashiosawa/work/tools/memcheck/mctest.c:62
__libc_start_call_main @ /usr/lib/x86_64-linux-gnu/libc.so.6 [0x7f6696a29d90 (29d90)]
  |- __libc_start_call_main in ./csu/../sysdeps/nptl/libc_start_call_main.h:58
call_init @ /usr/lib/x86_64-linux-gnu/libc.so.6 [0x7f6696a29e40 (29e40)]
  |- call_init in ./csu/../csu/libc-start.c:128
  |- __libc_start_main_impl in ./csu/../csu/libc-start.c:379
_start @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe7591e5 (11e5)]

and freed from:
free @ /home/takashiosawa/work/tools/memcheck/libmemchk.so [0x7f6696e0420c (420c)]
  |- free in /home/takashiosawa/work/tools/memcheck/memchk_hook.c:77
access_freed_area @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe7592cf (12cf)]
  |- access_freed_area in /home/takashiosawa/work/tools/memcheck/mctest.c:11
main @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe759518 (1518)]
  |- main in /home/takashiosawa/work/tools/memcheck/mctest.c:62
__libc_start_call_main @ /usr/lib/x86_64-linux-gnu/libc.so.6 [0x7f6696a29d90 (29d90)]
  |- __libc_start_call_main in ./csu/../sysdeps/nptl/libc_start_call_main.h:58
call_init @ /usr/lib/x86_64-linux-gnu/libc.so.6 [0x7f6696a29e40 (29e40)]
  |- call_init in ./csu/../csu/libc-start.c:128
  |- __libc_start_main_impl in ./csu/../csu/libc-start.c:379
_start @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe7591e5 (11e5)]
```
* Press Enter on Terminal 1 to execute the double_free function
```
double_free returned


Hit enter to call memory_overrun
```
  - Detected errors are output to `~/memchk/mc<pid>.txt`
```
Double delete or free (or realloc) !!! (0x55950058e680)

This memory block was allocated from:
malloc @ /home/takashiosawa/work/tools/memcheck/libmemchk.so [0x7f6696e04195 (4195)]
  |- malloc in /home/takashiosawa/work/tools/memcheck/memchk_hook.c:57
double_free @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe7592f6 (12f6)]
  |- double_free in /home/takashiosawa/work/tools/memcheck/mctest.c:16
main @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe759562 (1562)]
  |- main in /home/takashiosawa/work/tools/memcheck/mctest.c:68
__libc_start_call_main @ /usr/lib/x86_64-linux-gnu/libc.so.6 [0x7f6696a29d90 (29d90)]
  |- __libc_start_call_main in ./csu/../sysdeps/nptl/libc_start_call_main.h:58
call_init @ /usr/lib/x86_64-lipnux-gnu/libc.so.6 [0x7f6696a29e40 (29e40)]
  |- call_init in ./csu/../csu/libc-start.c:128
  |- __libc_start_main_impl in ./csu/../csu/libc-start.c:379
_start @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe7591e5 (11e5)]

freed from:
free @ /home/takashiosawa/work/tools/memcheck/libmemchk.so [0x7f6696e0420c (420c)]
  |- free in /home/takashiosawa/work/tools/memcheck/memchk_hook.c:77
double_free @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe759306 (1306)]
  |- double_free in /home/takashiosawa/work/tools/memcheck/mctest.c:18
main @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe759562 (1562)]
  |- main in /home/takashiosawa/work/tools/memcheck/mctest.c:68
__libc_start_call_main @ /usr/lib/x86_64-linux-gnu/libc.so.6 [0x7f6696a29d90 (29d90)]
  |- __libc_start_call_main in ./csu/../sysdeps/nptl/libc_start_call_main.h:58
call_init @ /usr/lib/x86_64-linux-gnu/libc.so.6 [0x7f6696a29e40 (29e40)]
  |- call_init in ./csu/../csu/libc-start.c:128
  |- __libc_start_main_impl in ./csu/../csu/libc-start.c:379
_start @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe7591e5 (11e5)]

and then is being freed from:
double_free @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe759312 (1312)]
  |- double_free in /home/takashiosawa/work/tools/memcheck/mctest.c:19
main @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe759562 (1562)]
  |- main in /home/takashiosawa/work/tools/memcheck/mctest.c:68
__libc_start_call_main @ /usr/lib/x86_64-linux-gnu/libc.so.6 [0x7f6696a29d90 (29d90)]
  |- __libc_start_call_main in ./csu/../sysdeps/nptl/libc_start_call_main.h:58
call_init @ /usr/lib/x86_64-linux-gnu/libc.so.6 [0x7f6696a29e40 (29e40)]
  |- call_init in ./csu/../csu/libc-start.c:128
  |- __libc_start_main_impl in ./csu/../csu/libc-start.c:379
_start @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe7591e5 (11e5)]
```
* Run `./memchk -s` in Terminal 2 to create a snapshot
  - Output indicating creation appears in `~/memchk/mc<pid>.txt`
```
creating snapshot...

snapshot created successfully.
```
* Press Enter on Terminal 1 to execute the memory_leak function
```
memory_leak returned


Hit enter to finish
```
* Run `./memchk -C` on Terminal 2 to display the difference between the snapshot and the current memory blocks
  - Differences are output to `~/memchk/mc<pid>.txt`
```
group 0: 10  (total 10 bytes)
---
malloc @ /home/takashiosawa/work/tools/memcheck/libmemchk.so [0x7f6696e04195 (4195)]
  |- malloc in /home/takashiosawa/work/tools/memcheck/memchk_hook.c:57
memory_leak @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe75935f (135f)]
  |- memory_leak in /home/takashiosawa/work/tools/memcheck/mctest.c:30
main @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe7595f3 (15f3)]
  |- main in /home/takashiosawa/work/tools/memcheck/mctest.c:80
__libc_start_call_main @ /usr/lib/x86_64-linux-gnu/libc.so.6 [0x7f6696a29d90 (29d90)]
  |- __libc_start_call_main in ./csu/../sysdeps/nptl/libc_start_call_main.h:58
call_init @ /usr/lib/x86_64-linux-gnu/libc.so.6 [0x7f6696a29e40 (29e40)]
  |- call_init in ./csu/../csu/libc-start.c:128
  |- __libc_start_main_impl in ./csu/../csu/libc-start.c:379
_start @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe7591e5 (11e5)]

group 1: 5  (total 5 bytes)
---
malloc @ /home/takashiosawa/work/tools/memcheck/libmemchk.so [0x7f6696e04195 (4195)]
  |- malloc in /home/takashiosawa/work/tools/memcheck/memchk_hook.c:57
__GI___strdup @ /usr/lib/x86_64-linux-gnu/libc.so.6 [0x7f6696aa858f (a858f)]
  |- __GI___strdup in ./string/strdup.c:44
memory_leak @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe759372 (1372)]
  |- memory_leak in /home/takashiosawa/work/tools/memcheck/mctest.c:31
main @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe7595f3 (15f3)]
  |- main in /home/takashiosawa/work/tools/memcheck/mctest.c:80
__libc_start_call_main @ /usr/lib/x86_64-linux-gnu/libc.so.6 [0x7f6696a29d90 (29d90)]
  |- __libc_start_call_main in ./csu/../sysdeps/nptl/libc_start_call_main.h:58
call_init @ /usr/lib/x86_64-linux-gnu/libc.so.6 [0x7f6696a29e40 (29e40)]
  |- call_init in ./csu/../csu/libc-start.c:128
  |- __libc_start_main_impl in ./csu/../csu/libc-start.c:379
_start @ /home/takashiosawa/work/tools/memcheck/mctest [0x5594fe7591e5 (11e5)]
```
