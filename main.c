#include <fcntl.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/stat.h>

/* IDEA(xenobas):
 * Move from registered definition based arguments counting, To a stack that gets arguments and they get popped by call, this way arguments can be properly determined.
 * This requires two machines to solve it, but should provide better interface for interpreting?
*/

/* REFERENCE(xenobas): [ANSI Escape Codes](https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797)*/
#define TERMINAL_COLOR_RED "\x1b[1;31m"
#define TERMINAL_COLOR_GREEN "\x1b[1;32m"
#define TERMINAL_COLOR_WHITE "\x1b[1;37m"
#define TERMINAL_COLOR_YELLOW "\x1b[1;33m"
#define TERMINAL_STYLE_RESET "\x1b[0m"

#define TERMINAL_NOTICE_SUCCESS TERMINAL_COLOR_GREEN "Success" TERMINAL_STYLE_RESET
#define TERMINAL_NOTICE_ERROR TERMINAL_COLOR_RED "Error" TERMINAL_STYLE_RESET
#define TERMINAL_NOTICE_TODO TERMINAL_COLOR_YELLOW "TODO" TERMINAL_STYLE_RESET
#define TERMINAL_NOTICE_HINT TERMINAL_COLOR_WHITE "Hint" TERMINAL_STYLE_RESET
#define TERMINAL_NOTICE_USAGE TERMINAL_COLOR_WHITE "Usage" TERMINAL_STYLE_RESET

void	mem_copy(void *dest, void *source, size_t nbytes) {
	if (dest == NULL || source == NULL || nbytes == 0) return ;
	for (size_t i = 0; i < nbytes; i++)
		((char *)dest)[i] = ((char *)source)[i];
}

void*	mem_clone(void *source, size_t nbytes) {
	void*	clone = malloc(nbytes);
	mem_copy(clone, source, nbytes);
	return (clone);
}

int		cstr_length(const char* str) {
	int	length = 0;

	if (str == NULL) return (0);
	while (str[length])
		length++;
	return (length);
}

struct sview {
	char	*data;
	size_t	len;
};

int				sview_index(struct sview string, const char* to_find) {
	int	length_to_find = cstr_length(to_find);
	if (to_find == NULL || length_to_find == 0) return (-1);

	for (int i = 0; i < (int)string.len; ++i) {
		int		j = 0;
		bool	match = true;
		while (j < length_to_find && j + i < (int)string.len && match) {
			match = (string.data[i + j] == to_find[j]);
			j++;
		}
		if (match && j == length_to_find) return (i);
	}
	return (-1);
}

int				sview_index_sview(struct sview string, struct sview to_find) {
	if (to_find.data == NULL || to_find.len == 0) return (-1);

	for (int i = 0; i < (int)string.len; ++i) {
		int		j = 0;
		bool	match = true;
		while ((size_t)j < to_find.len && j + (size_t)i < string.len && match) {
			match = (string.data[i + j] == to_find.data[j]);
			j++;
		}
		if (match && (size_t)j == to_find.len) return (i);
	}
	return (-1);
}

struct sview	sview_make_cstr(char* cstr) {
	return ((struct sview){ cstr, (size_t)cstr_length(cstr) });
}

struct sview	sview_make_cstr_const(const char* cstr) {
	return ((struct sview){ (char*)cstr, (size_t)cstr_length(cstr) });
}

struct sview	sview_make(char* data, size_t len) {
	return ((struct sview){ data, len });
}

struct sview	sview_drop(struct sview string, int count) {
	if (count < 0) count = 0;
	if (count >= (int)string.len) return ((struct sview){ NULL, 0ul });
	else return ((struct sview){ &string.data[count], (int)string.len - count });
}

struct sview	sview_take(struct sview string, int count) {
	if (count >= (int)string.len) count = (int)string.len;
	if (count <= 0) return ((struct sview){ NULL, 0ul });
	else return ((struct sview){ string.data, count });
}

struct sview	sview_drop_take(struct sview string, int drop, int take) {
	return (sview_take(sview_drop(string, drop), take));
}

bool			sview_equals_cstr(struct sview lhs, const char* rhs) {
	return (lhs.len == (size_t)cstr_length(rhs) && sview_index(lhs, rhs) == 0);
}

bool			sview_equals(struct sview lhs, struct sview rhs) {
	return (lhs.len == rhs.len && sview_index_sview(lhs, rhs) == 0);
}

struct sview	sview_trim(struct sview string, const char *charset) {
	if (charset == NULL || string.data == NULL || (int)string.len <= 0) return (string);
	int				drop_count = 0;
	while (drop_count < (int)string.len) {
		bool	match = false;
		for (int i = 0; charset[i] && !match; i++)
			match = (charset[i] == string.data[drop_count]);
		if (!match) break ;
		drop_count++;
	}
	struct sview	trim_left = sview_drop(string, drop_count);

	int				take_count = trim_left.len;
	while (take_count > 0) {
		bool	match = false;
		for (int i = 0; charset[i] && !match; ++i)
			match = (charset[i] == trim_left.data[take_count - 1]);
		if (!match) break ;
		take_count--;
	}
	return (sview_take(trim_left, take_count));
}

bool			sview_prefix(struct sview string, const char* prefix) {
	return (sview_index(string, prefix) == 0);
}

bool			sview_suffix(struct sview string, const char* suffix) {
	int	length_suffix = cstr_length(suffix);
	if ((int)string.len < length_suffix) return (false);
	return (sview_index(string, suffix) == ((int)string.len - length_suffix));
}

char*	cstr_clone_len(char *str, size_t len) {
	if (str == NULL) return (NULL);
	char*	clone = malloc(sizeof(char) * (len + 1));
	if (clone != NULL) {
		mem_copy(clone, str, sizeof(char) * len);
		clone[len] = '\0';
	}
	return (clone);
}

char*	cstr_clone(char *str) {
	return (cstr_clone_len(str, cstr_length(str)));
}

char*	cstr_clone_sview(struct sview view) {
	return (cstr_clone_len(view.data, view.len));
}

int		cstr_index(const char* str, const char* to_find) {
	struct sview	vstr = sview_make_cstr_const(str);
	return (sview_index(vstr, to_find));
}

bool	cstr_equals(const char *lhs, const char *rhs) {
	return (cstr_length(lhs) == cstr_length(rhs) && cstr_index(lhs, rhs) == 0);
}

bool	cstr_contains(const char *str, const char *to_find) {
	return (cstr_index(str, to_find) >= 0);
}

bool	cstr_prefix(const char* str, const char* prefix) {
	struct sview	vstr = sview_make_cstr_const(str);
	return (sview_prefix(vstr, prefix));
}

bool	cstr_suffix(const char* str, const char* suffix) {
	struct sview	vstr = sview_make_cstr_const(str);
	return (sview_suffix(vstr, suffix));
}

struct dynamic_array {
	void	*items;
	size_t	len;
	size_t	cap;
	size_t	unit;
};

#define da_at(TYPE, DA, INDEX) (((TYPE *)((DA)->items))[INDEX])

struct dynamic_array	da_make(size_t unit) {
	struct dynamic_array	da;

	da.items = NULL;
	da.len = 0ul;
	da.cap = 0ul;
	da.unit = unit;
	return (da);
}

void					da_free(struct dynamic_array *da) {
	if (da == NULL) return ;
	free(da->items);

	da->items = NULL;
	da->len = 0ul;
	da->cap = 0ul;
}

void					da_clear(struct dynamic_array *da) {
	da->len = 0ul;
}

void					da_expand(struct dynamic_array *da) {
	if (da == NULL) return ;
	if (da->len >= da->cap) {
		size_t	new_cap = (da->cap <= 0ul) ? 8ul : (da->cap * 2ul);
		void	*new_items = realloc(da->items, da->unit * new_cap);
		assert(new_items != NULL && "Could not reallocate new items");

		da->cap = new_cap;
		da->items = new_items;
	}
}

