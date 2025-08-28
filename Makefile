# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: aindjare <marvin@42.fr>                    +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/08/25 09:04:51 by aindjare          #+#    #+#              #
#    Updated: 2025/08/25 09:11:28 by aindjare         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

NAME	:=	99static
SRCS	:=	$(wildcard *.c)
OBJS	:=	$(SRCS:.c=.o)
DEPS	:=	$(OBJS:.o=.d)

CC		:=	cc
CFLAGS	:=	-Wall -Wextra -Werror -Wconversion -Wswitch-enum -std=c99 -g -MMD -MP

all: $(NAME)

clean:
	$(RM) $(OBJS) $(DEPS)

fclean: clean
	$(RM) $(NAME)

re: fclean all

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) -o $@	$^

-include $(DEPS)

.PHONY: all clean fclean re
.SECONDARY: $(OBJS) $(DEPS)
