# To compile with test1, make test1
# To compile with test2, make test2
CC = clang -g -Wall
EXECUTABLE=sfs
SOURCES_TEST1= disk_emu.c sfs_api.c sfs_test1.c tests.c
SOURCES_TEST2= disk_emu.c sfs_api.c sfs_test2.c tests.c

test1: $(SOURCES_TEST1)
	$(CC) -o $(EXECUTABLE) $(SOURCES_TEST1)

test2: $(SOURCES_TEST2)
	$(CC) -o $(EXECUTABLE) $(SOURCES_TEST2)
clean:
	rm $(EXECUTABLE)

gdb1: $(SOURCES_TEST1)
	clear
	$(CC) $(SOURCES_TEST1) -o $(EXECUTABLE) && gdb -ex run ./$(EXECUTABLE) -ex backtrace -ex quit

gdb2: $(SOURCES_TEST2)
	clear
	$(CC) $(SOURCES_TEST2) -o $(EXECUTABLE) && gdb -ex run ./$(EXECUTABLE) -ex backtrace -ex quit

valgrind2: $(SOURCES_TEST1)
	clear
	$(CC) $(SOURCES_TEST1) -o $(EXECUTABLE) && valgrind --tool=memcheck --leak-check=yes ./$(EXECUTABLE)

valgrind2: $(SOURCES_TEST2)
	clear
	$(CC) $(SOURCES_TEST2) -o $(EXECUTABLE) && valgrind --tool=memcheck --leak-check=yes ./$(EXECUTABLE)
