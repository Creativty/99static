#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <stdbool.h>
#include <sys/stat.h>

#define ESUCCESS 0

int	max(int a, int b) {
	return (a > b ? a : b);
}

int		string_length(const char *str) {
	int	len = 0;
	while (str != NULL && str[len])
		len++;
	return (len);
}

int		string_index(const char *str, const char *pat) {
	if (str == pat)
		return (true);
	if (str == NULL || pat == NULL)
		return (false);
	int	len = string_length(str);
	for (int i = 0; i + string_length(pat) <= len; i++) {
		int j = 0;
		while (pat[j]) {
			if (str[i + j] != pat[j])
				break ;
			j++;
		}
		if (pat[j] == '\0')
			return (i);
	}
	return (-1);
}

bool	string_equals(const char *lhs, const char *rhs) {
	if (string_length(lhs) == string_length(rhs))
		return (string_index(lhs, rhs) == 0);
	return (false);
}

bool	string_suffix(const char *str, const char *sfx) {
	return (string_index(str, sfx) == string_length(str) - string_length(sfx));
}

bool	string_prefix(const char *str, const char *pfx) {
	return (string_index(str, pfx) == 0);
}

const char	*string_trim_left(const char *str) {
	int	index = 0;
	while (str != NULL && (str[index] == ' ' || str[index] == '\t'))
		index++;
	return (&str[index]);
}

void	memory_copy(void *dest, void *src, int size) {
	if (dest == NULL || src == NULL)
		return ;
	for (int i = 0; i < size; i++)
		((char *)dest)[i] = ((char *)src)[i];
}

char	*file_load(const char *path) {
	int		fd = open(path, O_RDONLY);
	if (fd < 0)
		return (NULL);

	char	*data = NULL;
	int		length = 0;
	while (true) {
		char	buff[512] = { 0 };
		int	count = (int)read(fd, buff, 511);
		if (count == 0)
			break ;
		if (count < 0)
			return (free(data), NULL);
		data = realloc(data, sizeof(char) * (unsigned long)(count + length + 1));
		if (data == NULL)
			return (NULL);
		memory_copy(&data[length], buff, count);
		length += count;
	}
	return (data);
}

struct segment {
	int		index: 32;
	int		count: 31;
	bool	eof: 1;
};

struct segment	*file_segment(const char *source) {
	errno = ESUCCESS;
	if (source == NULL)
		return (NULL);

	int					cap = 0;
	int					len = 0;
	int					index = 0;
	struct segment		*segments = NULL;
	while (source[index]) {
		int	count = 0;
		bool	is_comment = string_prefix(&source[index], "<!--");
		while (source[index + count] && !string_prefix(&source[index + count], is_comment ? "-->" : "<!--"))
			count++;
		if (is_comment && string_prefix(&source[index + count], "-->"))
			count += 3;

		struct segment segment = { index, count, source[index + count] == '\0' };
		if (len >= cap) {
			if (cap == 0) cap = 8;
			else cap = cap * 2;

			segments = realloc(segments, sizeof(struct segment) * (unsigned long)cap);
			if (segments == NULL)
				return (NULL);
		}
		segments[len++] = segment;
		index += count;
	}
	return (segments);
}

struct token {
	const char	*txt;
	int			len: 31;
	bool		eos: 1;
};

bool			gsx_token_match(struct token token, const char *keyword) {
	for (int i = 0; i < token.len; i++) {
		if (keyword[i] == '\0') return (false);
		if (keyword[i] != token.txt[i]) return (false);
	}
	return (true);
}

void			gsx_tokens_dump(struct token* tokens) {
	fprintf(stdout, "99static: token { ");
	for (int i = 0; !tokens[i].eos; i++) {
		fprintf(stdout, "`%.*s`", tokens[i].len, tokens[i].txt);
		if (!tokens[i + 1].eos)
			fprintf(stdout, ", ");
	}
	fprintf(stdout, " }\n");
}

int				gsx_tokens_length(struct token* tokens) {
	int	len = 0;
	while (tokens && !tokens[len].eos)
		len++;
	return (len);
}

struct token	*gsx_scan(const char *source, int length) {
	errno = ESUCCESS;
	if (source == NULL || length <= 0) return (NULL);