void					da_append(struct dynamic_array *da, void *item) {
	if (da == NULL) return ;

	da_expand(da);
	mem_copy(&((char *)da->items)[da->len * da->unit], item, da->unit);
	da->len++;
}

struct string_builder {
	struct dynamic_array	bytes;
};

struct string_builder	string_builder_make(void) {
	struct string_builder	sb = {
		.bytes = da_make(sizeof(char)),
	};
	return (sb);
}

void					string_builder_clear(struct string_builder* sb) {
	if (sb == NULL) return ;
	da_clear(&sb->bytes);
}

void					string_builder_destroy(struct string_builder* sb) {
	if (sb == NULL) return ;
	da_free(&sb->bytes);
	*sb = (struct string_builder){ 0 };
}

void					string_builder_write_cstr(struct string_builder* sb, const char* str) {
	if (sb == NULL || str == NULL) return ;
	for (size_t i = 0; str[i]; ++i)
		da_append(&sb->bytes, (void *)&str[i]);
}

void					string_builder_write_sview(struct string_builder* sb, struct sview string) {
	if (sb == NULL || string.data == NULL) return ;
	for (size_t i = 0; i < string.len; ++i)
		da_append(&sb->bytes, (void *)&string.data[i]);
}

void					string_builder_write_char(struct string_builder* sb, char c) {
	if (sb == NULL || c == '\0') return ;
	da_append(&sb->bytes, (void *)&c);
}

char*					string_builder_as_cstr(struct string_builder sb) {
	return (cstr_clone_len(sb.bytes.items, sb.bytes.len));
}

struct sview			string_builder_as_sview(struct string_builder sb) {
	char*	data = string_builder_as_cstr(sb);
	return ((struct sview){ .data = data, .len = cstr_length(data) });
}

int				os_filedes_size(int fd) {
	struct stat	buf;

	if (fd < 0) return (-1);
	if (fstat(fd, &buf)) return (-1);
	return ((int)buf.st_size);
}

int				os_file_size(const char* path) {
	struct stat	buf;

	if (path == NULL) return (-1);
	if (stat(path, &buf)) return (-1);
	return ((int)buf.st_size);
}

struct sview	os_file_view(const char* path) {
	int	fd = open(path, O_RDONLY);
	if (fd < 0) return ((struct sview){ NULL, 0ul });
	int	size = os_filedes_size(fd);
	if (size < 0) return ((struct sview){ NULL, 0ul });

	char	*data = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	if (data == NULL) return ((struct sview){ NULL, 0ul });
	return ((struct sview){ data, size });
}

bool			os_file_dump(const char* path, struct sview bytes) {
	if (path == NULL || bytes.data == NULL || bytes.len == 0ul)
		return (false);

	int					fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd < 0) return (false);

	bool			ok = true;
	const size_t	dump_block_size_max = 8192ul;
	for (size_t	dump_cursor = 0ul; dump_cursor < bytes.len; dump_cursor += dump_block_size_max) {
		size_t		dump_block_size = dump_block_size_max;
		if (dump_cursor + dump_block_size >= bytes.len)
			dump_block_size = bytes.len - dump_cursor;
		if (write(fd, &bytes.data[dump_cursor], dump_block_size) < 0) {
			ok = false;
			break ;
		}
	}
	close(fd);
	return (ok);
}

const char*	program_name = NULL;
int			program_return = 0;

struct gsx_section {
	struct sview	text;
	bool			is_comment;
	bool			is_expression;
	bool			is_terminated;
};

bool	gsx_load_sections(struct sview string, struct dynamic_array* sections) {
	if (sections == NULL) return (false);

	int		i = 0;
	bool	ok = true;
	while (i < (int)string.len) {
		bool				is_expression = false;
		bool				is_terminated = false;

		int					len = 0;
		struct sview		view_prefix = sview_drop(string, i);
		bool				is_comment = sview_prefix(view_prefix, "<!--");
		while (len < (int)view_prefix.len) {
			struct sview	view_suffix = sview_drop(view_prefix, len);
			is_terminated = sview_prefix(view_suffix, is_comment ? "-->" : "<!--");
			if (is_terminated) {
				if (is_comment) len += cstr_length("-->");
				break ;
			}
			len++;
		}
		struct sview		text = sview_drop_take(string, i, len);
		if (is_comment) {
			if (!is_terminated) {
				/* TODO(xenobas): Show location of unterminated comment. */
				ok = false;
			}
			struct sview	comment = sview_trim(sview_drop(text, cstr_length("<!--")), " \t");
			is_expression = (is_terminated && sview_prefix(comment, "gsx:"));
		}
		struct gsx_section	section = {
			.text			= text,
			.is_comment		= is_comment,
			.is_expression	= is_expression,
			.is_terminated	= is_terminated,
		};
		da_append(sections, &section);
		i += len;
	}
	return (ok);
}

enum gsx_token_kind {
	GSX_TOKEN_INVALID,
	GSX_TOKEN_IDENT,
	GSX_TOKEN_CALL,
	GSX_TOKEN_CHAR,
	GSX_TOKEN_STRING,
	GSX_TOKEN_INTEGER,
	GSX_TOKEN_ARRAY_OPEN,
	GSX_TOKEN_ARRAY_CLOSE,
	GSX_TOKEN_ARRAY_SEPARATOR,
	GSX_TOKEN_PARAMS_DELIMITER,
	GSX_TOKEN_DESTRUCTURE_SEPARATOR,
};

struct gsx_token {
	struct sview		text;
	enum gsx_token_kind	kind;
};

int		gsx_token_kind_fprint(FILE* file, enum gsx_token_kind kind) {
	switch (kind) {
		case GSX_TOKEN_INTEGER: return (fprintf(file, "integer"));
		case GSX_TOKEN_STRING: return (fprintf(file, "string"));
		case GSX_TOKEN_INVALID: return (fprintf(file, "invalid"));
		case GSX_TOKEN_IDENT: return (fprintf(file, "identifier"));
		case GSX_TOKEN_CALL: return (fprintf(file, "function call"));
		case GSX_TOKEN_CHAR: return (fprintf(file, "character"));
		case GSX_TOKEN_ARRAY_OPEN: return (fprintf(file, "array open"));
		case GSX_TOKEN_ARRAY_CLOSE: return (fprintf(file, "array close"));
		case GSX_TOKEN_ARRAY_SEPARATOR: return (fprintf(file, "array separator"));
		case GSX_TOKEN_PARAMS_DELIMITER: return (fprintf(file, "parameters delimiter"));
		case GSX_TOKEN_DESTRUCTURE_SEPARATOR: return (fprintf(file, "destructure separator"));
		default: return (fprintf(file, "unknown (%d)", kind));
	}
}

int		gsx_token_fprint(FILE* file, struct gsx_token token) {
	int	ret = 0;
	ret += fprintf(file, "token{ `%.*s` ", (int)token.text.len, token.text.data);
	ret += gsx_token_kind_fprint(file, token.kind);
	ret += fprintf(file, " }");
	return (ret);
}

bool	gsx_token_is_expression(enum gsx_token_kind kind) {
	if (kind == GSX_TOKEN_CALL) return (true);
	if (kind == GSX_TOKEN_STRING) return (true);
	if (kind == GSX_TOKEN_INTEGER) return (true);
	if (kind == GSX_TOKEN_IDENT) return (true);
	return (false);
}

struct gsx_lexer {
	struct sview	text;
	size_t			index;
	size_t			offset;
	bool			ok;
};

bool				gsx_is_digit(char c) {
	return (c >= '0' && c <= '9');
}

bool				gsx_is_ident_prefix(char c) {
	if (c >= 'a' && c <= 'z') return (true);
	if (c >= 'A' && c <= 'Z') return (true);
	if (c == '_') return (true);
	return (false);
}

