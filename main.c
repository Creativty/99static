#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <stdbool.h>
#include <sys/stat.h>

#define ESUCCESS 0

#define FMT_ERROR "\x1b[1;31m" "Error" "\x1b[0m"
#define FMT_99STATIC "\x1b[1;37m" "99static" "\x1b[0m"

int			max(int a, int b) {
	return (a > b ? a : b);
}

int			string_length(const char *str) {
	int	len = 0;
	while (str != NULL && str[len])
		len++;
	return (len);
}

int			string_index(const char *str, const char *pat) {
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

bool		string_equals(const char *lhs, const char *rhs) {
	if (string_length(lhs) == string_length(rhs))
		return (string_index(lhs, rhs) == 0);
	return (false);
}

bool		string_suffix(const char *str, const char *sfx) {
	return (string_index(str, sfx) == string_length(str) - string_length(sfx));
}

bool		string_prefix(const char *str, const char *pfx) {
	return (string_index(str, pfx) == 0);
}

const char	*string_trim_left(const char *str) {
	int	index = 0;
	while (str != NULL && (str[index] == ' ' || str[index] == '\t'))
		index++;
	return (&str[index]);
}

void		memory_copy(void *dest, void *src, int size) {
	if (dest == NULL || src == NULL)
		return ;
	for (int i = 0; i < size; i++)
		((char *)dest)[i] = ((char *)src)[i];
}

char		*string_clone(const char *src, int len) {
	if (src == NULL || len <= 0)
		return (NULL);
	char	*dst = malloc(sizeof(char) * ((unsigned long)len + 1ul));
	if (dst != NULL) {
		for (int i = 0; i < len; i++)
			dst[i] = src[i];
		dst[len] = '\0';
	}
	return (dst);
}

struct string_builder {
	char	*text;
	int		len;
	int		cap;
};

void		string_builder_free(struct string_builder *sb) {
	if (sb == NULL) return ;

	free(sb->text);
	*sb = (struct string_builder){ 0 };
}

void		string_builder_resize(struct string_builder *sb, int new_len) {
	if (new_len >= sb->cap) {
		sb->cap = (sb->cap <= 0) ? 8 : (sb->cap * 2);
		sb->text = realloc(sb->text, sizeof(char) * (unsigned long)(sb->cap));
		assert(sb->text != NULL);
	}
}

void		string_builder_append_string(struct string_builder *sb, const char *str, int len) {
	if (sb == NULL || str == NULL || len <= 0)
		return ;
	if (sb->len + len >= sb->cap)
		string_builder_resize(sb, sb->len + len);
	memory_copy((void *)&sb->text[sb->len], (void *)str, len);
	sb->len += len;
}

void		string_builder_append_char(struct string_builder *sb, const char c) {
	if (sb == NULL || c == '\0')
		return ;
	if (sb->len + 1 >= sb->cap)
		string_builder_resize(sb, sb->len + 1);
	sb->text[sb->len++] = c;
}

void		string_builder_clear(struct string_builder *sb) {
	sb->len = 0;
}

char		*string_builder_string(struct string_builder *sb) {
	return string_clone(sb->text, sb->len);
}

char		*file_load(const char *path) {
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
	int		index;
	int		count;
	bool	comment;
	bool	eof;
};

struct span {
	int	index;
	int	count;
};

struct spans {
	struct span	*data;
	int			len;
	int			cap;
};

struct ast_call {
	struct span	span;
	struct {
		struct ast	**data;
		int			len;
		int			cap;
	}			args;
};

struct ast_atom {
	struct span		span;
	bool			is_string;
	union {
		long		number;
		const char	*string;
	}				value;
};

struct ast_alias {
	struct span		span;
	struct ast_atom	*value;
	// TODO: HUH ?!?
};

enum ast_kind {
	AST_INVALID,
	AST_CALL,
	AST_ATOM,
	AST_ALIAS,
};

union ast_data {
	struct ast_call		*call;
	struct ast_atom		*atom;
	struct ast_alias	*alias;
};

struct ast {
	enum ast_kind	kind;
	union ast_data	data;
};

enum parameter_kind {
	PARAMETER_INVALID,
	PARAMETER_ATOM,
	PARAMETER_VARIABLE,
	PARAMETER_DESTRUCTURE,
};

union parameter_data {
	const char		*name;
	struct {
		const char	**names;
		const char	*tail;
	}				destructure;
};

struct parameter {
	enum parameter_kind		kind;
	union parameter_data	data;
};

struct builtin {
	void	*proc;
};

struct definition {
	const char			*span;
	struct parameter	*params;
	int					params_count;

	union {
		struct ast		*ast;
		void			*builtin;
	}					address;
	bool				is_builtin;
};

struct context {
	char				*source;
	struct {
		struct definition	*data;
		int					len;
		int					cap;
	}					definitions;
};

void				gsx_debug_spans(struct context *ctx, struct spans spans) {
	if (ctx == NULL || spans.len == 0 || spans.data == NULL) return ;
	fprintf(stdout, "99static: { ");
	for (int i = 0; i < spans.len; i++) {
		struct span span = spans.data[i];
		fprintf(stdout, "`%.*s`", span.count, &ctx->source[span.index]);
		if (i + 1 < spans.len) fprintf(stdout, ", ");
		else fprintf(stdout, " }\n");
	}
}

struct segment		*gsx_segment(const char *source) {
	errno = ESUCCESS;
	if (source == NULL)
		return (NULL);

	int					cap = 0;
	int					len = 0;
	int					index = 0;
	struct segment		*segments = NULL;
	while (source[index]) {
		int		count = 0;
		bool	is_comment = string_prefix(&source[index], "<!--");
		while (source[index + count] && !string_prefix(&source[index + count], is_comment ? "-->" : "<!--"))
			count++;
		if (is_comment && string_prefix(&source[index + count], "-->"))
			count += 3;

		struct segment segment = {
			.index = index,
			.count = count,
			.eof = source[index + count] == '\0',
			.comment = is_comment,
		};
		if (len >= cap) {
			cap = (cap == 0) ? 8 : (cap * 2);
			segments = realloc(segments, sizeof(struct segment) * (unsigned long)cap);
			if (segments == NULL)
				return (NULL);
		}
		segments[len++] = segment;
		index += count;
	}
	return (segments);
}

struct span			gsx_segment_span(const char *source, struct segment root) {
	const char		*str = &source[root.index];
	if (source == NULL)
		return ((struct span){ 0 });

	if (!string_prefix(str, "<!--"))
		return ((struct span){ 0 });
	str += 4;
	while (*str == ' ' || *str == '\t')
		str++;

	if (!string_prefix(str, "gsx:"))
		return ((struct span){ 0 });
	str += 4;
	while (*str == ' ' || *str == '\t')
		str++;

	const char	*end = str;
	while (!string_prefix(end, "-->") && (int)(end - &source[root.index] < root.count))
		end++;
	int	count = (int)(end - str);
	if (count >= root.count || count <= 0)
		return ((struct span){ 0 });
	return ((struct span){ .index = (int)(str - source), .count = count });
}

bool				gsx_match_span(struct context *ctx, struct span span, const char *str) {
	if (str == NULL || ctx == NULL) return (false);
	for (int i = 0; str[i]; i++) {
		if (i >= span.count) return (false);
		if (ctx->source[span.index + i] != str[i]) return (false);
	}
	return (true);
}

bool				gsx_match_spans(struct context *ctx, struct span lhs, struct span rhs) {
	if (ctx == NULL) return (false);
	if (lhs.count != rhs.count) return (false);
	for (int i = 0; i < lhs.count; i++) {
		if (ctx->source[lhs.index + i] != ctx->source[rhs.index + i])
		  return (false);
	}
	return (true);
}

bool				gsx_match_span_atom_string(struct context *ctx, struct span span) {
	if (ctx == NULL || span.count < 2) return (false);
	return (ctx->source[span.index] == '"' && ctx->source[span.index + span.count - 1] == '"');
}

bool				gsx_match_span_atom_number(struct context *ctx, struct span span) {
	if (ctx == NULL || span.count == 0) return (false);
	for (int i = 0; i < span.count; i++) {
		char	digit = ctx->source[span.index + i];
		if (digit < '0' || digit > '9')
			return (false);
	}
	return (true);
}

bool				gsx_match_span_call(struct context *ctx, struct span span) {
	if (ctx == NULL || span.count < 2) return (false);
	return (ctx->source[span.index + span.count - 1] == '!');
}

struct definition	*gsx_find_definition(struct context *ctx, struct span span) {
	if (ctx == NULL || ctx->source[span.index + span.count - 1] != '!') return (NULL);

	span.count--;
	for (int i = 0; i < ctx->definitions.len; i++) {
		struct definition	*definition = &ctx->definitions.data[i];
		if (gsx_match_span(ctx, span, definition->span))
			return (definition);
	}
	return (NULL);
}

struct spans		gsx_tokenize(const char *source, struct span root) {
	errno = ESUCCESS;
	struct spans spans = { 0 };

	if (source == NULL || root.count <= 0) return (spans);

	int i = 0;
	const char	*str = &source[root.index];
	while (i < root.count) {
		while (str[i] == ' ') i++;
		if (i >= root.count) break ;

		int		index = i;
		char	delimiter = str[i] == '"' ? '"' : ' ';
		if (delimiter == '"') i++;
		while (i < root.count && str[i] != delimiter)
			i++;
		if (delimiter == '"') i++;
		int		count = i - index;
		if (count <= 0) continue ;

		struct span span = { root.index + index, count };
		if (spans.len <= spans.cap) {
			spans.cap = (spans.cap <= 0) ? 8 : (spans.cap * 2);
			spans.data = realloc(spans.data, sizeof(struct span) * (unsigned long)spans.cap);
			if (spans.data == NULL)
				return ((struct spans){ 0 });
		}
		spans.data[spans.len++] = span;
	}
	return (spans);
}

struct ast			*gsx_ast_make_alias(struct span span, struct ast_atom *value) {
	struct ast	*ast = malloc(sizeof(struct ast) + sizeof(struct ast_alias));
	if (ast) {
		struct ast_alias	*alias = (struct ast_alias *)&ast[sizeof(struct ast)];
		if (alias == NULL)
			return (free(ast), NULL);
		alias->span = span;
		alias->value = value;

		ast->kind = AST_ALIAS;
		ast->data.alias = alias;
	}
	return (ast);
}

struct ast			*gsx_ast_make_atom_string(struct span span, const char *string) {
	struct ast	*ast = malloc(sizeof(struct ast) + sizeof(struct ast_atom));
	if (ast) {
		struct ast_atom	*atom = (struct ast_atom *)&ast[sizeof(struct ast)];
		atom->span = span;
		atom->is_string = true;
		atom->value.string = string;

		ast->kind = AST_ATOM;
		ast->data.atom = atom;
	}
	return (ast);
}

struct ast			*gsx_ast_make_atom_number(struct span span, long number) {
	struct ast	*ast = malloc(sizeof(struct ast) + sizeof(struct ast_atom));
	if (ast) {
		struct ast_atom	*atom = (struct ast_atom *)&ast[sizeof(struct ast)];
		atom->span = span;
		atom->is_string = false;
		atom->value.number = number;

		ast->kind = AST_ATOM;
		ast->data.atom = atom;
	}
	return (ast);
}

struct ast			*gsx_ast_make_call(struct span span, struct ast	**data, int len, int cap) {
	struct ast	*ast = malloc(sizeof(struct ast) + sizeof(struct ast_call));
	if (ast) {
		struct ast_call	*call = (struct ast_call *)&ast[sizeof(struct ast)];
		call->span = span;
		call->args.data = data;
		call->args.len = len;
		call->args.cap = cap;

		ast->kind = AST_CALL;
		ast->data.call = call;
	}
	return (ast);
}

struct ast			*gsx_parse_atom_string(struct context *ctx, struct span span) {
	struct string_builder	sb = { 0 };
	for (int i = 1; i < span.count - 1; i++) {
		char	c = ctx->source[span.index + i];
		string_builder_append_char(&sb, c);
	}
	string_builder_free(&sb);
	return (NULL);
}

struct ast			*gsx_parse_atom_number(struct context *ctx, struct span span) {
	long	num = 0, sgn = 1;

	if (ctx == NULL) return (NULL);
	for (int i = 0; i < span.count; i++) {
		char	digit = ctx->source[span.index + i];
		if (digit < '0' && digit > '9') break ;
		num = (num * 10l) + (long)(digit - '0');
	}
	return (gsx_ast_make_atom_number(span, num * sgn));
}

struct ast			*gsx_parse_call(struct context *ctx, struct spans spans) {
	if (ctx == NULL || spans.data == NULL || spans.len <= 0) return (NULL);
	struct span			proc_span = spans.data[0];
	struct definition	*proc_def = gsx_find_definition(ctx, proc_span);
	if (proc_def == NULL) {
		fprintf(stderr, FMT_99STATIC": "FMT_ERROR": Undefined procedure `%.*s`\n", proc_span.count, &ctx->source[proc_span.index]);
		return (NULL);
	}

	struct ast	**proc_data = NULL;
	if (proc_def->params_count > 0) {
		fprintf(stderr, FMT_99STATIC": Debug: Parsing call to procedure `%.*s`\n", proc_span.count, &ctx->source[proc_span.index]);
		proc_data = malloc(sizeof(struct ast *) * (unsigned long)proc_def->params_count); assert(proc_data != NULL);
		int	i = 0;
		while (i < proc_def->params_count && i < spans.len - 1) {
			i++;
		}
		if (i < proc_def->params_count) {
			fprintf(stderr, FMT_99STATIC": "FMT_ERROR": Insufficient arguments passed to procedure `%.*s`\n", proc_span.count, &ctx->source[proc_span.index]);
			return (NULL);
		}
	}
	return (NULL);
}

struct ast			*gsx_parse_alias(struct context *ctx, struct span span) {
	(void)ctx;
	(void)span;
	return (NULL);
}

struct ast			*gsx_parse(struct context *ctx, struct spans spans) {
	if (ctx == NULL || spans.data == NULL || spans.len == 0) return (NULL);

	struct span span = spans.data[0];
	if (gsx_match_span_atom_string(ctx, span))
		return (gsx_parse_atom_string(ctx, span));
	if (gsx_match_span_atom_number(ctx, span))
		return (gsx_parse_atom_number(ctx, span));
	if (gsx_match_span_call(ctx, span))
		return (gsx_parse_call(ctx, spans));
	return (gsx_parse_alias(ctx, span));
}

char				*gsx_interpret(struct context *ctx, struct segment root_segment) {
	if (ctx == NULL) return (NULL);

	struct span root_span = gsx_segment_span(ctx->source, root_segment);
	if (root_span.count == 0) return (NULL);

	struct spans root_spans = gsx_tokenize(ctx->source, root_span);
	if (root_spans.len == 0 || root_spans.data == NULL) return (NULL);

	struct ast	*root_ast = gsx_parse(ctx, root_spans);
	(void)root_ast;
	return (NULL);
}

void				gsx_register_definition(struct context *ctx, struct definition definition) {
	if (ctx == NULL) return ;

	if (ctx->definitions.len >= ctx->definitions.cap) {
		ctx->definitions.cap = ctx->definitions.cap > 0 ? ctx->definitions.cap * 2 : 8;
		struct definition *definitions = realloc(ctx->definitions.data, sizeof(struct definition) * (unsigned long)ctx->definitions.cap);
		if (definitions) ctx->definitions.data = definitions;
		else return ;
	}
	ctx->definitions.data[ctx->definitions.len++] = definition;
}

char				*gsx_builtin_print(struct context *ctx, void *args) {
	(void)ctx;
	const char	*value = (const char *)args;
	if (value == NULL)
		return (NULL);

	int	len = string_length(value);
	return (string_clone(value, len));
}

void				gsx_register_core_definitions(struct context *ctx) {
	if (ctx == NULL) return ;


	static struct parameter	params_print[] = {
		{ .kind = PARAMETER_VARIABLE, .data = { .name = "value" } },
	};

	static struct definition	def_print = {
		.span = "core::print",
		.params = params_print,
		.params_count = 2,
		
		.is_builtin = true,
		.address = {
			.builtin = (void *)gsx_builtin_print,
		},
	};
	gsx_register_definition(ctx, def_print);
	// const char			*span;
	// struct parameter		*params;
	// int					params_count;

	// union {
	// 	struct ast		*ast;
	// 	void			*builtin;
	// }					body;
	// bool				is_builtin;
}

int					main(void) {
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
	struct context ctx = { 0 };
	gsx_register_core_definitions(&ctx);

	while ((ent = readdir(dir))) {
		if (string_equals(ent->d_name, ".")) continue ;
		if (string_equals(ent->d_name, "..")) continue ;
		if (!string_suffix(ent->d_name, ".gsx")) continue ;

		char	path[1024] = { 0 };
		snprintf(path, 1024, "%s/%s", cwd, ent->d_name);
		ctx.source = file_load(path);
		if (ctx.source == NULL) {
			fprintf(stderr, "99static: Could not `file_load` at `%s` because of: %m\n", path);
			continue ;
		}

		struct segment *segments = gsx_segment(ctx.source);
		if (segments == NULL && errno) {
			fprintf(stderr, "99static: Could not `file_segment` at `%s` because of: %m\n", path);
			continue ;
		}

		fprintf(stderr, "99static: parsing `%s`\n", path);
		for (int i = 0; !segments[i].eof; i++) {
			struct segment	segment = segments[i];
			char			*output = (segment.comment ? gsx_interpret(&ctx, segment) : NULL);
			if (output == NULL) continue ;
		}
	}

	closedir(dir);
	free((void *)cwd);
	return (0);
}