	int				cap = 0;
	int				len = 0;
	struct token	*tokens = NULL;
	for (int i = 0; i < length;) {
		while (i < length) {
			if (source[i] != ' ' && source[i] != '\t')
				break ;
			i++;
		}
		if (i >= length)
			break ;

		int	begin = i;
		if (source[i] == '"') {
			while (i < length) {
				char	c = source[i];
				if (i++ != begin && c == '"')
					break ;
			}
		} else {
			while (i < length) {
				if (source[i] == ' ' || source[i] == '\t')
					break ;
				i++;
			}
		}
		if (i - begin <= 0)
			break ;
		if (cap <= len) {
			if (cap == 0) cap = 4;
			else cap = cap * 2;

			tokens = realloc(tokens, sizeof(struct token) * (unsigned long)cap);
			if (tokens == NULL)
				return (NULL);
		}
		tokens[len++] = (struct token){ &source[begin], i - begin, false };
	}

	if (cap == len && cap != 0) {
		tokens = realloc(tokens, sizeof(struct token) * (unsigned long)(cap + 1));
		if (tokens == NULL)
			return (NULL);
	}
	if (tokens) tokens[len].eos = true;
	return (tokens);
}

const char	*gsx_strip(const char *source, struct segment segment, int *length) {
	if (length == NULL) return (NULL);
	const char	*begin = &source[segment.index], *current = &source[segment.index];
	*length = segment.count;

	if (!string_prefix(current, "<!--")) return (NULL);
	current = string_trim_left(&current[4]);

	if (!string_prefix(current, "gsx:")) return (NULL);
	current = string_trim_left(&current[4]);

	*length = max(0, segment.count - (int)(current - begin) - 3);
	if (*length <= 0) return (NULL);
	return (current);
}

// NOTE(XENOBAS): Do we support multiple return values ?!?, I kinda don't wanna...

struct call;

struct arguments {
	union {
		struct call		*call;
		struct token	*atom;
	}				data;
	int				len;
	int				cap;
	bool			is_atom;
};

struct call {
	struct token		*name;
	struct arguments	arguments;
};

struct	define {
	union {
		struct call		*call;
		struct token	*atom;
	}		data;
	bool	is_atom;
};

struct defines {
	struct define	*data;
	int				len;
	int				cap;
};

struct gsx {
	struct defines	defines;
};

struct define	*gsx_is_defined(struct gsx *gsx, struct token *tokens) {
	(void)gsx;
	(void)tokens;
	return (NULL);
}

struct call	*gsx_interpret(struct token* tokens) {
	if (tokens == NULL || tokens->eos) return (NULL);

	if (gsx_is_defined()) {
	}
	if (gsx_token_match(*tokens, "core::print")) {
		struct token		*token = tokens;
		struct arguments	arguments = { 0 };
		struct call *call = gsx_interpret(&tokens[1]);
	}
	return (gsx_interpret(tokens));
}

bool			gsx_evaluate(const char *source, struct segment segment) {
	int			length = 0;
	const char	*current = gsx_strip(source, segment, &length);
	if (current == NULL) return (false);

	struct token	*tokens = gsx_scan(current, length);
	if (tokens == NULL) return (false);


	// Parser
 	if (!gsx_interpret(tokens))
		fprintf(stderr, "99static: Unrecognized procedure `%.*s`\n", *tokens.len, *tokens.txt);

	// Cleanup
	free(tokens);
	return (true);
}

int	main(void) {
	const char	*cwd = getcwd(NULL, 0);
	if (cwd == NULL) {
		fprintf(stderr, "99static: Could not `getcwd` because of: %m\n");
		return (1);
	}

	DIR	*dir = opendir(cwd);
	if (dir == NULL) {
		fprintf(stderr, "99static: Could not `opendir` at `%s` because of: %m\n", cwd);
		return (1);
	}

	struct dirent	*ent = NULL;
	while ((ent = readdir(dir))) {
		if (string_equals(ent->d_name, ".")) continue ;
		if (string_equals(ent->d_name, "..")) continue ;
		if (!string_suffix(ent->d_name, ".gsx")) continue ;

		char	path[1024] = { 0 };
		snprintf(path, 1024, "%s/%s", cwd, ent->d_name);
		char	*source = file_load(path);
		if (source == NULL) {
			fprintf(stderr, "99static: Could not `file_load` at `%s` because of: %m\n", path);
			continue ;
		}

		struct segment *segments = file_segment(source);
		if (segments == NULL && errno) {
			fprintf(stderr, "99static: Could not `file_segment` at `%s` because of: %m\n", path);
			continue ;
		}

		fprintf(stderr, "99static: parsing `%s`\n", path);
		for (int i = 0; !segments[i].eof; i++) {
			struct segment segment = segments[i];

			bool	success = gsx_evaluate(source, segment);
			(void)success;
		}
	}

	closedir(dir);
	free((void *)cwd);
	return (0);
}

// Template evaluator.