bool				gsx_is_ident_infix(char c) {
	if (gsx_is_digit(c)) return (true);
	if (c == ':' || c == '!') return (true);
	if (gsx_is_ident_prefix(c)) return (true);
	return (false);
}

bool				gsx_lexer_eof(struct gsx_lexer* lexer) {
	return (lexer == NULL || lexer->index >= lexer->text.len);
}

char				gsx_lexer_peek(struct gsx_lexer* lexer) {
	if (gsx_lexer_eof(lexer)) return ('\0');
	return (lexer->text.data[lexer->index]);
}

char				gsx_lexer_next(struct gsx_lexer* lexer) {
	char	peek = gsx_lexer_peek(lexer);
	if (peek != '\0') lexer->index++;
	return (peek);
}

struct sview		gsx_lexer_peek_view(struct gsx_lexer* lexer) {
	if (lexer == NULL || lexer->offset >= lexer->index) return ((struct sview){ NULL, 0ul });
	size_t				drop = lexer->offset,
						take = lexer->index - lexer->offset;
	return (sview_drop_take(lexer->text, drop, take));
}

struct gsx_token	gsx_lexer_next_token(struct gsx_lexer* lexer, enum gsx_token_kind kind) {
	if (lexer == NULL || lexer->offset >= lexer->index) return ((struct gsx_token){ 0 });
	size_t				drop = lexer->offset,
						take = lexer->index - lexer->offset;
	struct sview		text = sview_drop_take(lexer->text, drop, take);
	struct gsx_token	token = { .text = text, .kind = kind };
	lexer->offset = lexer->index;
	return (token);
}

bool				gsx_lexer_tokenize(struct sview text, struct dynamic_array* tokens) {
	struct gsx_lexer	lexer = { .text = text, .index = 0ul, .offset = 0ul, .ok = true };
	while (!gsx_lexer_eof(&lexer)) {
		while (!gsx_lexer_eof(&lexer)) {
			char	letter = gsx_lexer_peek(&lexer);
			if (letter != ' ' && letter != '\t') break ;
			gsx_lexer_next(&lexer);
		}
		gsx_lexer_next_token(&lexer, GSX_TOKEN_INVALID);

		struct gsx_token	token = { 0 };
		char				c = gsx_lexer_peek(&lexer);
		if (c == '\0') break ;
		if (c == '=') {
			gsx_lexer_next(&lexer);
			token = gsx_lexer_next_token(&lexer, GSX_TOKEN_PARAMS_DELIMITER);
		} else if (c == ':') {
			gsx_lexer_next(&lexer);
			token = gsx_lexer_next_token(&lexer, GSX_TOKEN_DESTRUCTURE_SEPARATOR);
		} else if (c == ',') {
			gsx_lexer_next(&lexer);
			token = gsx_lexer_next_token(&lexer, GSX_TOKEN_ARRAY_SEPARATOR);
		} else if (c == '[') {
			gsx_lexer_next(&lexer);
			token = gsx_lexer_next_token(&lexer, GSX_TOKEN_ARRAY_OPEN);
		} else if (c == ']') {
			gsx_lexer_next(&lexer);
			token = gsx_lexer_next_token(&lexer, GSX_TOKEN_ARRAY_CLOSE);
		} else if (c == '\'') {
			gsx_lexer_next(&lexer);
			char	letter = '\0';
			while (!gsx_lexer_eof(&lexer)) {
				if (letter == '\'') break ;
				gsx_lexer_next(&lexer);
			}
			if (letter == '\'') gsx_lexer_next(&lexer);
			token = gsx_lexer_next_token(&lexer, GSX_TOKEN_CHAR);
		} else if (c == '"') {
			gsx_lexer_next(&lexer);
			char	letter = '\0';
			while (!gsx_lexer_eof(&lexer)) {
				letter = gsx_lexer_peek(&lexer);
				if (letter == '"') break ;
				gsx_lexer_next(&lexer);
			}
			if (letter == '"') gsx_lexer_next(&lexer);
			token = gsx_lexer_next_token(&lexer, GSX_TOKEN_STRING);
		} else if (gsx_is_digit(c)) {
			while (!gsx_lexer_eof(&lexer)) {
				char	digit = gsx_lexer_peek(&lexer);
				if (!gsx_is_digit(digit)) break ;
				gsx_lexer_next(&lexer);
			}
			token = gsx_lexer_next_token(&lexer, GSX_TOKEN_INTEGER);
		} else if (gsx_is_ident_prefix(c)) {
			char				letter = '\0';
			while (!gsx_lexer_eof(&lexer)) {
				if (!gsx_is_ident_infix(gsx_lexer_peek(&lexer))) break ;
				letter = gsx_lexer_next(&lexer);
			}
			enum gsx_token_kind	kind = GSX_TOKEN_IDENT;
			if (letter == '!')
				kind = GSX_TOKEN_CALL;
			token = gsx_lexer_next_token(&lexer, kind);
		} else {
			gsx_lexer_next(&lexer);
			token = gsx_lexer_next_token(&lexer, GSX_TOKEN_INVALID);
			lexer.ok = false;
		}
		da_append(tokens, &token);
	}
	return (lexer.ok);
}

enum gsx_ast_kind {
	GSX_AST_INVALID,
	GSX_AST_CALL,
	GSX_AST_IDENT,
	GSX_AST_LITERAL,
	GSX_AST_DEFINE,
};

struct gsx_ast_call {
	struct gsx_token		name;
	struct dynamic_array	args;
};

struct gsx_ast_ident {
	struct gsx_token	name;
};

struct gsx_ast_literal {
	struct gsx_token	token;
};

enum gsx_ast_define_param_kind {
	GSX_AST_DEFINE_PARAM_INVALID,

	GSX_AST_DEFINE_PARAM_IDENT,
	GSX_AST_DEFINE_PARAM_PATTERN,
	GSX_AST_DEFINE_PARAM_DESTRUCTURE,
};

struct gsx_ast_destructure {
	struct dynamic_array	parts;
};

struct gsx_ast_define_param {
	enum gsx_ast_define_param_kind	kind;
	struct gsx_ast_destructure*	destructure;
};

struct gsx_ast_define {
	struct gsx_token		token;
	struct gsx_token		name;
	struct gsx_token		delimiter;
	struct dynamic_array	params;
	struct gsx_ast*			value;
};

struct gsx_ast {
	enum gsx_ast_kind		kind;
	union {
		struct gsx_ast_call*		call;
		struct gsx_ast_ident*		ident;
		struct gsx_ast_literal*		literal;
		struct gsx_ast_define*		define;
	}					data;
};

#ifdef DEBUG_AST_CALLS
#define fprintf_ast_call(NAME, TOKEN) fprintf(stdout, NAME " :: `%.*s`\n", (int)((TOKEN).text.len), (TOKEN).text.data);
#else
#define fprintf_ast_call(NAME, TOKEN) ((void)(NAME), (void)(TOKEN))
#endif

void				gsx_ast_print(FILE* file, struct gsx_ast *node, int indent) {
	if (node == NULL) return ;

	for (int i = 0; i < indent; ++i) fprintf(file, "\t");
	switch (node->kind) {
		case GSX_AST_CALL: {
			struct gsx_ast_call*	call = node->data.call;
			fprintf(file, "- call `%.*s`\n", (int)call->name.text.len, call->name.text.data);
			for (size_t i = 0; i < call->args.len; ++i) {
				struct gsx_ast*	node = da_at(struct gsx_ast*, &call->args, i);
				gsx_ast_print(file, node, indent + 1);
			}
		} break ;
		case GSX_AST_DEFINE: {
			struct gsx_ast_define*	define = node->data.define;
			struct gsx_token		name = define->name;
			fprintf(file, "- call define `%.*s`\n", (int)name.text.len, name.text.data);
		} break ;
		case GSX_AST_LITERAL: {
			struct gsx_ast_literal*	literal = node->data.literal;
			fprintf(file, "- literal `%.*s`\n", (int)literal->token.text.len, literal->token.text.data);
		} break ;
		case GSX_AST_IDENT: {
			struct gsx_ast_ident*	ident = node->data.ident;
			fprintf(file, "- ident `%.*s`\n", (int)ident->name.text.len, ident->name.text.data);
		} break ;
		case GSX_AST_INVALID: {
			fprintf(file, "- invalid\n");
		} break ;
		default: {
			fprintf(file, "- unknown\n");
		} break ;
	}
}

