#get all .c file name split by 'blank space'
SRCS = $(wildcard *.c)
#file.c ==> file.o 
OBJS = $(SRCS:.c=.o)

# target name.
TARGET = adhoc

#complie falgs
CFLAGS = -w -O -g

#add libs for thread timer and math
LIBS := -lpthread -lrt -lm

#c complier
CC = gcc

#rm command
RM := rm -rf

$(TARGET) : $(OBJS)
	#$(CC) $^ -o $@
	$(CC) -o $(TARGET) $(OBJS) $(LIBS)
	@echo "make done"
%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS) $(TARGET)
	@echo "clean done"

