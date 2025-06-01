COMPILER = gcc

NAME = a.out

FLAGS = -std=gnu99 -g

SRC = shell.c job_control.c

OBJS = $(SRC:.c=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(COMPILER) $(FLAGS) $(OBJS) -pthread -lreadline -o $(NAME)

%.o: %.c
	$(COMPILER) $(FLAGS) -o $@ -c $<

clean:
	rm -rf $(OBJS)

fclean: clean
	rm -rf $(NAME)

re: fclean all

.PHONY: all clean fclean re