void				gsx_ast_free_define_param(struct gsx_ast_define_param* param) {
	(void)param;
}

void				gsx_ast_free(struct gsx_ast* node) {
	if (node == NULL) return ;
	switch (node->kind) {
		case GSX_AST_CALL: {
			struct gsx_ast_call*	call = node->data.call;
			for (size_t i = 0; i < call->args.len; ++i) {
				struct gsx_ast*	node = da_at(struct gsx_ast*, &call->args, i);
				gsx_ast_free(node);
			}
			da_free(&call->args);
		} break ;
		case GSX_AST_DEFINE: {
			fprintf(stderr, "%s:\t" TERMINAL_NOTICE_TODO ": gsx_ast_free(kind = GSX_AST_DEFINE)\n", program_name);
		} break ;
		case GSX_AST_LITERAL: {
		} break ;
		case GSX_AST_IDENT: {
		} break ;
		case GSX_AST_INVALID:
		default: { } break ;
	}
	free(node);
}

bool				gsx_ast_is_value_void(struct gsx_ast* ast) {
	if (ast != NULL && ast->kind == GSX_AST_CALL) return (false);
	if (ast != NULL && ast->kind == GSX_AST_IDENT) return (false);
	if (ast != NULL && ast->kind == GSX_AST_LITERAL) return (false);
	return (true);
}

enum gsx_box_kind {
	GSX_BOX_INVALID,

	GSX_BOX_CHAR,
	GSX_BOX_STRING,
	GSX_BOX_INTEGER,
	GSX_BOX_ARRAY,
};

struct gsx_box {
	enum gsx_box_kind	kind;
	union {
		char					character;
		long					integer;
		char*					str;
		struct dynamic_array	array;
	}					data;
};

typedef struct gsx_box*	(*gsx_definition_internal)(struct dynamic_array*);

struct gsx_box*	gsx_box_new_char(char character) {
	struct gsx_box*	box = malloc(sizeof(struct gsx_box));
	if (box) {
		box->kind = GSX_BOX_CHAR;
		box->data.character = character;
	}
	return (box);
}

struct gsx_box*	gsx_box_new_integer(long integer) {
	struct gsx_box*	box = malloc(sizeof(struct gsx_box));
	if (box) {
		box->kind = GSX_BOX_INTEGER;
		box->data.integer = integer;
	}
	return (box);
}

struct gsx_box*	gsx_box_new_string(char* str) {
	struct gsx_box*	box = malloc(sizeof(struct gsx_box) + (cstr_length(str) + 1));
	if (box) {
		box->kind = GSX_BOX_STRING;
		box->data.str = &((char *)box)[sizeof(struct gsx_box)];
		mem_copy(box->data.str, str, cstr_length(str) + 1);
	}
	return (box);
}

struct gsx_box*	gsx_box_new_sview(struct sview string) {
	struct gsx_box*	box = malloc(sizeof(struct gsx_box) + (size_t)(string.len + 1u));
	if (box) {
		box->kind = GSX_BOX_STRING;
		box->data.str = &((char *)box)[sizeof(struct gsx_box)];
		mem_copy(box->data.str, string.data, string.len);
		box->data.str[string.len] = '\0';
	}
	return (box);
}

struct gsx_box*	gsx_box_new_array(struct dynamic_array array) {
	struct gsx_box*	box = malloc(sizeof(struct gsx_box));
	if (box) {
		box->kind = GSX_BOX_ARRAY;
		box->data.array = array;
	}
	return (box);
}

void			gsx_box_free(struct gsx_box* box) {
	if (box != NULL) switch (box->kind) {
		case GSX_BOX_ARRAY: {
			struct dynamic_array*	array = &box->data.array;
			for (size_t i = 0ul; i < array->len; ++i) {
				struct gsx_box	*box = da_at(struct gsx_box*, array, i);
				gsx_box_free(box);
			}
			da_free(array);
		} break ;
		case GSX_BOX_STRING:
		case GSX_BOX_CHAR:
		case GSX_BOX_INTEGER:
		case GSX_BOX_INVALID:
		default: { } break;
	}
	free(box);
}

enum gsx_definition_kind {
	GSX_DEFINITION_INVALID,

	GSX_DEFINITION_VARIABLE,
	GSX_DEFINITION_FUNCTION,
	GSX_DEFINITION_INTERNAL,
};

struct gsx_definition {
	struct sview				name;
	struct dynamic_array		params;
	enum gsx_definition_kind	kind;
	union {
		struct gsx_ast*			ast;
		gsx_definition_internal	internal;
	}							data;
};

struct gsx_definition*	gsx_definition_get(const struct dynamic_array* definitions, struct sview name) {
	if (definitions == NULL) return (NULL);
	for (size_t i = 0; i < definitions->len; ++i) {
		struct gsx_definition	*definition = &da_at(struct gsx_definition, definitions, i);
		if (sview_equals(definition->name, name))
			return (definition);
	}
	return (NULL);
}

struct gsx_parser {
	const struct dynamic_array*	definitions;
	const struct dynamic_array*	tokens;
	struct gsx_token			current;
	size_t						index;
	bool						ok;
};

bool				gsx_parser_eof(struct gsx_parser* parser) {
	return (parser == NULL || parser->index >= parser->tokens->len);
}

struct gsx_token	gsx_parser_peek(struct gsx_parser* parser) {
	if (gsx_parser_eof(parser)) return ((struct gsx_token){ 0 });
	struct gsx_token	token = da_at(struct gsx_token, parser->tokens, parser->index);
	return (token);
}

struct gsx_token	gsx_parser_next(struct gsx_parser* parser) {
	if (gsx_parser_eof(parser)) return ((struct gsx_token){ 0 });
	parser->current = gsx_parser_peek(parser);
	return (parser->index++, parser->current);
}

bool				gsx_parser_accept(struct gsx_parser* parser, enum gsx_token_kind kind) {
	struct gsx_token	token = gsx_parser_peek(parser);
	if (token.kind != GSX_TOKEN_INVALID && token.kind == kind)
		return (gsx_parser_next(parser), true);
	return (false);
}

bool				gsx_parser_accept_any(struct gsx_parser* parser, enum gsx_token_kind* kinds, size_t kinds_count) {
	struct gsx_token	token = gsx_parser_peek(parser);

	if (kinds == NULL || kinds_count == 0) return (false);
	for (size_t i = 0; i < kinds_count; ++i) {
		enum gsx_token_kind	kind = kinds[i];
		if (token.kind != GSX_TOKEN_INVALID && token.kind == kind)
			return (gsx_parser_next(parser), true);
	}
	return (false);
}

bool				gsx_parser_expect(struct gsx_parser* parser, enum gsx_token_kind kind) {
	if (!gsx_parser_accept(parser, kind)) {
		fprintf(stderr, "%s:\t"TERMINAL_NOTICE_ERROR": Expected `", program_name);
		gsx_token_kind_fprint(stderr, kind);
		fprintf(stderr, "`, got ");
		gsx_token_fprint(stderr, gsx_parser_peek(parser));
		fprintf(stderr, " instead.\n");
		return (parser->ok = false);
	}
	return (true);
}

