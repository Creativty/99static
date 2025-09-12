#!/usr/bin/env bash
cc --std=c99 -Wall -Wextra -Werror -pedantic -Wswitch-enum -ggdb -o 99static	$@	main.c