bool				gsx_parser_expect_any(struct gsx_parser* parser, enum gsx_token_kind* kinds, size_t kinds_count) {
	if (!gsx_parser_accept_any(parser, kinds, kinds_count)) {
		fprintf(stderr, "%s:\t"TERMINAL_NOTICE_ERROR": Expected ", program_name);
		for (size_t i = 0; i < kinds_count; ++i) {
			enum gsx_token_kind	kind = kinds[i];
			fprintf(stderr, "`");
			gsx_token_kind_fprint(stderr, kind);
			fprintf(stderr, "`");
			if (i + 1 < kinds_count)
				fprintf(stderr, " | ");
		}
		fprintf(stderr, ", got ");
		struct gsx_token	token = gsx_parser_peek(parser);
		gsx_token_fprint(stderr, token);
		fprintf(stderr, " instead.\n");
		return (parser->ok = false);
	}
	return (true);
}

size_t				gsx_parser_remaining(struct gsx_parser* parser) {
	if (gsx_parser_eof(parser)) return (0ul);
	return (parser->tokens->len - parser->index);
}

struct gsx_ast*		gsx_ast_parse_ident(struct gsx_parser* parser) {
	if (!gsx_parser_expect(parser, GSX_TOKEN_IDENT)) return (NULL);
	struct gsx_token		name = parser->current;
	fprintf_ast_call("gsx_ast_parse_ident", name);
	struct gsx_ast*			node = malloc(sizeof(struct gsx_ast) + sizeof(struct gsx_ast_ident));
	struct gsx_ast_ident*	ident = (struct gsx_ast_ident*)&((char *)node)[sizeof(struct gsx_ast)];
	if (node != NULL) {
		ident->name = name;
		node->kind = GSX_AST_IDENT;
		node->data.ident = ident;
	}
	return (node);
}

struct gsx_ast*		gsx_ast_parse_literal(struct gsx_parser* parser) {
	static enum gsx_token_kind	lit_kinds[] = { GSX_TOKEN_INTEGER, GSX_TOKEN_STRING, GSX_TOKEN_CHAR };
	const size_t				lit_kinds_count = sizeof(lit_kinds) / sizeof(lit_kinds[0]);
	if (!gsx_parser_expect_any(parser, lit_kinds, lit_kinds_count)) return (NULL);
	struct gsx_token		token = parser->current;
	fprintf_ast_call("gsx_ast_parse_literal", token);
	/* fprintf(stderr, "gsx_ast_parse_literal :: `%.*s`\n", (int)token.text.len, token.text.data); */
	struct gsx_ast*			node = malloc(sizeof(struct gsx_ast) + sizeof(struct gsx_ast_literal));
	struct gsx_ast_literal*	literal = (struct gsx_ast_literal*)&((char *)node)[sizeof(struct gsx_ast)];
	if (node != NULL) {
		literal->token = token;
		node->kind = GSX_AST_LITERAL;
		node->data.literal = literal;
	}
	return (node);
}

struct gsx_ast*		gsx_ast_parse_define_param(struct gsx_parser* parser) {
	static enum gsx_token_kind	define_param_kinds[] = {
		GSX_TOKEN_CALL, /* NOTE(xenobas): Value */
		GSX_TOKEN_IDENT, /* NOTE(xenobas): Named parameter or Value */
		GSX_TOKEN_ARRAY_OPEN, /* NOTE(xenobas): Destructure pattern matching */
		GSX_TOKEN_INTEGER, GSX_TOKEN_STRING, GSX_TOKEN_CHAR, /* NOTE(xenobas): Literal pattern matching */
	};
	const size_t				define_param_kinds_count = sizeof(define_param_kinds) / sizeof(define_param_kinds[0]);
	if (!gsx_parser_expect_any(parser, define_param_kinds, define_param_kinds_count)) return (NULL);
	struct gsx_token		token = parser->current;
	fprintf_ast_call("gsx_ast_parse_define_param", token);
	if (token.kind == GSX_TOKEN_CALL) { /* NOTE(xenobas): Value */
	}
	return (NULL);
}

struct gsx_ast*		gsx_ast_parse_define(struct gsx_parser* parser, struct gsx_definition* definition) {
	if (parser == NULL || definition == NULL) return (NULL);

	struct gsx_token					token = parser->current;
	fprintf_ast_call("gsx_ast_parse_define", token);
	if (token.kind != GSX_TOKEN_CALL) return (NULL);

	struct sview						text = sview_take(token.text, token.text.len - 1ul); /* EXAMPLE(xenobas): core::define */
	if (!sview_equals_cstr(text, "core::define")) return (NULL);
	if (!gsx_parser_expect(parser, GSX_TOKEN_IDENT)) return (NULL);
	struct gsx_token					name = parser->current; /* EXAMPLE(xenobas): vendor::my_method */
	struct dynamic_array				params  = da_make(sizeof(struct gsx_ast_define_param*));
	while (!gsx_parser_eof(parser)) {
		struct gsx_token				token = gsx_parser_peek(parser);
		if (token.kind == GSX_TOKEN_PARAMS_DELIMITER) break ;
		struct gsx_ast*	param = gsx_ast_parse_define_param(parser);
		if (param == NULL) break ;
		da_append(&params, &param);
	}
	(void)name;
	if (gsx_parser_accept(parser, GSX_TOKEN_PARAMS_DELIMITER)) { /* NOTE(xenobas): Function definition */
	} else if (params.len == 1u) { /* NOTE(xenobas): Variable definition */
	} else {
		if (params.len == 0u) { /* NOTE(xenobas): Empty definition */
		} else { /* NOTE(xenobas): Overflowing definition, did they mean to make a function ? */
		}
		for (size_t i = 0ul; i < params.len; ++i) {
			struct gsx_ast_define_param*	param = da_at(struct gsx_ast_define_param*, &params, i);
			gsx_ast_free_define_param(param);
		}
		da_free(&params);
		return (NULL);
	}
	return (fprintf(stderr, "%s:\t" TERMINAL_NOTICE_TODO ": gsx_ast_parse_define\n", program_name), NULL);
}

struct gsx_ast*		gsx_ast_parse_call(struct gsx_parser* parser) {
	static enum gsx_token_kind	arg_kinds[] = { GSX_TOKEN_CALL, GSX_TOKEN_IDENT, GSX_TOKEN_INTEGER, GSX_TOKEN_STRING };
	const size_t				arg_kinds_count = sizeof(arg_kinds) / sizeof(arg_kinds[0]);
	if (!gsx_parser_expect(parser, GSX_TOKEN_CALL)) return (NULL);

	struct gsx_token		name = parser->current;
	fprintf_ast_call("gsx_ast_parse_call", name);
	struct sview			text = sview_take(name.text, name.text.len - 1ul);
	struct gsx_definition*	definition = gsx_definition_get(parser->definitions, text);
	if (definition == NULL) {
		fprintf(stderr, "%s:\t"TERMINAL_NOTICE_ERROR": Undeclared function `%.*s` was called!\n", program_name, (int)text.len, text.data);
		return (NULL);
	}
	if (sview_equals_cstr(text, "core::define")) {
		return (gsx_ast_parse_define(parser, definition));
	}

	struct dynamic_array	args = da_make(sizeof(struct gsx_ast*));
	size_t					args_count = definition->params.len;
	while (!gsx_parser_eof(parser) && args.len < args_count) {
		struct gsx_token	token = gsx_parser_peek(parser);
		struct gsx_ast*		node = NULL;
		if (token.kind == GSX_TOKEN_CALL) {
			node = gsx_ast_parse_call(parser);
		} else if (token.kind == GSX_TOKEN_IDENT) {
			node = gsx_ast_parse_ident(parser);
		} else if (token.kind == GSX_TOKEN_INTEGER | token.kind == GSX_TOKEN_STRING) {
			node = gsx_ast_parse_literal(parser);
		} else {
			if (!gsx_parser_expect_any(parser, arg_kinds, arg_kinds_count)) break ;
		}
		if (node == NULL) break ;
		da_append(&args, &node);
	}
	if (args.len < args_count) {
		fprintf(stderr, "%s:\t"TERMINAL_NOTICE_ERROR": Insufficient arguments passed to `%.*s`!\n", program_name, (int)text.len, text.data);
		fprintf(stderr, "\tExpected %zu parameters, got %zu arguments instead.\n", args_count, args.len);

		for (size_t i = 0; i < args.len; ++i) {
			struct gsx_ast*	arg = da_at(struct gsx_ast*, &args, i);
			gsx_ast_free(arg);
		}
		da_free(&args);
		return (NULL);
	}

	struct gsx_ast*			node = malloc(sizeof(struct gsx_ast) + sizeof(struct gsx_ast_call));
	if (node == NULL) {
		for (size_t i = 0; i < args.len; ++i) {
			struct gsx_ast*	arg = da_at(struct gsx_ast*, &args, i);
			gsx_ast_free(arg);
		}
		da_free(&args);
		return (NULL);
	}
	struct gsx_ast_call*	call = (struct gsx_ast_call*)&((char *)node)[sizeof(struct gsx_ast)];
	call->name = name;
	call->args = args;
	node->kind = GSX_AST_CALL;
	node->data.call = call;
	return (node);
}

struct gsx_ast*		gsx_ast_parse(const struct dynamic_array* definitions, const struct dynamic_array* tokens) {
	struct gsx_parser	parser = {
		.definitions = definitions,
		.tokens = tokens,
		.current = { 0 },
		.index = 0ul,
		.ok = true
	};

	struct gsx_ast*		ast = NULL;
	struct gsx_token	token = gsx_parser_peek(&parser);
	fprintf_ast_call("gsx_ast_parse", token);
	if (token.kind == GSX_TOKEN_STRING || token.kind == GSX_TOKEN_CHAR || token.kind == GSX_TOKEN_INTEGER)
		ast = gsx_ast_parse_literal(&parser);
	else if (token.kind == GSX_TOKEN_IDENT)
		ast = gsx_ast_parse_ident(&parser);
	else if (token.kind == GSX_TOKEN_CALL) {
		ast = gsx_ast_parse_call(&parser);
		if (ast != NULL && !gsx_parser_eof(&parser)) {
			struct sview	name = ast->data.call->name.text;
			size_t			extra_count = 0u;
			while (!gsx_parser_eof(&parser)) {
				gsx_parser_next(&parser);
				extra_count++;
			}

			fprintf(stderr, "%s:\t"TERMINAL_NOTICE_ERROR": Extra arguments passed to `%.*s`!\n", program_name, (int)name.len, name.data);
			fprintf(stderr, "\tExpected %zu parameters, got %zu arguments instead.\n", ast->data.call->args.len, ast->data.call->args.len + extra_count);
			return (gsx_ast_free(ast), parser.ok = false, NULL);
		}
	} else {
		struct sview	name = token.text;
		fprintf(stderr, "%s:\t"TERMINAL_NOTICE_ERROR": Expected `expression`, got `%.*s` instead.\n", program_name, (int)name.len, name.data);
		return (parser.ok = false, NULL);
	}

	if (!gsx_parser_eof(&parser)) {
		struct sview	name = token.text;
		fprintf(stderr, "%s:\t"TERMINAL_NOTICE_ERROR": Multi value expression `%.*s` is not allowed!\n", program_name, (int)name.len, name.data);
		fprintf(stderr, "\t| remaining tokens = [ ");
		while (!gsx_parser_eof(&parser)) {
			struct gsx_token	token = gsx_parser_next(&parser);
			fprintf(stderr, "`%.*s`", (int)token.text.len, token.text.data);
			if (!gsx_parser_eof(&parser))
				fprintf(stderr, ", ");
		}
		fprintf(stderr, " ]\n");
		return (gsx_ast_free(ast), parser.ok = false, NULL);
	} else return (ast);
}

struct gsx_box*		gsx_process_ast(const struct dynamic_array* definitions, struct gsx_ast* ast);

struct gsx_box*		gsx_process_ast_literal(const struct dynamic_array* definitions, struct gsx_ast_literal* ast) {
	/* NOTE(xenobas): do I remove the argument definitions? */
	struct gsx_token	token = ast->token;
	return ((void)definitions, gsx_box_new_sview(token.text));
}

struct gsx_box*		gsx_process_ast_ident(const struct dynamic_array* definitions, struct gsx_ast_ident* ident) {
	struct sview			text = ident->name.text;
	struct gsx_definition*	definition = gsx_definition_get(definitions, text);
	struct gsx_box*			retval = gsx_process_ast(definitions, definition->data.ast);
	if (retval == NULL)
		fprintf(stderr, "%s:\t" TERMINAL_NOTICE_ERROR ": Referenced undeclared identifier `%.*s`!\n", program_name, (int)text.len, text.data);
	return (retval);
}

struct gsx_box*		gsx_process_ast_call(const struct dynamic_array* definitions, struct gsx_ast_call* call) {
	struct dynamic_array			args = call->args;
	struct sview					text = { .data = call->name.text.data, .len = call->name.text.len - 1u };
	struct gsx_box*					retval = NULL;
	struct gsx_definition*			definition = gsx_definition_get(definitions, text);
	if (definition == NULL) {
		fprintf(stderr, "%s:\t" TERMINAL_NOTICE_ERROR ": Called undeclared function `%.*s`!\n", program_name, (int)text.len, text.data);
		goto gsx_process_ast_call_return;
	}

	enum gsx_definition_kind		kind = definition->kind;
	struct dynamic_array			params = definition->params;
	if (params.len != args.len) {
		fprintf(stderr, "%s:\t" TERMINAL_NOTICE_ERROR ": Called function `%.*s` with incorrect number of arguments.\n", program_name, (int)text.len, text.data);
		fprintf(stderr, "\tExpected %zu, got %zu arguments instead.\n", params.len, args.len);
		goto gsx_process_ast_call_return;
	}
	switch (kind) {
		case GSX_DEFINITION_FUNCTION: {
			retval = gsx_box_new_sview(text);
		} break ;
		case GSX_DEFINITION_INTERNAL: {
			gsx_definition_internal		function = definition->data.internal;
			if (function == NULL) {
				fprintf(stderr, "%s:\t" TERMINAL_NOTICE_ERROR ": Unimplemented internal function `%.*s`.\n", program_name, (int)text.len, text.data);
				fprintf(stderr, "\tQuestion: How did you even get here?\n");
				goto gsx_process_ast_call_return;
			}

			struct dynamic_array	args_boxed = da_make(sizeof(struct gsx_box*));
			retval = function(&args_boxed);
			if (retval == NULL) {
				fprintf(stderr, "%s:\t" TERMINAL_NOTICE_ERROR ": Disallowed function `%.*s` returns void expression.\n", program_name, (int)text.len, text.data);
				fprintf(stderr, "\tHint: Will be supported in the future.\n");
				goto gsx_process_ast_call_return;
			}
		} break ;
		case GSX_DEFINITION_VARIABLE: {
			fprintf(stderr, "%s:\t" TERMINAL_NOTICE_ERROR ": Cannot call variable `%.*s` as if it was a function.\n", program_name, (int)text.len, text.data);
			fprintf(stderr, "\tQuestion: How did you even get here?\n");
			goto gsx_process_ast_call_return;
		} break ;
		case GSX_DEFINITION_INVALID: {
			fprintf(stderr, "%s:\t" TERMINAL_NOTICE_ERROR ": Called function `%.*s` with invalid registered definition.\n", program_name, (int)text.len, text.data);
			fprintf(stderr, "\tQuestion: How did you even get here?\n");
			goto gsx_process_ast_call_return;
		} break ;
		default: {
			fprintf(stderr, "%s:\t" TERMINAL_NOTICE_ERROR ": Called function `%.*s` with unreachable registered definition.\n", program_name, (int)text.len, text.data);
			fprintf(stderr, "\tQuestion: How did you even get here?\n");
			goto gsx_process_ast_call_return;
		} break ;
	}

gsx_process_ast_call_return:
	return (retval);
}

struct gsx_box*		gsx_process_ast_define(const struct dynamic_array* definitions, struct gsx_ast_define* ast) {
	(void)definitions;
	(void)ast;
	return (gsx_box_new_string("todo: gsx_process_ast_define"));
}

struct gsx_box*		gsx_process_ast(const struct dynamic_array* definitions, struct gsx_ast* ast) {
	if (definitions == NULL || ast == NULL || gsx_ast_is_value_void(ast)) return (NULL);
	if (ast->kind == GSX_AST_LITERAL) return (gsx_process_ast_literal(definitions, ast->data.literal));
	if (ast->kind == GSX_AST_IDENT) return (gsx_process_ast_ident(definitions, ast->data.ident));
	if (ast->kind == GSX_AST_CALL) return (gsx_process_ast_call(definitions, ast->data.call));
	if (ast->kind == GSX_AST_DEFINE) return (gsx_process_ast_define(definitions, ast->data.define));
	return (NULL);
}

struct gsx_box*		gsx_process_expression(struct dynamic_array* definitions, struct sview comment) {
	assert(comment.data != NULL && comment.len > 0ul && "Invalid argument passed to gsx_process_expression");
	struct sview	expression = comment;
	expression = sview_drop(expression, cstr_length("<!--"));
	expression = sview_take(expression, expression.len - cstr_length("-->"));
	/* TODO(xenobas): Handle this mess post tokenization not during processing of raw strings. */
	expression = sview_trim(expression, " \t");
	for (size_t i = 0; i < expression.len; ++i) { /* NOTE(xenobas): Do not allow for multiline expressions... for now that is. */
		if (expression.data[i] == '\n') {
			struct sview	text = sview_trim(expression, " \r\n\t");
			fprintf(stderr, "%s:\t"TERMINAL_NOTICE_ERROR": Unsupported multiline gsx comment\n\t|\t", program_name);
			if (text.len > 0) fprintf(stderr, "%.*s\n", (int)text.len, text.data);
			else fprintf(stderr, "<empty comment>\n");
			fprintf(stderr, "\t|\n");
			return (NULL);
		}
	}
	expression = sview_drop(expression, cstr_length("gsx:"));
	expression = sview_trim(expression, " \t");

	struct gsx_box*			production = NULL;
	struct dynamic_array	tokens = da_make(sizeof(struct gsx_token));
	if (!gsx_lexer_tokenize(expression, &tokens)) {
		for (size_t i = 0; i < tokens.len; ++i) {
			struct gsx_token	token = da_at(struct gsx_token, &tokens, i);

			token.text = sview_trim(token.text, " \n\r\t");
			gsx_token_fprint(stderr, token);
			fprintf(stderr, "\n");
		}
		goto gsx_process_expression_return;
	}
	if (false) {
		fprintf(stdout, "tokens = { ");
		for (size_t i = 0; i < tokens.len; ++i) {
			struct gsx_token	token = da_at(struct gsx_token, &tokens, i);
			struct sview		text = sview_trim(token.text, " \n\r\t");
			fprintf(stdout, "( ");
			gsx_token_kind_fprint(stdout, token.kind);
			fprintf(stdout, " `%.*s` )", (int)text.len, text.data);
			if (i + 1 < tokens.len)
				fprintf(stdout, ", ");
		}
		fprintf(stdout, " }\n");
	}

	struct gsx_ast*			syntax_tree = gsx_ast_parse(definitions, &tokens);
	if (syntax_tree == NULL)
		goto gsx_process_expression_return;
	production = gsx_process_ast(definitions, syntax_tree);
	/* gsx_ast_print(stdout, syntax_tree, 0); */
	gsx_ast_free(syntax_tree);

gsx_process_expression_return:
	da_free(&tokens);
	return (production);
}

bool				gsx_process_sections(struct dynamic_array* definitions, const struct dynamic_array* sections, struct dynamic_array* boxes) {
	assert((sections != NULL && boxes != NULL) && "Invalid arguments passed to gsx_process_sections");

	bool				ok = true;
	for (int i = 0; i < (int)sections->len; ++i) {
		struct gsx_section	section = da_at(struct gsx_section, sections, i);
		if (section.is_comment && !section.is_terminated) {
			ok = false;

			struct sview	text = sview_trim(section.text, " \r\n\t");
			fprintf(stderr, "%s:\t"TERMINAL_NOTICE_ERROR": Unterminated comment\n\t|\t", program_name);
			if (text.len > 0) fprintf(stderr, "%.*s\n", (int)text.len, text.data);
			else fprintf(stderr, "<empty text>\n");
			fprintf(stderr, "\t|\n");
			continue ;
		}
		struct gsx_box*	box = NULL;
		if (section.is_expression) {
			box = gsx_process_expression(definitions, section.text);
			ok = (ok && (box != NULL));
		} else if (!section.is_comment) {
			box = gsx_box_new_sview(section.text);
			ok = (ok && (box != NULL));
		}
		if (box != NULL) da_append(boxes, &box);
	}
	return (ok);
}

struct gsx_box*		gsx_internal_stub(struct dynamic_array* args) {
	(void)args;
	return (NULL);
}

struct gsx_box*		gsx_internal_include(struct dynamic_array* args) {
	(void)args;
	return (gsx_box_new_string("core::include"));
}

void				gsx_init_definitions(struct dynamic_array* definitions) {
	bool					arg_placeholder = true;

	char*					define_title_name = cstr_clone("TITLE");
	char*					define_title_val = cstr_clone("index.html");
	struct gsx_ast*			define_title_ast = NULL;
	{
		struct gsx_ast*			node = malloc(sizeof(struct gsx_ast) + sizeof(struct gsx_ast_literal));
		struct gsx_ast_literal*	literal = (struct gsx_ast_literal*)&((char *)node)[sizeof(struct gsx_ast)];
		if (node != NULL) {
			literal->token.text = sview_make_cstr(define_title_val);
			literal->token.kind = GSX_TOKEN_STRING;

			node->kind = GSX_AST_LITERAL;
			node->data.literal = literal;
		}
		define_title_ast = node;
	}
	struct gsx_definition	define_title = {
		.name = sview_make_cstr(define_title_name),
		.params = { 0 },
		.kind = GSX_DEFINITION_VARIABLE,
		.data = { .ast = define_title_ast },
	};
	da_append(definitions, &define_title);

	char*					define_define_name = cstr_clone("core::define");
	struct dynamic_array	define_define_params = da_make(sizeof(bool));
	da_append(&define_define_params, &arg_placeholder);
	struct gsx_definition	define_define = {
		.name = sview_make_cstr(define_define_name),
		.params = define_define_params,
		.kind = GSX_DEFINITION_INTERNAL,
		.data = { .internal = gsx_internal_stub },
	};
	da_append(definitions, &define_define);

	char*					define_env_name = cstr_clone("core::env");
	struct dynamic_array	define_env_params = da_make(sizeof(bool));
	da_append(&define_env_params, &arg_placeholder);
	struct gsx_definition	define_env = {
		.name = sview_make_cstr(define_env_name),
		.params = define_env_params,
		.kind = GSX_DEFINITION_INTERNAL,
		.data = { .internal = gsx_internal_stub },
	};
	da_append(definitions, &define_env);

	char*					define_include_name = cstr_clone("core::include");
	struct dynamic_array	define_include_params = da_make(sizeof(bool));
	da_append(&define_include_params, &arg_placeholder);
	struct gsx_definition	define_include = {
		.name = sview_make_cstr(define_include_name),
		.params = define_include_params,
		.kind = GSX_DEFINITION_INTERNAL,
		.data = { .internal = gsx_internal_include },
	};
	da_append(definitions, &define_include);

	char*					define_factorial_name = cstr_clone("core::factorial");
	struct dynamic_array	define_factorial_params = da_make(sizeof(bool));
	da_append(&define_factorial_params, &arg_placeholder);
	struct gsx_definition	define_factorial = {
		.name = sview_make_cstr(define_factorial_name),
		.params = define_factorial_params,
		.kind = GSX_DEFINITION_INTERNAL,
		.data = { .internal = gsx_internal_stub },
	};
	da_append(definitions, &define_factorial);

	char*					define_mul_name = cstr_clone("core::mul");
	struct dynamic_array	define_mul_params = da_make(sizeof(bool));
	da_append(&define_mul_params, &arg_placeholder);
	da_append(&define_mul_params, &arg_placeholder);
	struct gsx_definition	define_mul = {
		.name = sview_make_cstr(define_mul_name),
		.params = define_mul_params,
		.kind = GSX_DEFINITION_INTERNAL,
		.data = { .internal = gsx_internal_stub },
	};
	da_append(definitions, &define_mul);

	char*					define_add_name = cstr_clone("core::add");
	struct dynamic_array	define_add_params = da_make(sizeof(bool));
	da_append(&define_add_params, &arg_placeholder);
	da_append(&define_add_params, &arg_placeholder);
	struct gsx_definition	define_add = {
		.name = sview_make_cstr(define_add_name),
		.params = define_add_params,
		.kind = GSX_DEFINITION_INTERNAL,
		.data = { .internal = gsx_internal_stub },
	};
	da_append(definitions, &define_add);

	char*					define_replace_name = cstr_clone("core::replace");
	/* NOTE(xenobas): Parameters need to be rethought out */
	struct dynamic_array	define_replace_params = da_make(sizeof(enum gsx_ast_define_param_kind));
	/* struct gsx_ast_define_param { */
	/* 	enum gsx_ast_define_param_kind	kind; */
	/* 	union { */
	/* 		struct gsx_ast_ident*		ident; */
	/* 		struct gsx_ast_literal*		pattern; */
	/* 		struct gsx_ast_destructure*	destructure; */
	/* 	}								data; */
	/* }; */
	da_append(&define_replace_params, &arg_placeholder);
	da_append(&define_replace_params, &arg_placeholder);
	da_append(&define_replace_params, &arg_placeholder);
	struct gsx_definition	define_replace = {
		.name = sview_make_cstr(define_replace_name),
		.params = define_replace_params,
		.kind = GSX_DEFINITION_INTERNAL,
		.data = { .internal = gsx_internal_stub },
	};
	da_append(definitions, &define_replace);
}

void				gsx_free_definitions(struct dynamic_array* definitions) {
	/* TODO(xenobas): Finish implementing gsx_free_definitions */
	if (definitions == NULL) return ;
	for (size_t i = 0; i < definitions->len; ++i) {
		struct gsx_definition	definition = da_at(struct gsx_definition, definitions, i);
		da_free(&definition.params);
	}
}

int		main(int argc, char **argv) {
	program_name = argv[0];

	if (argc != 3) {
		fprintf(stderr, "%s:\t"TERMINAL_NOTICE_ERROR": Incorrect number of arguments.\n", program_name);
		fprintf(stderr, "\t"TERMINAL_NOTICE_USAGE": %s <in.gsx> <out.html>\n", program_name);
		return (1);
	}

	const char*				file_in_path = argv[1];
	if (!cstr_suffix(file_in_path, ".gsx")) {
		fprintf(stderr, "%s:\t"TERMINAL_NOTICE_ERROR": Invalid input file extension `%s`.\n", program_name, file_in_path);
		fprintf(stderr, "\t"TERMINAL_NOTICE_USAGE": %s <in.gsx> <out.html>\n", program_name);
		return (2);
	}

	const char*				file_out_path = argv[2];
	if (cstr_equals(file_in_path, file_out_path)) {
		fprintf(stderr, "%s:\t"TERMINAL_NOTICE_ERROR": Reading and writing into the same file `%s` is not allowed.\n", program_name, file_out_path);
		fprintf(stderr, "\t"TERMINAL_NOTICE_USAGE": %s \"%s\" <out.html>\n", program_name, file_in_path);
		return (2);
	}
	if (!cstr_suffix(file_out_path, ".html")) {
		fprintf(stderr, "%s:\t"TERMINAL_NOTICE_ERROR": Invalid output file extension `%s`.\n", program_name, file_out_path);
		fprintf(stderr, "\t"TERMINAL_NOTICE_USAGE": %s \"%s\" <out.html>\n", program_name, file_in_path);
		return (2);
	}

	struct sview			file_in_view = os_file_view(file_in_path);
	if (file_in_view.data == NULL) {
		fprintf(stderr, "%s:\t"TERMINAL_NOTICE_ERROR": Could not read file `%s`.\n", program_name, file_in_path);
		fprintf(stderr, "\t"TERMINAL_NOTICE_USAGE": %s <in.gsx> \"%s\"\n", program_name, file_out_path);
		return (4);
	}

	struct dynamic_array	definitions = da_make(sizeof(struct gsx_definition));
	gsx_init_definitions(&definitions);

	struct dynamic_array	sections = da_make(sizeof(struct gsx_section));
	if (!gsx_load_sections(file_in_view, &sections)) {
		fprintf(stderr, "%s:\t"TERMINAL_NOTICE_ERROR": Invalid gsx file `%s`.\n", program_name, file_in_path);
		fprintf(stderr, "\t"TERMINAL_NOTICE_HINT": Maybe the file has an unterminated comment somewhere?\n");
		program_return = 8;
		goto sections_free;
	}

	struct dynamic_array	boxes = da_make(sizeof(struct gsx_box*));
	if (!gsx_process_sections(&definitions, &sections, &boxes)) {
		fprintf(stderr, "%s:\t"TERMINAL_NOTICE_ERROR": An error happened while processing `%s`.\n", program_name, file_in_path);
		program_return = 16;
		goto boxes_free;
	}
	
	struct string_builder	sb = string_builder_make();
	for (int i = 0; i < (int)boxes.len; ++i) {
		// struct gsx_section	section = da_at(struct gsx_section, &sections, i);
		struct gsx_box*		box	= da_at(struct gsx_box*, &boxes, i);
		if (box == NULL) continue ;
		if (box->kind == GSX_BOX_STRING) string_builder_write_cstr(&sb, box->data.str);
		if (box->kind == GSX_BOX_CHAR) string_builder_write_char(&sb, box->data.character);
		// struct sview		string = sview_trim(sview_make_cstr_const(str), " \r\n\t");
		// if (section.is_expression && string.len == 0ul)
		// 	continue ;
		// string_builder_write_cstr(&sb, str);
	}

	struct sview			file_out_view = string_builder_as_sview(sb);
	if (file_out_view.data == NULL) {
		fprintf(stderr, "%s:\t"TERMINAL_NOTICE_ERROR": An error happened while generating output file content `%s`.\n", program_name, file_out_path);
		program_return = 32;
		goto builder_free;
	}
	if (!os_file_dump(file_out_path, file_out_view)) {
		fprintf(stderr, "%s:\t"TERMINAL_NOTICE_ERROR": An error happened while generating output file content `%s`.\n", program_name, file_out_path);
		fprintf(stderr, "\tDescription: %m\n");
		program_return = 32;
		goto file_out_free;
	}
	fprintf(stdout, "%s:\t"TERMINAL_NOTICE_SUCCESS": Evaluated gsx template `%s` into %zu bytes written in `%s`.\n", program_name, file_in_path, file_out_view.len, file_out_path);

file_out_free:
	free(file_out_view.data);
builder_free:
	string_builder_destroy(&sb);
boxes_free:
	for (size_t i = 0; i < boxes.len; ++i) {
		struct gsx_box*	box = da_at(struct gsx_box*, &boxes, i);
		gsx_box_free(box);
	}
	da_free(&boxes);
sections_free:
	da_free(&sections);
	gsx_free_definitions(&definitions);
	da_free(&definitions);
	return (program_return);
}
