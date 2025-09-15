#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <string.h>
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

#define TERMINAL_NOTICE_SUCCESS TERMINAL_COLOR_GREEN "ok" TERMINAL_STYLE_RESET
#define TERMINAL_NOTICE_ERROR TERMINAL_COLOR_RED "error" TERMINAL_STYLE_RESET
#define TERMINAL_NOTICE_TODO TERMINAL_COLOR_YELLOW "todo" TERMINAL_STYLE_RESET
#define TERMINAL_NOTICE_HINT TERMINAL_COLOR_WHITE "hint" TERMINAL_STYLE_RESET
#define TERMINAL_NOTICE_USAGE TERMINAL_COLOR_WHITE "usage" TERMINAL_STYLE_RESET
#define TERMINAL_NOTICE_DEBUG TERMINAL_COLOR_WHITE "debug" TERMINAL_STYLE_RESET

#define _STRINGIFY(X) #X
#define STRINGIFY(X) _STRINGIFY(X)

#define ttyout ((int)1)
#define ttyerr ((int)2)

#ifdef DEBUG_AST_PARSE_CALLS
#define PRINT_AST_PARSE_CALL(NAME, TOKEN) print(ttyout, "%cstr%: %int%: PARSE_CALL: " NAME " <- %gsx_token%\n", __FILE__, __LINE__, (TOKEN));
#else
#define PRINT_AST_PARSE_CALL(NAME, TOKEN) ((void)(NAME), (void)(TOKEN))
#endif

#ifdef DEBUG_AST_INTERPRET_CALLS
#define PRINT_AST_INTERPRET_CALL(NAME, AST) print(ttyout, "%cstr%: %int%: INTERPRET_CALL: " NAME " <- %#gsx_ast%\n", __FILE__, __LINE__, (AST));
#else
#define PRINT_AST_INTERPRET_CALL(NAME, AST) ((void)(NAME), (void)(AST))
#endif

/*	 Program	*/

const char*	program_name = NULL;
int			program_return = 0;

/*	 Forward_Declarations	*/
struct					view {
	char	*data;
	size_t	len;
};
struct					dynamic_array {
	void	*items;
	size_t	len;
	size_t	cap;
	size_t	unit;
};
struct					string_builder {
	struct dynamic_array	bytes;
};
struct					location {
	size_t		index;
	size_t		line;
	size_t		column;
	struct view	path;
};

struct					gsx_ast;
struct					gsx_box;
struct					gsx_parser;
struct					gsx_section {
	struct location	loc;
	struct view		text;
	bool			is_comment;
	bool			is_statement;
	bool			is_terminated;
};
struct					gsx_template;
struct					gsx_definition;
struct					gsx_statement {
	struct gsx_ast*			ast;
	struct dynamic_array	tokens;
	struct gsx_section		section;
};
struct					gsx_virtual_machine {
	struct dynamic_array	definitions; /* type: struct gsx_definition */
	struct dynamic_array	templates; /* type: struct gsx_template */
	struct gsx_template*	template_entry;
	struct dynamic_array	scopes; /* type: struct gsx_dynamic_array<struct gsx_definition> */
	bool					ok;
};

int						cstr_length(const char*);

struct string_builder	string_builder_make(void);
void					string_builder_clear(struct string_builder*);
void					string_builder_destroy(struct string_builder*);
char*					string_builder_as_cstr(struct string_builder);
struct view				string_builder_as_view(struct string_builder);
void					string_builder_write_cstr(struct string_builder*, const char*);
void					string_builder_write_view(struct string_builder*, struct view);
void					string_builder_write_char(struct string_builder*, char);
void					string_builder_write_long(struct string_builder*, long);
void					string_builder_write_bool(struct string_builder*, bool);

struct gsx_ast*			gsx_clone_ast(struct gsx_ast*);

/*	 Memory	*/

void	mem_copy(void *dest, void *source, size_t nbytes) {
	if (dest == NULL || source == NULL || nbytes == 0) return ;
	for (size_t i = 0; i < nbytes; i++)
		((char *)dest)[i] = ((char *)source)[i];
}

void	mem_fill(void *dest, unsigned char byte, size_t nbytes) {
	if (dest == NULL || nbytes == 0) return ;
	for (size_t i = 0; i < nbytes; i++)
		((unsigned char *)dest)[i] = byte;
}

void*	mem_clone(void *source, size_t nbytes) {
	void*	clone = malloc(nbytes);
	mem_copy(clone, source, nbytes);
	return (clone);
}

/*	Math */
long	math_factorial(long n) {
	if (n < 2) return (1);
	return (math_factorial(n - 1) * n);
}

/*	 String_View	*/

int				view_index(const struct view string, const char* to_find) {
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

int				view_index_last(const struct view string, const char* to_find) {
	int	length_to_find = cstr_length(to_find);
	if (to_find == NULL || length_to_find == 0) return (-1);

	int	position = -1;
	for (int i = 0; i < (int)string.len; ++i) {
		int		j = 0;
		bool	match = true;
		while (j < length_to_find && j + i < (int)string.len && match) {
			match = (string.data[i + j] == to_find[j]);
			j++;
		}
		if (match && j == length_to_find) position = i;
	}
	return (position);
}

int				view_index_char(const struct view string, const char to_find) {
	if (to_find == '\0') return (-1);
	for (int i = 0; i < (int)string.len; ++i) {
		if (string.data[i] == to_find) return (i);
	}
	return (-1);
}

int				view_index_view(const struct view string, struct view to_find) {
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

struct view		view_make_cstr(char* cstr) {
	return ((struct view){ cstr, (size_t)cstr_length(cstr) });
}

struct view		view_make_cstr_const(const char* cstr) {
	return ((struct view){ (char*)cstr, (size_t)cstr_length(cstr) });
}

struct view		view_make(char* data, size_t len) {
	return ((struct view){ data, len });
}

struct view		view_slice(const struct view string, size_t begin, size_t end) {
	if (begin > string.len) return (view_make(NULL, 0ul));
	if (end > string.len) end = string.len;
	size_t	len = end - begin;
	return (view_make(&string.data[begin], len));
}

struct view		view_drop(const struct view string, int count) {
	if (count < 0) count = 0;
	if (count >= (int)string.len) return ((struct view){ NULL, 0ul });
	else return ((struct view){ &string.data[count], (int)string.len - count });
}

struct view		view_take(const struct view string, int count) {
	if (count >= (int)string.len) count = (int)string.len;
	if (count <= 0) return ((struct view){ NULL, 0ul });
	else return ((struct view){ string.data, count });
}

struct view		view_drop_take(const struct view string, int drop, int take) {
	return (view_take(view_drop(string, drop), take));
}

bool			view_equals_cstr(const struct view lhs, const char* rhs) {
	return (lhs.len == (size_t)cstr_length(rhs) && view_index(lhs, rhs) == 0);
}

bool			view_equals(const struct view lhs, struct view rhs) {
	return (lhs.len == rhs.len && view_index_view(lhs, rhs) == 0);
}

struct view		view_trim(const struct view string, const char *charset) {
	if (charset == NULL || string.data == NULL || (int)string.len <= 0) return (string);
	int				drop_count = 0;
	while (drop_count < (int)string.len) {
		bool	match = false;
		for (int i = 0; charset[i] && !match; i++)
			match = (charset[i] == string.data[drop_count]);
		if (!match) break ;
		drop_count++;
	}
	struct view	trim_left = view_drop(string, drop_count);

	int				take_count = trim_left.len;
	while (take_count > 0) {
		bool	match = false;
		for (int i = 0; charset[i] && !match; ++i)
			match = (charset[i] == trim_left.data[take_count - 1]);
		if (!match) break ;
		take_count--;
	}
	return (view_take(trim_left, take_count));
}

bool			view_prefix(const struct view string, const char* prefix) {
	return (view_index(string, prefix) == 0);
}

bool			view_suffix(const struct view string, const char* suffix) {
	int	length_suffix = cstr_length(suffix);
	if ((int)string.len < length_suffix) return (false);
	return (view_index_last(string, suffix) == ((int)string.len - length_suffix));
}

/*	 C_String	*/

int		cstr_length(const char* str) {
	int	length = 0;

	if (str == NULL) return (0);
	while (str[length])
		length++;
	return (length);
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

char*	cstr_clone_view(struct view view) {
	return (cstr_clone_len(view.data, view.len));
}

int		cstr_index(const char* str, const char* to_find) {
	struct view	vstr = view_make_cstr_const(str);
	return (view_index(vstr, to_find));
}

int		cstr_index_char(const char* str, const char to_find) {
	struct view	vstr = view_make_cstr_const(str);
	return (view_index_char(vstr, to_find));
}

bool	cstr_equals(const char *lhs, const char *rhs) {
	return (cstr_length(lhs) == cstr_length(rhs) && cstr_index(lhs, rhs) == 0);
}

bool	cstr_contains(const char *str, const char *to_find) {
	return (cstr_index(str, to_find) >= 0);
}

bool	cstr_contains_char(const char *str, const char to_find) {
	return (cstr_index_char(str, to_find) >= 0);
}

bool	cstr_prefix(const char* str, const char* prefix) {
	struct view	vstr = view_make_cstr_const(str);
	return (view_prefix(vstr, prefix));
}

bool	cstr_suffix(const char* str, const char* suffix) {
	struct view	vstr = view_make_cstr_const(str);
	return (view_suffix(vstr, suffix));
}

char*	cstr_replace(const char* text, const char* pattern, const char* replace) {
	if (text == NULL || pattern == NULL || replace == NULL) return (NULL);

	size_t					len_text = cstr_length(text);
	size_t					len_pattern = cstr_length(pattern);
	if (len_pattern == 0ul || len_text == 0ul) return (NULL);

	size_t					i = 0ul;
	struct string_builder	sb = string_builder_make();
	char*					result = NULL;
	while (i < len_text) {
		int	offset = cstr_index(&text[i], pattern);
		if (offset == 0) {
			string_builder_write_cstr(&sb, replace);
			i += len_pattern;
			continue ;
		}
		if (offset < 0) {
			string_builder_write_cstr(&sb, &text[i]);
			i = len_text;
			break ;
		}
		struct view			segment = view_make((char*)&text[i], offset);
		string_builder_write_view(&sb, segment);
		i += offset;
	}

	result = string_builder_as_cstr(sb);
	string_builder_destroy(&sb);
	return (result);
}

/*	 Text_Conversion	*/

long	view_conv_long(struct view str) {
	size_t	i = 0ul;
	long	s = 1l;
	long	n = 0l;
	if (view_prefix(str, "-"))  {
		s = -1;
		i++;
	}
	while (i < str.len) {
		char	byte = str.data[i];
		if (byte < '0' || byte > '9')
			break ;
		n = (n * 10l) + (long)(byte - '0');
		i++;
	}
	return ((i == str.len) ? (n * s) : 0l);
}

long	cstr_conv_long(const char* str) {
	return (view_conv_long(view_make_cstr_const(str)));
}

/*	 Dynamic_Array	*/

#define da_at(TYPE, DA, INDEX) (((TYPE *)((DA)->items))[INDEX])
#define da_first(TYPE, DA) (assert((DA)->len > 0ul && "cannot get first in an empty dynamic array"), (((TYPE *)((DA)->items))[0]))
#define da_last(TYPE, DA) (assert((DA)->len > 0ul && "cannot get last in an empty dynamic array"), (((TYPE *)((DA)->items))[(DA)->len - 1ul]))

#define da_foreach_begin_index(NAME, INDEX, DA, TYPE) for (size_t INDEX = 0ul; INDEX < (DA)->len; ++INDEX) {\
											TYPE	NAME = da_at(TYPE, DA, INDEX)

#define da_foreach_begin_index_ref(NAME, INDEX, DA, TYPE) for (size_t INDEX = 0ul; INDEX < (DA)->len; ++INDEX) {\
											TYPE*	NAME = &da_at(TYPE, DA, INDEX)

#define da_foreach_begin(NAME, DA, TYPE) da_foreach_begin_index(NAME, da_index, DA, TYPE)

#define da_foreach_begin_ref(NAME, DA, TYPE) da_foreach_begin_index_ref(NAME, da_index, DA, TYPE)

#define da_foreach_end() }

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

bool					da_pop(struct dynamic_array *da) {
	if (da == NULL) return (false);
	if (da->len == 0ul) return (false);
	return (da->len--, true);
}

bool					da_push(struct dynamic_array *da, void *item) {
	if (da == NULL) return (false);

	da_expand(da);
	mem_copy(&((char *)da->items)[da->len * da->unit], item, da->unit);
	da->len++;
	return (da->items != NULL);
}

struct dynamic_array	da_clone(struct dynamic_array ref) {
	struct dynamic_array	da = da_make(ref.unit);
	da.cap = ref.cap;
	da.len = ref.len;

	if (da.unit * da.cap > 0) {
		size_t				size = sizeof(da.unit) * sizeof(da.cap);

		da.items = malloc(size);
		mem_copy(da.items, ref.items, size);
	}
	return (da);
}

/*	 String_Builder	*/

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

char*					string_builder_as_cstr(struct string_builder sb) {
	return (cstr_clone_len(sb.bytes.items, sb.bytes.len));
}

struct view				string_builder_as_view(struct string_builder sb) {
	char*	data = string_builder_as_cstr(sb);
	return ((struct view){ .data = data, .len = cstr_length(data) });
}

struct view				string_builder_readonly_view(struct string_builder sb) {
	return ((struct view){ .data = (char*)sb.bytes.items, .len = sb.bytes.len * sb.bytes.unit });
}

void					string_builder_write_cstr(struct string_builder* sb, const char* str) {
	if (sb == NULL || str == NULL) return ;
	for (size_t i = 0ul; str[i]; ++i)
		da_push(&sb->bytes, (char*)&(str[i]));
}

void					string_builder_write_view(struct string_builder* sb, struct view string) {
	if (sb == NULL || string.data == NULL) return ;
	for (size_t i = 0; i < string.len; ++i)
		da_push(&sb->bytes, (void *)&string.data[i]);
}

void					string_builder_write_char(struct string_builder* sb, char c) {
	if (sb == NULL || c == '\0') return ;
	da_push(&sb->bytes, (void *)&c);
}

void					string_builder_write_long(struct string_builder* sb, long n) {
	if (n == LONG_MIN)
		string_builder_write_cstr(sb, "-9223372036854775808");
	else if (n < 0l) {
		string_builder_write_char(sb, '-');
		string_builder_write_long(sb, -n);
	}
	else if (n >= 10l) {
		string_builder_write_long(sb, n / 10l);
		string_builder_write_char(sb, "0123456789"[n % 10l]);
	}
	else {
		string_builder_write_char(sb, "0123456789"[n % 10l]);
	}
}

void					string_builder_write_bool(struct string_builder* sb, bool b) {
	const char*	text = b ? "true" : "false";
	string_builder_write_cstr(sb, text);
}

/*	Print_Format	*/

#define PRINT_LEADER '%'
#define PRINT_SYMBOL_SET "#_abcdefghijklmnopqrstuvwxyz0123456789"
#define PRINT_REGISTERY_CAPACITY 32ul

typedef void				(*print_writer)(int, va_list);

struct						print_definition {
	struct view			name;
	print_writer		writer;
};
struct						print_registery {
	struct print_definition	defs[PRINT_REGISTERY_CAPACITY];
	size_t					defs_len;
}							print_registery;
bool						print_registery_is_init = false;

void						print_definition_add(struct view name, print_writer writer) {
	assert(print_registery_is_init && "cannot add print definition before default definitions");
	assert(print_registery.defs_len < ((size_t)PRINT_REGISTERY_CAPACITY) && "Too many print definitions registered, Consider raising the capacity.");
	if (name.len == 0 || writer == NULL) return ;

	struct print_definition*	def = &print_registery.defs[print_registery.defs_len++];
	def->name = name;
	def->writer = writer;
}

struct print_definition*	print_definition_get(struct view name) {
	for (size_t i = 0ul; i < print_registery.defs_len; ++i) {
		struct print_definition*	def = &print_registery.defs[i];
		if (view_equals(name, def->name)) return (def);
	}
	return (NULL);
}

void						_print_writer_char(int fd, const char c) {
	write(fd, &c, 1ul);
}

void						_print_writer_cstr(int fd, const char* s) {
	write(fd, s, cstr_length(s));
}

void						_print_writer_cstr_alt(int fd, const char* s, char c) {
	write(fd, &c, 1ul);
	write(fd, s, cstr_length(s));
	write(fd, &c, 1ul);
}

void						_print_writer_view(int fd, struct view v) {
	write(fd, v.data, v.len);
}

void						_print_writer_view_alt(int fd, struct view v) {
	char		w = '"';
	if (v.len >= 2 && view_prefix(v, "\"") && view_suffix(v, "\"")) w = '`';

	write(fd, &w, 1ul);
	write(fd, v.data, v.len);
	write(fd, &w, 1ul);
}

void						_print_writer_long(int fd, long n) {
	if (n == LONG_MIN)
		write(fd, "-9223372036854775808", 19);
	else if (n < 0l) {
		write(fd, "-", 1);
		_print_writer_long(fd, -n);
	}
	else if (n >= 10l) {
		_print_writer_long(fd, n/ 10l);
		write(fd, &("0123456789"[n % 10l]), 1);
	}
	else {
		write(fd, &("0123456789"[n % 10l]), 1);
	}
}

void						_print_writer_ulong(int fd, unsigned long n) {
	if (n >= 10ul) {
		_print_writer_long(fd, n/ 10ul);
		write(fd, &("0123456789"[n % 10ul]), 1);
	}
	else {
		write(fd, &("0123456789"[n % 10ul]), 1);
	}
}

void						_print_writer_addr(int fd, unsigned long n) {
	if (n >= 16ul) {
		_print_writer_long(fd, n/ 16ul);
		write(fd, &("0123456789ABCDEF"[n % 16ul]), 1);
	}
	else {
		write(fd, &("0123456789ABCDEF"[n % 16ul]), 1);
	}
}

void						_print_writer_bool(int fd, bool b) {
	_print_writer_cstr(fd, b ? "true" : "false");
}

void						_print_writer_nth(int fd, size_t n) {
	_print_writer_ulong(fd, n);
	switch (n % 10ul) {
		case 1: _print_writer_cstr(fd, "st"); break ;
		case 2: _print_writer_cstr(fd, "nd"); break ;
		case 3: _print_writer_cstr(fd, "rd"); break ;
		default: _print_writer_cstr(fd, "th"); break ;
	}
}

void						print_writer_uint(int fd, va_list args) {
	unsigned int	n = va_arg(args, unsigned int);
	_print_writer_ulong(fd, (unsigned long)n);
}

void						print_writer_ulong(int fd, va_list args) {
	unsigned long	n = va_arg(args, long);
	_print_writer_ulong(fd, n);
}

void						print_writer_int(int fd, va_list args) {
	int	n = va_arg(args, int);
	_print_writer_long(fd, (long)n);
}

void						print_writer_long(int fd, va_list args) {
	long	n = va_arg(args, long);
	_print_writer_long(fd, n);
}

void						print_writer_char(int fd, va_list args) {
	/* NOTE(xenobas): char is promoted to int when passed through. */
	char	c = (char)va_arg(args, int);
	_print_writer_char(fd, c);
}

void						print_writer_cstr(int fd, va_list args) {
	const char*	s = va_arg(args, const char*);
	_print_writer_cstr(fd, s);
}

void						print_writer_view(int fd, va_list args) {
	struct view	v = va_arg(args, struct view);
	_print_writer_view(fd, v);
}

void						print_writer_cstr_alt(int fd, va_list args) {
	const char*	s = va_arg(args, const char*);
	struct view	v = view_make_cstr_const(s);

	_print_writer_view_alt(fd, v);
}

void						print_writer_view_alt(int fd, va_list args) {
	struct view	v = va_arg(args, struct view);

	_print_writer_view_alt(fd, v);
}

void						print_writer_bool(int fd, va_list args) {
	bool	b = (bool)va_arg(args, int);
	_print_writer_bool(fd, b);
}

void						print_writer_addr(int fd, va_list args) {
	void*	addr = va_arg(args, void*);
	_print_writer_cstr(fd, "&0x");
	_print_writer_addr(fd, (unsigned long)addr);
	(void)args;
}

void						print_writer_errno(int fd, va_list args) {
	(void)args;
	_print_writer_cstr(fd, strerror(errno));
}

void						print_writer_nth(int fd, va_list args) {
	size_t	n = va_arg(args, size_t);
	_print_writer_nth(fd, n);
}

void						print_registery_init(void) {
	if (!print_registery_is_init) {
		print_registery_is_init = true;

		mem_fill(&print_registery, 0, sizeof(struct print_registery));
		print_definition_add(view_make_cstr_const("char"), print_writer_char);
		print_definition_add(view_make_cstr_const("cstr"), print_writer_cstr);
		print_definition_add(view_make_cstr_const("view"), print_writer_view);
		print_definition_add(view_make_cstr_const("#cstr"), print_writer_cstr_alt);
		print_definition_add(view_make_cstr_const("#view"), print_writer_view_alt);
		print_definition_add(view_make_cstr_const("int"), print_writer_int);
		print_definition_add(view_make_cstr_const("uint"), print_writer_uint);
		print_definition_add(view_make_cstr_const("long"), print_writer_long);
		print_definition_add(view_make_cstr_const("ulong"), print_writer_ulong);
		print_definition_add(view_make_cstr_const("addr"), print_writer_addr);
		print_definition_add(view_make_cstr_const("bool"), print_writer_bool);
		print_definition_add(view_make_cstr_const("errno"), print_writer_errno);
		print_definition_add(view_make_cstr_const("nth"), print_writer_nth);
	}
}

void						print_va(int fd, const char* fmt, va_list args) {
	struct view								fmt_view = view_make_cstr_const(fmt);

	print_registery_init();
	for (size_t i = 0ul; i < fmt_view.len;) {
		char								leading = fmt_view.data[i];
		if (leading == PRINT_LEADER) {
			size_t							j = 0;
			char							terminus = '\0';
			for (j = i; j < fmt_view.len;) {
				terminus = fmt_view.data[++j];
				if (terminus == PRINT_LEADER) break ;
				if (!cstr_contains_char(PRINT_SYMBOL_SET, fmt_view.data[j])) break ;
			}
			if (terminus == PRINT_LEADER) {
				struct view					name = view_slice(fmt_view, i + 1, j);
				if (name.len > 0ul) {
					struct print_definition*	def = print_definition_get(name);
					if (def != NULL) def->writer(fd, args);
					else {
						write(fd, "%", 1ul);
						write(fd, name.data, name.len);
						write(fd, "?%", 2ul);
					}
				} else write(fd, "%", 1ul);
				i = j + 1;
				continue ;
			}
		}
		write(fd, &fmt_view.data[i], (int)sizeof(char));
		++i;
	}
}

void						print(int fd, const char* fmt, ...) {
	va_list	args; va_start(args, fmt);
	print_va(fd, fmt, args);
	va_end(args);
}

/*	 OS_File	*/

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

struct view		os_file_view(const char* path) {
	int	fd = open(path, O_RDONLY);
	if (fd < 0) return ((struct view){ NULL, 0ul });
	int	size = os_filedes_size(fd);
	if (size < 0) return ((struct view){ NULL, 0ul });

	char	*data = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	if (data == NULL) return (close(fd), (struct view){ NULL, 0ul });
	return (close(fd), (struct view){ data, size });
}

bool			os_file_dump(const char* path, struct view bytes) {
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
	return (close(fd), ok);
}

/*	Location	*/
bool	location_advance(struct location* loc, char c) {
	if (loc == NULL || c == '\0') return (false);

	++loc->index;
	++loc->column;
	if (c == '\n') {
		++loc->line;
		loc->column = 1ul;
	}
	return (true);
}

/*	 GSX_Definition	*/

typedef struct gsx_box*	(*gsx_definition_internal)(struct gsx_virtual_machine*, struct dynamic_array*);

enum					gsx_definition_kind {
	GSX_DEFINITION_INVALID,

	GSX_DEFINITION_CONSTANT,
	GSX_DEFINITION_FUNCTION,
	GSX_DEFINITION_INTERNAL_FUNCTION,
	GSX_DEFINITION_INTERNAL_CONSTANT,
};

union gsx_definition_data {
	struct gsx_ast*			ast;
	gsx_definition_internal	internal;
};

struct					gsx_definition {
	enum gsx_definition_kind	kind;
	struct view					name;
	struct dynamic_array		params; /* type: struct gsx_ast_param* */
	union gsx_definition_data	data;
};

struct gsx_definition*	gsx_definition_get(const struct dynamic_array* definitions, struct view name) {
	if (definitions == NULL) return (NULL);
	da_foreach_begin_ref(def, definitions, struct gsx_definition);
		if (view_equals(def->name, name))
			return (def);
	da_foreach_end();
	return (NULL);
}

/*	 GSX_Token	*/

enum				gsx_token_kind {
	GSX_TOKEN_INVALID,
	GSX_TOKEN_IDENT,
	GSX_TOKEN_CALL,
	GSX_TOKEN_CHAR,
	GSX_TOKEN_STRING,
	GSX_TOKEN_INTEGER,

	GSX_TOKEN_SQUARE_OPEN,
	GSX_TOKEN_SQUARE_CLOSE,
	GSX_TOKEN_PAREN_OPEN,
	GSX_TOKEN_PAREN_CLOSE,
	GSX_TOKEN_COMMA,
	GSX_TOKEN_COLON,

	GSX_TOKEN_HTML_SIGNATURE,
	GSX_TOKEN_HTML_OPEN,
	GSX_TOKEN_HTML_CLOSE,
};

struct				gsx_token {
	enum gsx_token_kind	kind;
	struct view			text;
	struct location		loc;
};

/*	 GSX_Lexer	*/

struct				gsx_lexer {
	struct view		text;
	struct location	loc_cursor;
	struct location	loc_offset;
	size_t			offset;
	size_t			index;
	bool			ok;
};

bool				gsx_is_digit(char c) {
	return (c >= '0' && c <= '9');
}

bool				gsx_is_ident_prefix(char c) {
	if (c >= 'a' && c <= 'z') return (true);
	if (c >= 'A' && c <= 'Z') return (true);
	return (false);
}

bool				gsx_is_ident_infix(char c) {
	if (gsx_is_digit(c)) return (true);
	if (gsx_is_ident_prefix(c)) return (true);
	if (c == '/' || c == '_') return (true);
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

	if (location_advance(&lexer->loc_cursor, peek))
		lexer->index++;
	return (peek);
}

void				gsx_lexer_skip(struct gsx_lexer* lexer) {
	if (!gsx_lexer_eof(lexer))
		gsx_lexer_next(lexer);
	lexer->offset = lexer->index;
	lexer->loc_offset = lexer->loc_cursor;
}

bool				gsx_lexer_ahead(struct gsx_lexer* lexer, const char* ahead) {
	struct view	view = view_drop(lexer->text, lexer->index);
	return (view_prefix(view, ahead));
}

struct view			gsx_lexer_peek_view(struct gsx_lexer* lexer) {
	if (lexer == NULL || lexer->offset >= lexer->index) return ((struct view){ NULL, 0ul });
	size_t				drop = lexer->offset;
	size_t				take = lexer->index - lexer->offset;
	return (view_drop_take(lexer->text, drop, take));
}

struct gsx_token	gsx_lexer_next_token(struct gsx_lexer* lexer, enum gsx_token_kind kind) {
	if (lexer == NULL || lexer->offset >= lexer->index) return ((struct gsx_token){ 0 });
	size_t				drop = lexer->offset;
	size_t				take = lexer->index - lexer->offset;

	struct view			text = view_drop_take(lexer->text, drop, take);
	struct gsx_token	token = { .text = text, .kind = kind, .loc = lexer->loc_offset };

	lexer->offset = lexer->index;
	lexer->loc_offset = lexer->loc_cursor;
	return (token);
}

bool				gsx_lexer_tokenize(struct gsx_section statement, struct dynamic_array* tokens) {
	struct gsx_lexer	lexer = {
		.text = statement.text,
		.loc_cursor = statement.loc,
		.loc_offset = statement.loc,
		.ok = true
	};
	while (!gsx_lexer_eof(&lexer)) {
		while (!gsx_lexer_eof(&lexer)) {
			char	letter = gsx_lexer_peek(&lexer);
			if (letter != ' ' && letter != '\t') break ;
			gsx_lexer_next(&lexer);
		}
		(void)gsx_lexer_next_token(&lexer, GSX_TOKEN_INVALID);

		struct gsx_token	token = { 0 };
		char				c = gsx_lexer_peek(&lexer);
		if (c == '\0') break ;
		if (gsx_lexer_ahead(&lexer, "<!--")) {
			for (size_t i = 0ul; i < 4ul; ++i)
				gsx_lexer_next(&lexer);
			token = gsx_lexer_next_token(&lexer, GSX_TOKEN_HTML_OPEN);
		} else if (gsx_lexer_ahead(&lexer, "-->")) {
			for (size_t i = 0ul; i < 3ul; ++i)
				gsx_lexer_next(&lexer);
			token = gsx_lexer_next_token(&lexer, GSX_TOKEN_HTML_CLOSE);
		} else if (gsx_lexer_ahead(&lexer, "gsx:")) {
			for (size_t i = 0ul; i < 4ul; ++i)
				gsx_lexer_next(&lexer);
			token = gsx_lexer_next_token(&lexer, GSX_TOKEN_HTML_SIGNATURE);
		} else if (c == ':') {
			gsx_lexer_next(&lexer);
			token = gsx_lexer_next_token(&lexer, GSX_TOKEN_COLON);
		} else if (c == ',') {
			gsx_lexer_next(&lexer);
			token = gsx_lexer_next_token(&lexer, GSX_TOKEN_COMMA);
		} else if (c == '(') {
			gsx_lexer_next(&lexer);
			token = gsx_lexer_next_token(&lexer, GSX_TOKEN_PAREN_OPEN);
		} else if (c == ')') {
			gsx_lexer_next(&lexer);
			token = gsx_lexer_next_token(&lexer, GSX_TOKEN_PAREN_CLOSE);
		} else if (c == '[') {
			gsx_lexer_next(&lexer);
			token = gsx_lexer_next_token(&lexer, GSX_TOKEN_SQUARE_OPEN);
		} else if (c == ']') {
			gsx_lexer_next(&lexer);
			token = gsx_lexer_next_token(&lexer, GSX_TOKEN_SQUARE_CLOSE);
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
		} else if (gsx_is_digit(c) || c == '-') {
			if (c == '-')
				gsx_lexer_next(&lexer);
			size_t	count = 0ul;
			while (!gsx_lexer_eof(&lexer)) {
				char	digit = gsx_lexer_peek(&lexer);
				if (!gsx_is_digit(digit)) break ;
				gsx_lexer_next(&lexer);
				count++;
			}
			token = gsx_lexer_next_token(&lexer, (count > 0ul) ? GSX_TOKEN_INTEGER : GSX_TOKEN_INVALID);
		} else if (gsx_is_ident_prefix(c)) {
			char				letter = '\0';
			while (!gsx_lexer_eof(&lexer)) {
				letter = gsx_lexer_peek(&lexer);
				if (!gsx_is_ident_infix(letter)) break ;
				gsx_lexer_next(&lexer);
			}
			enum gsx_token_kind	kind = letter == '!' ? GSX_TOKEN_CALL : GSX_TOKEN_IDENT;

			token = gsx_lexer_next_token(&lexer, kind);
			if (kind == GSX_TOKEN_CALL) gsx_lexer_skip(&lexer);
		}
		else {
			gsx_lexer_next(&lexer);
			token = gsx_lexer_next_token(&lexer, GSX_TOKEN_INVALID);
			lexer.ok = false;
		}
		da_push(tokens, &token);
	}
	return (lexer.ok);
}

/*	 GSX_Parser	*/

struct				gsx_parser {
	struct gsx_virtual_machine*	vm;
	const struct dynamic_array*	tokens; /* type: struct gsx_token */
	struct gsx_token			current;
	size_t						index;
	bool						ok;
};

bool				gsx_parser_eof(struct gsx_parser* parser) {
	if (parser == NULL || parser->index >= parser->tokens->len)
		return (true);
	struct gsx_token	token = da_at(struct gsx_token, parser->tokens, parser->index);
	return (token.kind == GSX_TOKEN_HTML_CLOSE);
}

struct gsx_token	gsx_parser_peek(struct gsx_parser* parser) {
	if (gsx_parser_eof(parser))
		return ((struct gsx_token){ 0 });
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
		print(ttyerr, "%cstr%: "TERMINAL_NOTICE_ERROR":\texpected %gsx_token_kind%, got %gsx_token% instead\n", program_name, kind, gsx_parser_peek(parser));
		return (parser->ok = false);
	}
	return (true);
}

bool				gsx_parser_expect_any(struct gsx_parser* parser, enum gsx_token_kind* kinds, size_t kinds_count) {
	if (!gsx_parser_accept_any(parser, kinds, kinds_count)) {
		print(ttyerr, "%cstr%: "TERMINAL_NOTICE_ERROR":\texpected ", program_name);
		for (size_t i = 0; i < kinds_count; ++i) {
			print(ttyerr, "%gsx_token_kind%", kinds[i]);
			if (i + 1 < kinds_count) print(ttyerr, " | ");
		}
		print(ttyerr, ", got %gsx_token% instead\n", gsx_parser_peek(parser));
		return (parser->ok = false);
	}
	return (true);
}

size_t				gsx_parser_remaining(struct gsx_parser* parser) {
	if (gsx_parser_eof(parser)) return (0ul);
	return (parser->tokens->len - parser->index);
}

void				gsx_parser_exhaust(struct gsx_parser* parser) {
	while (!gsx_parser_eof(parser))
		gsx_parser_next(parser);
}

/*	 GSX_Box	*/

enum			gsx_box_kind {
	GSX_BOX_INVALID,

	GSX_BOX_CHAR,
	GSX_BOX_STRING,
	GSX_BOX_INTEGER,
	GSX_BOX_ARRAY,
};

struct			gsx_box {
	enum gsx_box_kind			kind;
	union {
		char					character;
		long					integer;
		char*					str;
		struct dynamic_array	array; /* type: struct gsx_box* */
	}							data;
};

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

struct gsx_box*	gsx_box_new_view(struct view string) {
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

struct gsx_box*	gsx_box_clone(struct gsx_box* ref) {
	if (ref == NULL) return (NULL);
	switch (ref->kind) {
		case GSX_BOX_CHAR: return (gsx_box_new_char(ref->data.character));
		case GSX_BOX_STRING: return (gsx_box_new_string(ref->data.str));
		case GSX_BOX_INTEGER: return (gsx_box_new_integer(ref->data.integer));
		case GSX_BOX_ARRAY: {
			struct dynamic_array*	src = &ref->data.array;
			struct dynamic_array	dst = da_make(sizeof(struct gsx_box*));
			da_foreach_begin(src_box, src, struct gsx_box*);
				struct gsx_box*	dst_box = gsx_box_clone(src_box);
				da_push(&dst, &dst_box);
			da_foreach_end();
			return (gsx_box_new_array(dst));
		} break ;
		case GSX_BOX_INVALID:
		default: return (NULL);
	}
}

void			gsx_box_free(struct gsx_box* box) {
	if (box != NULL) switch (box->kind) {
		case GSX_BOX_ARRAY: {
			struct dynamic_array*	array = &box->data.array;
			if (array == NULL) break ;
			da_foreach_begin(elem, array, struct gsx_box*);
				gsx_box_free(elem);
			da_foreach_end();
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

/*	GSX_Type	*/

enum			gsx_type {
	GSX_TYPE_INVALID,

	GSX_TYPE_CHAR,
	GSX_TYPE_STRING,
	GSX_TYPE_INTEGER,
	GSX_TYPE_ARRAY,

	GSX_TYPE_VOID,
};

enum gsx_type	gsx_type_of_box(struct gsx_box* box) {
	if (box == NULL) return (GSX_TYPE_VOID);
	switch (box->kind) {
		case GSX_BOX_CHAR: return (GSX_TYPE_CHAR);
		case GSX_BOX_STRING: return (GSX_TYPE_STRING);
		case GSX_BOX_INTEGER: return (GSX_TYPE_INTEGER);
		case GSX_BOX_ARRAY: return (GSX_TYPE_ARRAY);
		case GSX_BOX_INVALID:
		default: return (GSX_TYPE_INVALID);
	}
}

/*	 GSX_Syntax_Tree	*/

enum							gsx_ast_kind {
	GSX_AST_INVALID,
	GSX_AST_CALL,
	GSX_AST_IDENT,
	GSX_AST_LITERAL,
	GSX_AST_DEFINE,
	GSX_AST_LIST,
};

struct							gsx_ast_call {
	struct gsx_token		name;
	struct dynamic_array	args; /* type: struct gsx_ast* */
};

struct							gsx_ast_ident {
	struct gsx_token	name;
};

struct							gsx_ast_literal {
	struct gsx_token	token;
};

enum							gsx_ast_param_kind {
	GSX_PARAM_INVALID,

	GSX_PARAM_IDENT,
	GSX_PARAM_LITERAL,
	GSX_PARAM_DESTRUCTURE,
};

struct							gsx_ast_param {
	enum gsx_ast_param_kind		kind;
	enum gsx_type				type;
	struct view					name;
	struct gsx_ast_destructure*	destructure;
};

struct							gsx_ast_destructure {
	struct dynamic_array	parts; /* type: struct gsx_ast_param */
};

struct							gsx_ast_define {
	struct gsx_token		token;
	struct gsx_token		name;
	struct dynamic_array	params;	/* elem: struct gsx_ast_param* */
	struct gsx_ast*			value;

	bool					is_function;
};

struct							gsx_ast_list {
	struct gsx_token		open;
	struct gsx_token		close;
	struct dynamic_array	elems;	/* elem: struct gsx_ast* */
};

struct							gsx_ast {
	enum gsx_ast_kind		kind;
	union {
		struct gsx_ast_call*		call;
		struct gsx_ast_list*		list;
		struct gsx_ast_ident*		ident;
		struct gsx_ast_literal*		literal;
		struct gsx_ast_define*		define;
	}					data;
};

struct gsx_ast*					gsx_clone_ast_call(struct gsx_ast_call* call) {
	if (call == NULL) return (NULL);

	struct gsx_ast*			ast = malloc(sizeof(struct gsx_ast) + sizeof(struct gsx_ast_call));
	if (ast == NULL) return (NULL);

	struct gsx_ast_call*	clone = (struct gsx_ast_call*)(&((char*)ast)[sizeof(struct gsx_ast)]);
	clone->name = call->name;
	clone->args = da_make(sizeof(struct gsx_ast*));
	for (size_t i = 0ul; i < call->args.len; ++i) {
		struct gsx_ast*	arg = da_at(struct gsx_ast*, &call->args, i);
		struct gsx_ast* arg_clone = gsx_clone_ast(arg);
		da_push(&clone->args, &arg_clone);
	}

	ast->kind = GSX_AST_CALL;
	ast->data.call = call;
	return (ast);
}

struct gsx_ast*					gsx_clone_ast_literal(struct gsx_ast_literal* literal) {
	if (literal == NULL) return (NULL);

	struct gsx_ast*			ast = malloc(sizeof(struct gsx_ast) + sizeof(struct gsx_ast_literal));
	if (ast == NULL) return (NULL);

	struct gsx_ast_literal*	clone = (struct gsx_ast_literal*)(&((char*)ast)[sizeof(struct gsx_ast)]);
	clone->token = literal->token;

	ast->kind = GSX_AST_LITERAL;
	ast->data.literal = clone;
	return (ast);
}

struct gsx_ast*					gsx_clone_ast_ident(struct gsx_ast_ident* ident) {
	if (ident == NULL) return (NULL);

	struct gsx_ast*			ast = malloc(sizeof(struct gsx_ast) + sizeof(struct gsx_ast_ident));
	if (ast == NULL) return (NULL);

	struct gsx_ast_ident*	clone = (struct gsx_ast_ident*)(&((char*)ast)[sizeof(struct gsx_ast)]);
	clone->name = ident->name;

	ast->kind = GSX_AST_IDENT;
	ast->data.ident = clone;
	return (ast);
}

struct gsx_ast*					gsx_clone_ast(struct gsx_ast* ref) {
	switch (ref->kind) {
		case GSX_AST_CALL: return (gsx_clone_ast_call(ref->data.call));
		case GSX_AST_DEFINE: assert(false && "why are you trying to clone a gsx_ast_define?"); break ; // return (gsx_clone_ast_define(ref->data.define));
		case GSX_AST_LIST: assert(false && "i am not implementing this bitch"); break ;
		case GSX_AST_LITERAL: return (gsx_clone_ast_literal(ref->data.literal));
		case GSX_AST_IDENT: return (gsx_clone_ast_ident(ref->data.ident));
		case GSX_AST_INVALID: return (NULL);
		default: assert(false && "gsx_ast_clone: unreachable");
	}
}

void							gsx_free_ast(struct gsx_ast* node) {
	if (node == NULL) return ;
	switch (node->kind) {
		case GSX_AST_LIST: {
			struct gsx_ast_list*	list = node->data.list;
			da_foreach_begin(elem, &list->elems, struct gsx_ast*);
				gsx_free_ast(elem);
			da_foreach_end();
			da_free(&list->elems);
		} break;
		case GSX_AST_CALL: {
			struct gsx_ast_call*	call = node->data.call;
			da_foreach_begin(arg, &call->args, struct gsx_ast*);
				gsx_free_ast(arg);
			da_foreach_end();
			da_free(&call->args);
		} break ;
		case GSX_AST_DEFINE: {
			struct gsx_ast_define*	define = node->data.define;
			da_free(&define->params);
			gsx_free_ast(define->value);
		} break ;
		case GSX_AST_LITERAL:
		case GSX_AST_IDENT:
		case GSX_AST_INVALID:
		default: { } break ;
	}
	free(node);
}

struct gsx_ast_param*			gsx_new_ast_param(enum gsx_ast_param_kind kind, enum gsx_type type, struct gsx_ast_destructure* destructure) {
	struct gsx_ast_param*	param = malloc(sizeof(struct gsx_ast_param));
	if (param != NULL) {
		param->name = (struct view){ .data = NULL, .len = 0 };
		param->kind = kind;
		param->type = type;
		param->destructure = destructure;
	}
	return (param);
}

struct gsx_ast_param*			gsx_new_ast_param_named(struct view name, enum gsx_ast_param_kind kind, enum gsx_type type, struct gsx_ast_destructure* destructure) {
	struct gsx_ast_param*	param = malloc(sizeof(struct gsx_ast_param));
	if (param != NULL) {
		param->name = name;
		param->kind = kind;
		param->type = type;
		param->destructure = destructure;
	}
	return (param);
}

void							gsx_free_ast_param(struct gsx_ast_param* param) {
	if (param == NULL) return ;

	switch (param->kind) {
		case GSX_PARAM_DESTRUCTURE: /* TODO(xenobas): free the param->destructure */ break ;
		case GSX_PARAM_IDENT:
		case GSX_PARAM_LITERAL:
		case GSX_PARAM_INVALID:
		default: break ;
	}
	free(param);
}

/*	GSX_Internal	*/

struct gsx_box*		gsx_internal_stub(struct gsx_virtual_machine* vm, struct dynamic_array* args) {
	(void)vm;
	(void)args;
	print(ttyerr, "%cstr%: " TERMINAL_NOTICE_TODO ":\tcalled gsx_internal_stub.\n", program_name);
	return (NULL);
}

struct gsx_box*		gsx_internal_debug(struct gsx_virtual_machine* vm, struct dynamic_array* args) {
	(void)vm;
	struct gsx_box*	arg_text = da_at(struct gsx_box*, args, 0);
	const char*		text = arg_text->data.str;

	print(ttyout, "%cstr%: " TERMINAL_NOTICE_DEBUG ":\t%cstr%\n", program_name, text);
	return (NULL);
}

struct gsx_box*		gsx_internal_include(struct gsx_virtual_machine* vm, struct dynamic_array* args) {
	struct gsx_box*			ret = NULL;
	struct gsx_box*			arg_path = da_at(struct gsx_box*, args, 0);
	const char*				include_path = arg_path->data.str;
	(void)vm;
	(void)arg_path;
	(void)include_path;
	return (ret);
}

struct gsx_box*		gsx_internal_replace(struct gsx_virtual_machine* vm, struct dynamic_array* args) {
	(void)vm;
	struct gsx_box*	arg_text = da_at(struct gsx_box*, args, 0);
	struct gsx_box*	arg_pattern = da_at(struct gsx_box*, args, 1);
	struct gsx_box*	arg_replace = da_at(struct gsx_box*, args, 2);

	const char*		text = arg_text->data.str;
	const char*		pattern = arg_pattern->data.str;
	const char*		replace = arg_replace->data.str;

	char*			result = cstr_replace(text, pattern, replace);
	if (result == NULL) return (NULL);
	struct gsx_box*	retval = gsx_box_new_string(result);
	return (free(result), retval);
}

struct gsx_box*		gsx_internal_sum(struct gsx_virtual_machine* vm, struct dynamic_array* args) {
	(void)vm;
	struct gsx_box*	arg_lhs = da_at(struct gsx_box*, args, 0);
	struct gsx_box*	arg_rhs = da_at(struct gsx_box*, args, 1);

	const long		lhs = arg_lhs->data.integer;
	const long		rhs = arg_rhs->data.integer;
	return (gsx_box_new_integer(lhs + rhs));
}

struct gsx_box*		gsx_internal_sub(struct gsx_virtual_machine* vm, struct dynamic_array* args) {
	(void)vm;
	struct gsx_box*	arg_lhs = da_at(struct gsx_box*, args, 0);
	struct gsx_box*	arg_rhs = da_at(struct gsx_box*, args, 1);

	const long		lhs = arg_lhs->data.integer;
	const long		rhs = arg_rhs->data.integer;
	return (gsx_box_new_integer(lhs - rhs));
}

struct gsx_box*		gsx_internal_mul(struct gsx_virtual_machine* vm, struct dynamic_array* args) {
	(void)vm;
	struct gsx_box*	arg_lhs = da_at(struct gsx_box*, args, 0);
	struct gsx_box*	arg_rhs = da_at(struct gsx_box*, args, 1);

	const long		lhs = arg_lhs->data.integer;
	const long		rhs = arg_rhs->data.integer;
	return (gsx_box_new_integer(lhs * rhs));
}

struct gsx_box*		gsx_internal_div(struct gsx_virtual_machine* vm, struct dynamic_array* args) {
	(void)vm;
	struct gsx_box*	arg_lhs = da_at(struct gsx_box*, args, 0);
	struct gsx_box*	arg_rhs = da_at(struct gsx_box*, args, 1);

	const long		lhs = arg_lhs->data.integer;
	const long		rhs = arg_rhs->data.integer;
	if (rhs == 0l) {
		print(ttyerr, "%cstr%: " TERMINAL_NOTICE_ERROR ":\tcannot divide %long% by 0\n", program_name, lhs);
		return (NULL);
	}
	return (gsx_box_new_integer(lhs / rhs));
}

struct gsx_box*		gsx_internal_mod(struct gsx_virtual_machine* vm, struct dynamic_array* args) {
	(void)vm;
	struct gsx_box*	arg_lhs = da_at(struct gsx_box*, args, 0);
	struct gsx_box*	arg_rhs = da_at(struct gsx_box*, args, 1);

	const long		lhs = arg_lhs->data.integer;
	const long		rhs = arg_rhs->data.integer;
	if (rhs == 0l) {
		print(ttyerr, "%cstr%: " TERMINAL_NOTICE_ERROR ":\tcannot divide %long% by 0\n", program_name, lhs);
		return (NULL);
	}
	return (gsx_box_new_integer(lhs % rhs));
}

struct gsx_box*		gsx_internal_factorial(struct gsx_virtual_machine* vm, struct dynamic_array* args) {
	(void)vm;
	struct gsx_box*	arg_n = da_at(struct gsx_box*, args, 0);
	const long		n = arg_n->data.integer;

	return (gsx_box_new_integer(math_factorial(n)));
}


/*	GSX_Definition	*/

bool					gsx_definition_is_function(struct gsx_definition* def) {
	if (def == NULL) return (false);
	return (def->kind == GSX_DEFINITION_FUNCTION || def->kind == GSX_DEFINITION_INTERNAL_FUNCTION);
}

struct gsx_definition	gsx_definition_make_user_value(enum gsx_definition_kind kind, struct view name, struct dynamic_array params, struct gsx_ast* ast) {
	struct gsx_definition	def = {
		.name = name,
		.kind = kind,
		.params = params,
		.data = { .ast = ast },
	};
	return (def);
}

struct gsx_definition	gsx_definition_make_internal_function(struct view name, struct dynamic_array params, gsx_definition_internal internal) {
	struct gsx_definition	def = {
		.name = name,
		.kind = GSX_DEFINITION_INTERNAL_FUNCTION,
		.params = params,
		.data = { .internal = internal },
	};
	return (def);
}

struct gsx_definition	gsx_definition_make_internal_constant(struct view name, struct gsx_ast* ast) {
	struct dynamic_array	params = da_make(sizeof(struct gsx_ast_param*));
	struct gsx_definition	def = {
		.name = name,
		.kind = GSX_DEFINITION_INTERNAL_CONSTANT,
		.params = params,
		.data = { .ast = ast },
	};
	return (def);
}

struct gsx_definition	gsx_definition_make_function(struct view name, struct dynamic_array params, struct gsx_ast* ast) {
	struct gsx_definition	def = {
		.name = name,
		.kind = GSX_DEFINITION_FUNCTION,
		.params = params,
		.data = { .ast = ast },
	};
	return (def);
}

struct gsx_definition	gsx_definition_make_constant(struct view name, struct gsx_ast* ast) {
	struct dynamic_array	params = da_make(sizeof(struct gsx_ast_param*));
	struct gsx_definition	def = {
		.name = name,
		.kind = GSX_DEFINITION_CONSTANT,
		.params = params,
		.data = { .ast = ast },
	};
	return (def);
}

void					gsx_definition_push(struct dynamic_array* definitions, struct gsx_definition def) {
	struct gsx_definition* slot = gsx_definition_get(definitions, def.name);
	if (slot != NULL) {
		slot->kind = def.kind;
		slot->data = def.data;
		slot->params = def.params;
	} else da_push(definitions, &def);
}

bool					gsx_definition_pop(struct dynamic_array* definitions) {
	return (da_pop(definitions));
}

void					gsx_definition_free(struct gsx_definition* def) {
	if (def == NULL) return ;

	switch (def->kind) {
		case GSX_DEFINITION_INTERNAL_FUNCTION: {
			for (size_t i = 0ul; i < def->params.len; ++i) {
				struct gsx_ast_param*	param = da_at(struct gsx_ast_param*, &def->params, i);
				gsx_free_ast_param(param);
			}
		} break ;
		case GSX_DEFINITION_INTERNAL_CONSTANT: {
			gsx_free_ast(def->data.ast);
		} break ;
		case GSX_DEFINITION_INVALID:
		case GSX_DEFINITION_CONSTANT:
		case GSX_DEFINITION_FUNCTION:
		default: break ;
	}
	da_free(&def->params);
	mem_fill(def, 0, sizeof(struct gsx_definition));
}

void					gsx_init_definitions_constant(struct dynamic_array* definitions, const char* name, const char* val) {
	struct gsx_ast*			def_ast = malloc(sizeof(struct gsx_ast) + sizeof(struct gsx_ast_literal));
	struct gsx_ast_literal*	literal = (struct gsx_ast_literal*)((char *)def_ast + sizeof(struct gsx_ast));

	if (def_ast != NULL) {
		literal->token.text = view_make_cstr_const(val);
		literal->token.kind = GSX_TOKEN_STRING;

		def_ast->kind = GSX_AST_LITERAL;
		def_ast->data.literal = literal;

		struct gsx_definition	def = gsx_definition_make_internal_constant(view_make_cstr_const(name), def_ast);
		da_push(definitions, &def);
	} else print(ttyerr, "%cstr%: " TERMINAL_NOTICE_ERROR ":\tcould not allocate definition for constant %cstr%\n", program_name, name);
}

void					gsx_init_definitions_function(struct dynamic_array* definitions, const char* name, struct dynamic_array params, gsx_definition_internal internal) {
	if (internal == NULL) {
		print(ttyerr, "%cstr%: " TERMINAL_NOTICE_ERROR ":\tcannot define call %cstr% to be the function at %addr%\n", program_name, name, internal);
		return ;
	}

	struct gsx_definition	def = gsx_definition_make_internal_function(view_make_cstr_const(name), params, internal);
	da_push(definitions, &def);
}

void					gsx_init_definitions(struct dynamic_array* definitions) {
	gsx_init_definitions_constant(definitions, "std/TITLE", "\"index.html\"");

	struct dynamic_array	params_define = da_make(sizeof(struct gsx_ast_param*));
	gsx_init_definitions_function(definitions, "std/define", params_define, gsx_internal_stub);

	struct dynamic_array	params_debug = da_make(sizeof(struct gsx_ast_param*));
	for (size_t i = 0ul; i < 1ul; ++i) {
		struct gsx_ast_param*	param = gsx_new_ast_param(GSX_PARAM_IDENT, GSX_TYPE_STRING, NULL);
		da_push(&params_debug, &param);
	}
	gsx_init_definitions_function(definitions, "std/debug", params_debug, gsx_internal_debug);

	struct dynamic_array	params_env = da_make(sizeof(struct gsx_ast_param*));
	for (size_t i = 0ul; i < 1ul; ++i) {
		struct gsx_ast_param*	param = gsx_new_ast_param(GSX_PARAM_IDENT, GSX_TYPE_STRING, NULL);
		da_push(&params_env, &param);
	}
	gsx_init_definitions_function(definitions, "std/env", params_env, gsx_internal_stub);

	struct dynamic_array	params_include = da_make(sizeof(struct gsx_ast_param*));
	for (size_t i = 0ul; i < 1ul; ++i) {
		struct gsx_ast_param*	param = gsx_new_ast_param(GSX_PARAM_IDENT, GSX_TYPE_STRING, NULL);
		da_push(&params_include, &param);
	}
	gsx_init_definitions_function(definitions, "std/include", params_include, gsx_internal_include);

	struct dynamic_array	params_replace = da_make(sizeof(struct gsx_ast_param*));
	for (size_t i = 0ul; i < 3ul; ++i) {
		struct gsx_ast_param*	param = gsx_new_ast_param(GSX_PARAM_IDENT, GSX_TYPE_STRING, NULL);
		da_push(&params_replace, &param);
	}
	gsx_init_definitions_function(definitions, "std/replace", params_replace, gsx_internal_replace);

	struct dynamic_array	params_factorial = da_make(sizeof(struct gsx_ast_param*));
	for (size_t i = 0ul; i < 1ul; ++i) {
		struct gsx_ast_param*	param = gsx_new_ast_param(GSX_PARAM_IDENT, GSX_TYPE_INTEGER, NULL);
		da_push(&params_factorial, &param);
	}
	gsx_init_definitions_function(definitions, "std/factorial", params_factorial, gsx_internal_factorial);

	struct dynamic_array	params_sum = da_make(sizeof(struct gsx_ast_param*));
	for (size_t i = 0ul; i < 2ul; ++i) {
		struct gsx_ast_param*	param = gsx_new_ast_param(GSX_PARAM_IDENT, GSX_TYPE_INTEGER, NULL);
		da_push(&params_sum, &param);
	}
	gsx_init_definitions_function(definitions, "std/sum", params_sum, gsx_internal_sum);

	struct dynamic_array	params_sub = da_make(sizeof(struct gsx_ast_param*));
	for (size_t i = 0ul; i < 2ul; ++i) {
		struct gsx_ast_param*	param = gsx_new_ast_param(GSX_PARAM_IDENT, GSX_TYPE_INTEGER, NULL);
		da_push(&params_sub, &param);
	}
	gsx_init_definitions_function(definitions, "std/sub", params_sub, gsx_internal_sub);

	struct dynamic_array	params_mul = da_make(sizeof(struct gsx_ast_param*));
	for (size_t i = 0ul; i < 2ul; ++i) {
		struct gsx_ast_param*	param = gsx_new_ast_param(GSX_PARAM_IDENT, GSX_TYPE_INTEGER, NULL);
		da_push(&params_mul, &param);
	}
	gsx_init_definitions_function(definitions, "std/mul", params_mul, gsx_internal_mul);

	struct dynamic_array	params_div = da_make(sizeof(struct gsx_ast_param*));
	for (size_t i = 0ul; i < 2ul; ++i) {
		struct gsx_ast_param*	param = gsx_new_ast_param(GSX_PARAM_IDENT, GSX_TYPE_INTEGER, NULL);
		da_push(&params_div, &param);
	}
	gsx_init_definitions_function(definitions, "std/div", params_div, gsx_internal_div);

	struct dynamic_array	params_mod = da_make(sizeof(struct gsx_ast_param*));
	for (size_t i = 0ul; i < 2ul; ++i) {
		struct gsx_ast_param*	param = gsx_new_ast_param(GSX_PARAM_IDENT, GSX_TYPE_INTEGER, NULL);
		da_push(&params_mod, &param);
	}
	gsx_init_definitions_function(definitions, "std/mod", params_mod, gsx_internal_mod);
}

void					gsx_free_definitions(struct dynamic_array* definitions) {
	if (definitions == NULL) return ;

	for (size_t i = 0; i < definitions->len; ++i) {
		struct gsx_definition	def = da_at(struct gsx_definition, definitions, i);
		gsx_definition_free(&def);
	}
	da_free(definitions);
}

/*	GSX_Template	*/
struct gsx_template {
	const struct view		path;
	const struct view		source; /* attr: read only mmap */
	struct string_builder	output;

	struct dynamic_array	statements; /* type: struct gsx_statement */
	struct gsx_ast*			root;
};

struct gsx_template		gsx_make_template(const struct view path, const struct view source) {
	struct gsx_template	tmpl = {
		.path = path,
		.source = source,
		.output = string_builder_make(),

		.statements = da_make(sizeof(struct gsx_statement)),
		.root = NULL,
	};
	return (tmpl);
}

void					gsx_destroy_template(struct gsx_template* tmpl) {
	string_builder_destroy(&tmpl->output);

	da_foreach_begin(statement, &tmpl->statements, struct gsx_statement);
		da_free(&statement.tokens);
		gsx_free_ast(statement.ast);
	da_foreach_end();
	da_free(&tmpl->statements);

	mem_fill(tmpl, 0, sizeof(struct gsx_template));
}

void					gsx_split_template(struct gsx_template* tmpl) {
	struct location			loc = {
		.index = 0ul,
		.line = 1ul,
		.column = 1ul,
		.path = tmpl->path,
	};
	while (loc.index < tmpl->source.len) {
		size_t					len = 0ul;
		size_t					idx = loc.index;

		struct view				source = view_drop(tmpl->source, loc.index);
		struct location			location = { 0 };
		struct gsx_section		section = {
			.loc			= location,
			.text			= { 0 },
			.is_comment		= false,
			.is_statement	= false,
			.is_terminated	= false,
		};

		mem_copy(&section.loc, &loc, sizeof(struct location));
		if (view_prefix(source, "<!--")) {
			section.is_comment = true;
			for (size_t i = 0ul; i < 4ul; ++i) {
				len++;
				location_advance(&loc, tmpl->source.data[loc.index]);
			}
			while (loc.index < tmpl->source.len) {
				struct view		view = view_drop(tmpl->source, loc.index);
				section.is_statement = view_prefix(view, "gsx:");
				if (section.is_statement || !cstr_contains_char(" \t\r\n", tmpl->source.data[loc.index])) break ;

				len++;
				location_advance(&loc, tmpl->source.data[loc.index]);
			}
			while (loc.index < tmpl->source.len) {
				struct view		view = view_drop(tmpl->source, loc.index);

				section.is_terminated = view_prefix(view, "-->");
				if (section.is_terminated) {
					location_advance(&loc, tmpl->source.data[loc.index]); len++;
					location_advance(&loc, tmpl->source.data[loc.index]); len++;
					location_advance(&loc, tmpl->source.data[loc.index]); len++;
					break ;
				}

				len++;
				location_advance(&loc, tmpl->source.data[loc.index]);
			}
		} else {
			len = 0ul;
			while (loc.index < tmpl->source.len) {
				struct view	trailing = view_drop(tmpl->source, loc.index);
				if (view_prefix(trailing, "<!--")) break ;

				location_advance(&loc, source.data[len++]);
			}
		}
		section.text = view_drop_take(tmpl->source, idx, len);

		struct gsx_statement	statement = {
			.ast = NULL,
			.tokens = da_make(sizeof(struct gsx_token)),
			.section = section,
		};
		da_push(&tmpl->statements, &statement);
	}
}

void					gsx_write_template_output(struct gsx_template* tmpl, struct gsx_section section, struct gsx_box* box) {
	if (box == NULL) return ;

	switch (box->kind) {
		case GSX_BOX_CHAR: string_builder_write_char(&tmpl->output, box->data.character); break ;
		case GSX_BOX_STRING: string_builder_write_cstr(&tmpl->output, box->data.str); break ;
		case GSX_BOX_INTEGER: string_builder_write_long(&tmpl->output, box->data.integer); break ;
		case GSX_BOX_ARRAY: {
			struct location loc = section.loc;
			print(ttyerr, "%view%:%ulong%:%ulong%: " TERMINAL_NOTICE_TODO ": write_output(kind = ARRAY)\n", loc.path, loc.line, loc.column);
		} break ;
		case GSX_BOX_INVALID: {
			struct location loc = section.loc;
			print(ttyerr, "%view%:%ulong%:%ulong%: " TERMINAL_NOTICE_ERROR ": cannot write invalid box\n", loc.path, loc.line, loc.column);
		} break ;
		default: {
			struct location loc = section.loc;
			print(ttyerr, "%view%:%ulong%:%ulong%: " TERMINAL_NOTICE_ERROR ": cannot write unreachable box %int%\n", loc.path, loc.line, loc.column, (int)box->kind);
		} break ;
	}
}

/*	GSX_Neo_Parser */

struct gsx_ast*			gsx_parse_neo_ast(struct gsx_parser* parser);
struct gsx_definition*	gsx_vm_get_definition(struct gsx_virtual_machine* vm, struct view name);

struct gsx_ast*			gsx_new_ast(enum gsx_ast_kind kind) {
	size_t	alloc_size = sizeof(struct gsx_ast);
	switch (kind) {
		case GSX_AST_LIST: alloc_size += sizeof(struct gsx_ast_list); break ;
		case GSX_AST_CALL: alloc_size += sizeof(struct gsx_ast_call); break ;
		case GSX_AST_LITERAL: alloc_size += sizeof(struct gsx_ast_literal); break ;
		case GSX_AST_IDENT: alloc_size += sizeof(struct gsx_ast_ident); break ;
		case GSX_AST_DEFINE: alloc_size += sizeof(struct gsx_ast_define); break ;
		case GSX_AST_INVALID:
		default: alloc_size += 0ul; break ;
	}

	struct gsx_ast*	ast = malloc(alloc_size);
	if (ast != NULL)
		ast->kind = kind;
	return (ast);
}

struct gsx_ast*			gsx_parse_neo_ast_ident(struct gsx_parser* parser) {
	if (!gsx_parser_expect(parser, GSX_TOKEN_IDENT)) return (NULL);
	struct gsx_token		name = parser->current;

	struct gsx_ast*			ast = gsx_new_ast(GSX_AST_IDENT);
	struct gsx_ast_ident*	ident = (struct gsx_ast_ident*)(&((char*)ast)[sizeof(struct gsx_ast)]);
	if (ast != NULL) {
		ast->data.ident = ident;
		ident->name = name;
	}
	return (ast);
}

struct gsx_ast*			gsx_parse_neo_ast_literal(struct gsx_parser* parser) {
	static enum gsx_token_kind	kinds[] = { GSX_TOKEN_STRING, GSX_TOKEN_CHAR, GSX_TOKEN_INTEGER };
	const size_t				kinds_n = sizeof(kinds) / sizeof(enum gsx_token_kind);

	if (!gsx_parser_expect_any(parser, kinds, kinds_n)) return (NULL);
	struct gsx_token			token = parser->current;

	struct gsx_ast*				ast = gsx_new_ast(GSX_AST_LITERAL);
	struct gsx_ast_literal*		literal = (struct gsx_ast_literal*)(&((char*)ast)[sizeof(struct gsx_ast)]);
	if (ast != NULL) {
		ast->data.literal = literal;
		literal->token = token;
	}
	return (ast);
}

struct gsx_ast*			gsx_parse_neo_ast_list(struct gsx_parser* parser) {
	if (!gsx_parser_expect(parser, GSX_TOKEN_SQUARE_OPEN)) return (NULL);
	struct gsx_token		open = parser->current;

	struct dynamic_array	elems = da_make(sizeof(struct gsx_ast*));
	while (!gsx_parser_eof(parser)) {
		struct gsx_token	ahead = gsx_parser_peek(parser);
		if (ahead.kind == GSX_TOKEN_SQUARE_CLOSE) break ;
		struct gsx_ast*	elem = gsx_parse_neo_ast(parser);
		if (elem != NULL) da_push(&elems, &elem);
		if (!gsx_parser_accept(parser, GSX_TOKEN_COMMA)) break ;
	}

	if (!gsx_parser_expect(parser, GSX_TOKEN_SQUARE_CLOSE)) return (NULL);
	struct gsx_token		close = parser->current;

	struct gsx_ast*			ast = gsx_new_ast(GSX_AST_LIST);
	struct gsx_ast_list*	list = (struct gsx_ast_list*)(&((char*)ast)[sizeof(struct gsx_ast)]);
	if (ast != NULL) {
		list->open = open;
		list->close = close;
		list->elems = elems;
		ast->data.list = list;
	}
	return (ast);
}

bool					gsx_parse_neo_ast_define_param(struct gsx_parser* parser, struct dynamic_array* params) {
	struct gsx_token		token = gsx_parser_peek(parser);
	struct gsx_ast_param*	param = NULL;
	switch (token.kind) {
		case GSX_TOKEN_IDENT: {
			/* TODO(xenobas): Implement type inference */
			param = gsx_new_ast_param_named(token.text, GSX_PARAM_LITERAL, GSX_TYPE_STRING, NULL);
		} break ;
		case GSX_TOKEN_CHAR:
		case GSX_TOKEN_STRING:
		case GSX_TOKEN_INTEGER: {
			enum gsx_type	type = GSX_TYPE_INVALID;
			if (token.kind == GSX_TOKEN_CHAR) type = GSX_TYPE_CHAR;
			if (token.kind == GSX_TOKEN_STRING) type = GSX_TYPE_STRING;
			if (token.kind == GSX_TOKEN_INTEGER) type = GSX_TYPE_INTEGER;
			param = gsx_new_ast_param(GSX_PARAM_LITERAL, type, NULL);
			print(ttyout, "%cstr%: " TERMINAL_NOTICE_DEBUG ":\tdefine_param_literal : %#view% at %addr%\n", program_name, token.text, param);
		} break ;
		case GSX_TOKEN_CALL:
		case GSX_TOKEN_HTML_OPEN:
		case GSX_TOKEN_HTML_CLOSE:
		case GSX_TOKEN_HTML_SIGNATURE:
		case GSX_TOKEN_PAREN_OPEN:
		case GSX_TOKEN_PAREN_CLOSE:
		case GSX_TOKEN_SQUARE_OPEN:
		case GSX_TOKEN_SQUARE_CLOSE:
		case GSX_TOKEN_COMMA:
		case GSX_TOKEN_COLON:
		case GSX_TOKEN_INVALID:
		default: {
			struct location	loc = token.loc;
			print(ttyerr, "%view%:%ulong%:%ulong%: " TERMINAL_NOTICE_ERROR ":\tencountered unexpected token %gsx_token% while parsing define parameters\n", loc.path, loc.line, loc.column, token);
			return (false);
		}
	}
	return (gsx_parser_next(parser), da_push(params, &param), true);
}

struct gsx_ast*			gsx_parse_neo_ast_define_call(struct gsx_parser* parser) {
	struct gsx_token			token = parser->current;
	if (!gsx_parser_expect(parser, GSX_TOKEN_IDENT)) return (NULL);
	struct gsx_token			name = parser->current;

	struct dynamic_array		params = da_make(sizeof(struct gsx_ast_param*));
	bool						is_function = false;
	if (gsx_parser_accept(parser, GSX_TOKEN_PAREN_OPEN)) {
		while (!gsx_parser_eof(parser)) {
			struct gsx_token	ahead = gsx_parser_peek(parser);
			if (ahead.kind == GSX_TOKEN_PAREN_CLOSE) break ;
			if (!gsx_parse_neo_ast_define_param(parser, &params)) break ;
			if (!gsx_parser_accept(parser, GSX_TOKEN_COMMA)) break ;
		}
		if (!gsx_parser_expect(parser, GSX_TOKEN_PAREN_CLOSE)) return (NULL);
		is_function = true;
	}

	struct gsx_ast*				value = gsx_parse_neo_ast(parser);
	if (value == NULL)
		return (NULL);

	struct gsx_ast*				ast = gsx_new_ast(GSX_AST_DEFINE);
	struct gsx_ast_define*		define = (struct gsx_ast_define*)(&((char*)ast)[sizeof(struct gsx_ast)]);
	define->token = token;
	define->name = name;
	define->params = params;
	define->value = value;
	define->is_function = is_function;
	ast->data.define = define;
	return (ast);
}

struct gsx_ast*			gsx_parse_neo_ast_call(struct gsx_parser* parser) {
	if (!gsx_parser_expect(parser, GSX_TOKEN_CALL)) return (NULL);
	struct gsx_token		name = parser->current;

	if (view_equals_cstr(name.text, "std/define"))
		return (gsx_parse_neo_ast_define_call(parser));

	struct gsx_definition*	def = gsx_vm_get_definition(parser->vm, name.text);
	if (def == NULL) {
		struct location	loc = name.loc;
		print(ttyerr, "%view%:%ulong%:%ulong%: " TERMINAL_NOTICE_ERROR ":\tcannot parse call to undefined function %#view%\n", loc.path, loc.line, loc.column, name.text);
		return (NULL);
	}
	if (!gsx_definition_is_function(def)) {
		struct location	loc = name.loc;
		print(ttyerr, "%view%:%ulong%:%ulong%: " TERMINAL_NOTICE_ERROR ":\tcannot parse call to non-function definition %#view%\n", loc.path, loc.line, loc.column, name.text);
		return (NULL);
	}

	struct dynamic_array	args = da_make(sizeof(struct gsx_ast*));
	for (size_t i = 0u; i < def->params.len; ++i) {
		if (gsx_parser_eof(parser)) {
			struct location	loc = name.loc;
			print(ttyerr, "%view%:%ulong%:%ulong%: " TERMINAL_NOTICE_ERROR ":\tincomplete call %#view%\n", loc.path, loc.line, loc.column, name.text);
			print(ttyerr, "\t" TERMINAL_NOTICE_HINT ":\t Expected %ulong% arguments got %ulong%\n", def->params.len, i);
			return (NULL);
		}
		struct gsx_ast*		arg = gsx_parse_neo_ast(parser);
		if (arg != NULL) da_push(&args, &arg);
	}
	struct gsx_ast*			ast = gsx_new_ast(GSX_AST_CALL);
	struct gsx_ast_call*	call = (struct gsx_ast_call*)(&((char*)ast)[sizeof(struct gsx_ast)]);
	if (ast != NULL) {
		ast->data.call = call;
		call->name = name;
		call->args = args;
	}
	return (ast);
}

struct gsx_ast*			gsx_parse_neo_ast(struct gsx_parser* parser) {
	assert(parser != NULL && "gsx_parse_neo_ast was passed an invalid parser pointer");

	struct gsx_token	token = gsx_parser_peek(parser);
	switch (token.kind) {
		case GSX_TOKEN_INTEGER:
		case GSX_TOKEN_STRING:
		case GSX_TOKEN_CHAR: return (gsx_parse_neo_ast_literal(parser));
		case GSX_TOKEN_CALL: return (gsx_parse_neo_ast_call(parser));
		case GSX_TOKEN_IDENT: return (gsx_parse_neo_ast_ident(parser));
		case GSX_TOKEN_SQUARE_OPEN: return (gsx_parse_neo_ast_list(parser));

		case GSX_TOKEN_HTML_OPEN:
		case GSX_TOKEN_HTML_CLOSE:
		case GSX_TOKEN_HTML_SIGNATURE:
		case GSX_TOKEN_PAREN_OPEN:
		case GSX_TOKEN_PAREN_CLOSE:
		case GSX_TOKEN_SQUARE_CLOSE:
		case GSX_TOKEN_COMMA:
		case GSX_TOKEN_COLON:
		case GSX_TOKEN_INVALID:
		default: {
			struct location	loc = token.loc;
			print(ttyerr, "%view%:%ulong%:%ulong%: " TERMINAL_NOTICE_ERROR ":\tencountered unexpected token %gsx_token%\n", loc.path, loc.line, loc.column, token);
		}
	}
	return (NULL);
}

struct gsx_ast*			gsx_parse_neo_ast_statement(struct gsx_virtual_machine* vm, struct gsx_statement* statement) {
	struct gsx_parser	parser = {
		.vm = vm,
		.tokens = &statement->tokens,
		.current = { 0 },
		.index = 0u,
		.ok = true,
	};

	if (!gsx_parser_expect(&parser, GSX_TOKEN_HTML_OPEN)) return (NULL);
	if (!gsx_parser_expect(&parser, GSX_TOKEN_HTML_SIGNATURE)) return (NULL);

	struct gsx_token		lead = gsx_parser_peek(&parser);
	struct gsx_ast*			ast = gsx_parse_neo_ast(&parser);
	if (ast == NULL) return (NULL);
	if (gsx_parser_remaining(&parser)) {
		struct gsx_token	trail = gsx_parser_peek(&parser);
		struct location		loc = statement->section.loc;
		print(ttyerr, "%view%:%ulong%:%ulong%: " TERMINAL_NOTICE_ERROR ":\tmulti value expressions are unsupported\n", loc.path, loc.line, loc.column);
		print(ttyerr, "\tExplanation:\ttoken %#view% trailing the expression %#view%\n", trail.text, lead.text);
		gsx_free_ast(ast);
		return (NULL);
	}
	return (ast);
}

/*	 GSX_Virtual_Machine	*/

struct gsx_virtual_machine	gsx_vm_make(void) {
	struct gsx_virtual_machine	vm = {
		.definitions = da_make(sizeof(struct gsx_definition)),
		.templates = da_make(sizeof(struct gsx_template)),
		.scopes = da_make(sizeof(struct dynamic_array)),

		.ok = true,
		.template_entry = NULL,
	};

	gsx_init_definitions(&vm.definitions);
	return (vm);
}

void						gsx_vm_destroy(struct gsx_virtual_machine* vm) {
	if (vm == NULL) return ;

	da_foreach_begin_index_ref(scope, _scope_index, &vm->scopes, struct dynamic_array);
		da_foreach_begin_index_ref(def, _def_index, scope, struct gsx_definition);
			gsx_definition_free(def);
		da_foreach_end();
	da_foreach_end();
	gsx_free_definitions(&vm->definitions);
	da_foreach_begin_ref(tmpl, &vm->templates, struct gsx_template);
		gsx_destroy_template(tmpl);
	da_foreach_end();
	da_free(&vm->templates);
}

struct gsx_definition*		gsx_vm_get_definition(struct gsx_virtual_machine* vm, struct view name) {
	struct gsx_definition*		def_local = NULL;
	if (vm->scopes.len > 0ul) {
		struct dynamic_array	scope = da_last(struct dynamic_array, &vm->scopes);
		def_local = gsx_definition_get(&scope, name);
		if (def_local != NULL)
			return (def_local);
	}
	return (gsx_definition_get(&vm->definitions, name));
}

struct gsx_definition*		gsx_vm_resolve_definition(struct gsx_virtual_machine* vm, struct gsx_definition* def) {
	if (def == NULL) return (NULL);
	switch (def->kind) {
		case GSX_DEFINITION_CONSTANT:
		case GSX_DEFINITION_FUNCTION:
		case GSX_DEFINITION_INTERNAL_CONSTANT: {
			struct gsx_ast*					ast = def->data.ast;
			if (ast != NULL) switch (ast->kind) {
				case GSX_AST_CALL: {
					struct gsx_ast_call*	call = ast->data.call;
					if (call == NULL) return (NULL);
					return (gsx_vm_get_definition(vm, call->name.text));
				} break ;
				case GSX_AST_IDENT: {
					struct gsx_ast_ident*	ident = ast->data.ident;
					if (ident == NULL) return (NULL);
					struct gsx_definition*	ident_def = gsx_vm_get_definition(vm, ident->name.text);
					return (gsx_vm_resolve_definition(vm, ident_def));
				} break ;
				case GSX_AST_LIST:
				case GSX_AST_LITERAL: return (def); break ;
				case GSX_AST_DEFINE:
				case GSX_AST_INVALID:
				default: return (NULL);
			}
		} break ;
		case GSX_DEFINITION_INTERNAL_FUNCTION: return (def);
		case GSX_DEFINITION_INVALID:
		default: return (NULL);
	}
	return (NULL);
}

struct gsx_definition*		gsx_vm_resolve_ast(struct gsx_virtual_machine* vm, struct gsx_ast* ast) {
	if (ast == NULL) return (NULL);
	switch (ast->kind) {
		case GSX_AST_IDENT: {
			struct gsx_ast_ident*	ident = ast->data.ident;
			struct gsx_definition*	def_surface = gsx_vm_get_definition(vm, ident->name.text);
			struct gsx_definition*	def_resolved = gsx_vm_resolve_definition(vm, def_surface);
			if (def_resolved == NULL) return (NULL);
			return (def_resolved);
		} break ;
		case GSX_AST_CALL:
		case GSX_AST_LIST:
		case GSX_AST_DEFINE:
		case GSX_AST_LITERAL:
		case GSX_AST_INVALID:
		default: return (NULL);
	}
}

bool						gsx_vm_load_entry(struct gsx_virtual_machine* vm, const char* tmpl_path) {
	assert(vm != NULL && "invalid vm passed to gsx_vm_load_entry");
	assert(tmpl_path != NULL && "invalid path passed to gsx_vm_load_entry");
	assert(vm->templates.len == 0ul && "attempt at loading entry multiple times");

	const struct view			path = view_make_cstr_const(tmpl_path);
	const struct view			source = os_file_view(tmpl_path);
	if (source.data == NULL) {
		print(ttyerr, "%cstr%: "TERMINAL_NOTICE_ERROR":\tcould not read file %#view%.\n", program_name, path);
		print(ttyerr, "\t"TERMINAL_NOTICE_USAGE": %cstr% <in.gsx> %#view%\n", program_name, path);
		return (false);
	}

	struct gsx_template			tmpl = gsx_make_template(path, source);
	da_push(&vm->templates, &tmpl);

	vm->template_entry = &da_at(struct gsx_template, &vm->templates, 0);
	return (true);
}

struct gsx_box*				gsx_vm_interpret_ast(struct gsx_virtual_machine* vm, struct gsx_ast* ast) {
	assert(vm != NULL && "invalid interpreter at gsx_interpret_ast");

	if (ast == NULL) return (NULL);

	switch (ast->kind) {
		case GSX_AST_LITERAL: {
			PRINT_AST_INTERPRET_CALL("vm_interpret_literal", ast);
			struct gsx_ast_literal*	literal = ast->data.literal;
			struct gsx_token		token = literal->token;
			if (token.kind == GSX_TOKEN_CHAR)
				return (gsx_box_new_char(token.text.data[1]));
			else if (token.kind == GSX_TOKEN_STRING)
				return (gsx_box_new_view(view_make(token.text.data + 1, token.text.len - 2)));
			else if (token.kind == GSX_TOKEN_INTEGER)
				return (gsx_box_new_integer(view_conv_long(token.text)));
			else {
				print(ttyerr, "%view%:%ulong%:%ulong%: " TERMINAL_NOTICE_ERROR ":\tcannot interpret token %#view% as a literal\n", token.loc.path, token.loc.line, token.loc.column, token.text);
				return (NULL);
			}
		} break ;
		case GSX_AST_IDENT: {
			PRINT_AST_INTERPRET_CALL("vm_interpret_ident", ast);
			struct gsx_ast_ident*	ident = ast->data.ident;
			struct gsx_token		name = ident->name;
			struct gsx_definition*	def = gsx_vm_get_definition(vm, name.text);
			if (def == NULL) {
				print(ttyerr, "%view%:%ulong%:%ulong%: " TERMINAL_NOTICE_ERROR ":\tundefined identifier %#view%\n", name.loc.path, name.loc.line, name.loc.column, name.text);
				return (NULL);
			}
			return (gsx_vm_interpret_ast(vm, def->data.ast));
		} break ;
		case GSX_AST_LIST: {
			PRINT_AST_INTERPRET_CALL("vm_interpret_list", ast);
			struct gsx_ast_list*	list = ast->data.list;
			struct dynamic_array	elems = list->elems;

			struct dynamic_array	array = da_make(sizeof(struct gsx_box*));
			da_foreach_begin(elem, &elems, struct gsx_ast*);
				struct gsx_box*		elem_box = gsx_vm_interpret_ast(vm, elem);
				if (elem_box != NULL)
					da_push(&array, &elem_box);
			da_foreach_end();
			return (gsx_box_new_array(array));
		} break ;
		case GSX_AST_CALL: {
			/* TODO(xenobas): Implement type checking */
			PRINT_AST_INTERPRET_CALL("vm_interpret_call", ast);
			struct gsx_ast_call*					ast_call = ast->data.call;
			struct gsx_token						name = ast_call->name;
			struct gsx_definition*					def = gsx_vm_get_definition(vm, name.text);
			if (!gsx_definition_is_function(def)) {
				if (def == NULL)
					print(ttyerr, "%view%:%ulong%:%ulong%: " TERMINAL_NOTICE_ERROR ":\tcall to undefined function %#view%\n", name.loc.path, name.loc.line, name.loc.column, name.text);
				else
					print(ttyerr, "%view%:%ulong%:%ulong%: " TERMINAL_NOTICE_ERROR ":\tcannot call %#view% which is already defined as %gsx_definition_kind%\n", name.loc.path, name.loc.line, name.loc.column, def->kind, name.text);
				return (NULL);
			}

			struct gsx_box*							ret = NULL;
			if (def->kind == GSX_DEFINITION_INTERNAL_FUNCTION) {
				struct dynamic_array				boxes = da_make(sizeof(struct gsx_box*));
				bool								boxes_ok = true;
				da_foreach_begin(ast_arg, &ast_call->args, struct gsx_ast*);
					struct gsx_box*					box = gsx_vm_interpret_ast(vm, ast_arg);
					if (box == NULL) {
						print(ttyerr, "%view%:%ulong%:%ulong%: " TERMINAL_NOTICE_ERROR ":\tcannot pass void as the %nth% argument to %#view%\n", name.loc.path, name.loc.line, name.loc.column, da_index, name.text);
						boxes_ok = false;
						continue ;
					}
					da_push(&boxes, &box);
				da_foreach_end();

				if (boxes_ok)
					ret = def->data.internal(vm, &boxes);
				da_foreach_begin(box, &boxes, struct gsx_box*);
					gsx_box_free(box);
				da_foreach_end();
				da_free(&boxes);
			} else if (def->kind == GSX_DEFINITION_FUNCTION) {
				struct gsx_ast*						ast_func = def->data.ast;
				struct dynamic_array				scope = da_make(sizeof(struct gsx_definition));
				for (size_t i = 0ul; i < ast_call->args.len; ++i) {
					struct gsx_ast*			arg = da_at(struct gsx_ast*, &ast_call->args, i);
					struct gsx_ast_param*	param = da_at(struct gsx_ast_param*, &def->params, i);
					struct gsx_definition	def = gsx_definition_make_constant(param->name, arg);
					da_push(&scope, &def);
				}

				da_push(&vm->scopes, &scope);
					ret = gsx_vm_interpret_ast(vm, ast_func);
				da_pop(&vm->scopes);

				da_foreach_begin_ref(def, &scope, struct gsx_definition);
					gsx_definition_free(def);
				da_foreach_end();
				da_free(&scope);
			}
			return (ret);
		} break ;
		case GSX_AST_DEFINE: {
			/* TODO(xenobas): Implement type inference */
			PRINT_AST_INTERPRET_CALL("vm_interpret_define", ast);
			struct gsx_ast_define*			ast_def = ast->data.define;
			struct view						name = ast_def->name.text;
			if (ast_def->is_function) {
				struct gsx_ast*				ast_val = ast_def->value;
				struct dynamic_array		params = da_make(sizeof(struct gsx_ast_param*));
				da_foreach_begin(ref_param, &ast_def->params, struct gsx_ast_param*);
					struct gsx_ast_param*	param = gsx_new_ast_param_named(ref_param->name, ref_param->kind, ref_param->type, NULL);
					da_push(&params, &param);
				da_foreach_end();
				struct gsx_definition		def = gsx_definition_make_function(name, params, ast_val);
				da_push(&vm->definitions, &def);
			} else {
				struct gsx_ast*				ast_val = ast_def->value;
				struct gsx_definition*		ref_def = gsx_vm_resolve_ast(vm, ast_val);
				if (ref_def == NULL) {
					struct gsx_definition	def = gsx_definition_make_constant(name, ast_val);
					da_push(&vm->definitions, &def);
				} else {
					struct gsx_definition	def = {
						.kind = ref_def->kind,
						.name = ast_def->name.text,
						.params = da_make(sizeof(struct gsx_ast_param*)),
						.data = ref_def->data,
					};
					da_foreach_begin(ref_param, &ref_def->params, struct gsx_ast_param*);
						struct gsx_ast_param*	param = gsx_new_ast_param_named(ref_param->name, ref_param->kind, ref_param->type, NULL);
						da_push(&def.params, &param);
					da_foreach_end();
					da_push(&vm->definitions, &def);
				}
			}
			return (NULL);
		} break ;
		case GSX_AST_INVALID:
		default: {
			PRINT_AST_INTERPRET_CALL("vm_interpret_unreachable", ast);
			print(ttyerr, TERMINAL_NOTICE_ERROR ":\tcannot interpret invalid ast node %#gsx_ast%\n", ast);
			return (NULL);
		} break ;
	}
}

struct gsx_box*				gsx_vm_interpret_statement(struct gsx_virtual_machine* vm, struct gsx_statement* statement) {
	struct gsx_section		section = statement->section;
	if (!section.is_statement) return (NULL);

	if (!gsx_lexer_tokenize(section, &statement->tokens))
		return (vm->ok = false, NULL);
	statement->ast = gsx_parse_neo_ast_statement(vm, statement);
	/* print(ttyout, "%#gsx_ast%\n", statement->ast); */
	return (gsx_vm_interpret_ast(vm, statement->ast));
}

bool						gsx_vm_interpret_template(struct gsx_virtual_machine* vm, struct gsx_template* tmpl) {
	da_foreach_begin_ref(statement, &tmpl->statements, struct gsx_statement);
		struct gsx_box*		out = NULL;
		struct gsx_section	section = statement->section;
		struct location		location = section.loc;
		if (section.is_comment) {
			if (!section.is_terminated) {
				print(ttyerr, "%view%:%ulong%:%ulong%: " TERMINAL_NOTICE_ERROR ":\tunterminated html comment\n", location.path, location.line, location.column);
				vm->ok = false;
				continue ;
			} else if (section.is_statement) {
				out = gsx_vm_interpret_statement(vm, statement);
				if (gsx_type_of_box(out) == GSX_TYPE_INVALID) vm->ok = false;
			}
		} else out = gsx_box_new_view(section.text);
		gsx_write_template_output(tmpl, section, out);
		gsx_box_free(out);
	da_foreach_end();

	struct view	out_raw = string_builder_readonly_view(tmpl->output);
	struct view	out_trm = view_trim(out_raw, " \r\n\t");
	print(ttyout, "output -> %ulong% bytes\n", out_trm.len);
	print(ttyout, "%view%\n", out_trm);
	return (vm->ok);
}

bool						gsx_vm_run_template(struct gsx_virtual_machine* vm, struct gsx_template* tmpl) {
	gsx_split_template(tmpl);
	return (gsx_vm_interpret_template(vm, tmpl));
}

bool						gsx_vm_run(struct gsx_virtual_machine* vm) {
	assert(vm != NULL && "invalid vm passed to gsx_vm_run");

	if (vm->template_entry == NULL) return (true);
	return (gsx_vm_run_template(vm, vm->template_entry));
}

/*	GSX_Print_Format	*/

void	_gsx_print_writer_token_kind(int fd, enum gsx_token_kind kind) {
	switch (kind) {
		case GSX_TOKEN_INTEGER: _print_writer_cstr(fd, "integer"); break ;
		case GSX_TOKEN_STRING: _print_writer_cstr(fd, "string"); break ;
		case GSX_TOKEN_INVALID: _print_writer_cstr(fd, "invalid"); break ;
		case GSX_TOKEN_IDENT: _print_writer_cstr(fd, "identifier"); break ;
		case GSX_TOKEN_CALL: _print_writer_cstr(fd, "function call"); break ;
		case GSX_TOKEN_CHAR: _print_writer_cstr(fd, "character"); break ;
		case GSX_TOKEN_SQUARE_OPEN: _print_writer_cstr(fd, "array open"); break ;
		case GSX_TOKEN_SQUARE_CLOSE: _print_writer_cstr(fd, "array close"); break ;
		case GSX_TOKEN_PAREN_OPEN: _print_writer_cstr(fd, "parameters open"); break ;
		case GSX_TOKEN_PAREN_CLOSE: _print_writer_cstr(fd, "parameters close"); break ;
		case GSX_TOKEN_COMMA: _print_writer_cstr(fd, "comma"); break ;
		case GSX_TOKEN_COLON: _print_writer_cstr(fd, "colon"); break ;
		case GSX_TOKEN_HTML_OPEN: _print_writer_cstr(fd, "html open"); break ;
		case GSX_TOKEN_HTML_CLOSE: _print_writer_cstr(fd, "html close"); break ;
		case GSX_TOKEN_HTML_SIGNATURE: _print_writer_cstr(fd, "html gsx signature"); break ;
		default: {
			_print_writer_cstr(fd, "unreachable (");
			_print_writer_long(fd, (long)kind);
			_print_writer_char(fd, ')');
		} break ;
	}
}

void	_gsx_print_writer_token(int fd, struct gsx_token token) {
	_print_writer_cstr(fd, "{ token ");
	_print_writer_view_alt(fd, token.text);
	_print_writer_char(fd, ' ');
	_gsx_print_writer_token_kind(fd, token.kind);
	_print_writer_cstr(fd, " }");
}

void	_gsx_print_writer_ast(int fd, struct gsx_ast* node, unsigned indent) {
	if (node == NULL) {
		if (indent == 0u) _print_writer_cstr(fd, "(ast::null)");
		return ;
	}

	for (unsigned i = 0u; i < indent; ++i) print(fd, "\t");
	switch (node->kind) {
		case GSX_AST_LIST: {
			struct gsx_ast_list*	list = node->data.list;
			_print_writer_cstr(fd, "- list with ");
			_print_writer_ulong(fd, list->elems.len);
			_print_writer_cstr(fd, " elements");
		} break ;
		case GSX_AST_CALL: {
			struct gsx_ast_call*	call = node->data.call;
			_print_writer_cstr(fd, "- call ");
			_gsx_print_writer_token(fd, call->name);
			_print_writer_char(fd, '\n');
			da_foreach_begin(node, &call->args, struct gsx_ast*);
				_gsx_print_writer_ast(fd, node, indent + 1);
			da_foreach_end();
		} break ;
		case GSX_AST_DEFINE: {
			struct gsx_ast_define*	define = node->data.define;
			_print_writer_cstr(fd, "- define ");
			_gsx_print_writer_token(fd, define->name);
		} break ;
		case GSX_AST_LITERAL: {
			struct gsx_ast_literal*	literal = node->data.literal;
			_print_writer_cstr(fd, "- literal ");
			_gsx_print_writer_token(fd, literal->token);
		} break ;
		case GSX_AST_IDENT: {
			struct gsx_ast_ident*	ident = node->data.ident;
			_print_writer_cstr(fd, "- identifier ");
			_gsx_print_writer_token(fd, ident->name);
		} break ;
		case GSX_AST_INVALID: {
			_print_writer_cstr(fd, "- invalid");
		} break ;
		default: {
			_print_writer_cstr(fd, "- unreachable");
		} break ;
	}
}

void	_gsx_print_writer_ast_alt(int fd, struct gsx_ast* node) {
	if (node == NULL) {
		_print_writer_cstr(fd, "{ ast null }");
		return ;
	}

	switch (node->kind) {
		case GSX_AST_LIST: {
			struct gsx_ast_list*	list = node->data.list;

			_print_writer_cstr(fd, "{ ast ");
			_print_writer_ulong(fd, list->elems.len);
			_print_writer_cstr(fd, " elements list }");
		} break ;
		case GSX_AST_CALL: {
			struct gsx_ast_call*	call = node->data.call;

			_print_writer_cstr(fd, "{ call ");
			_print_writer_view_alt(fd, call->name.text);
			_print_writer_cstr(fd, " with ");
			_print_writer_ulong(fd, call->args.len);
			_print_writer_cstr(fd, " arguments }");
		} break ;
		case GSX_AST_DEFINE: {
			struct gsx_ast_define*	define = node->data.define;

			_print_writer_cstr(fd, "{ define ");
			_print_writer_view_alt(fd, define->name.text);
			_print_writer_cstr(fd, define->is_function ? " as a function that accepts " : " as a constant ");
			if (define->is_function) {
				_print_writer_ulong(fd, define->params.len);
				_print_writer_cstr(fd, " parameters via ");
			} else _print_writer_cstr(fd, "via ");
			_gsx_print_writer_ast_alt(fd, define->value);
			_print_writer_cstr(fd, " }");
		} break ;
		case GSX_AST_LITERAL: {
			struct gsx_ast_literal*	literal = node->data.literal;

			_print_writer_cstr(fd, "{ literal ");
			_print_writer_view_alt(fd, literal->token.text);
			_print_writer_cstr(fd, " }");
		} break ;
		case GSX_AST_IDENT: {
			struct gsx_ast_ident*	ident = node->data.ident;

			_print_writer_cstr(fd, "{ identifier ");
			_print_writer_view_alt(fd, ident->name.text);
			_print_writer_cstr(fd, " }");
		} break ;
		case GSX_AST_INVALID: {
			_print_writer_cstr(fd, "{ invalid ");
			_print_writer_addr(fd, (unsigned long)node);
			_print_writer_cstr(fd, " }");
		} break ;
		default: {
			_print_writer_cstr(fd, "{ unreachable }");
		} break ;
	}
}

void	_gsx_print_writer_type(int fd, enum gsx_type type) {
	switch (type) {
		case GSX_TYPE_CHAR: _print_writer_cstr(fd, "char"); break ;
		case GSX_TYPE_INTEGER: _print_writer_cstr(fd, "integer"); break ;
		case GSX_TYPE_STRING: _print_writer_cstr(fd, "string"); break ;
		case GSX_TYPE_ARRAY: _print_writer_cstr(fd, "array"); break ;
		case GSX_TYPE_VOID: _print_writer_cstr(fd, "void"); break ;
		case GSX_TYPE_INVALID: _print_writer_cstr(fd, "invalid"); break ;
		default: _print_writer_cstr(fd, "unreachable"); break ;
	}
}

void	_gsx_print_writer_definition_kind(int fd, enum gsx_definition_kind kind) {
	switch (kind) {
		case GSX_DEFINITION_CONSTANT: _print_writer_cstr(fd, "user constant"); break ;
		case GSX_DEFINITION_FUNCTION: _print_writer_cstr(fd, "user function"); break ;
		case GSX_DEFINITION_INTERNAL_CONSTANT: _print_writer_cstr(fd, "internal constant"); break ;
		case GSX_DEFINITION_INTERNAL_FUNCTION: _print_writer_cstr(fd, "internal function"); break ;
		case GSX_DEFINITION_INVALID: _print_writer_cstr(fd, "invalid"); break ;
		default: _print_writer_cstr(fd, "unreachable"); break ;
	}
}

void	_gsx_print_writer_ast_param_kind(int fd, enum gsx_ast_param_kind kind) {
	switch (kind) {
		case GSX_PARAM_DESTRUCTURE: _print_writer_cstr(fd, "destructure"); break ;
		case GSX_PARAM_IDENT: _print_writer_cstr(fd, "identifier"); break ;
		case GSX_PARAM_LITERAL: _print_writer_cstr(fd, "literal"); break ;
		case GSX_PARAM_INVALID: _print_writer_cstr(fd, "invalid"); break ;
		default: _print_writer_cstr(fd, "unreachable"); break ;
	}
}

void	gsx_print_writer_token_kind(int fd, va_list args) {
	enum gsx_token_kind	kind = va_arg(args, enum gsx_token_kind);
	_gsx_print_writer_token_kind(fd, kind);
}

void	gsx_print_writer_token(int fd, va_list args) {
	struct gsx_token	token = va_arg(args, struct gsx_token);
	_gsx_print_writer_token(fd, token);
}

void	gsx_print_writer_ast(int fd, va_list args) {
	struct gsx_ast*	node = va_arg(args, struct gsx_ast*);
	_gsx_print_writer_ast(fd, node, 0u);
}

void	gsx_print_writer_ast_alt(int fd, va_list args) {
	struct gsx_ast*	node = va_arg(args, struct gsx_ast*);
	_gsx_print_writer_ast_alt(fd, node);
}

void	gsx_print_writer_type(int fd, va_list args) {
	enum gsx_type	type = va_arg(args, enum gsx_type);
	_gsx_print_writer_type(fd, type);
}

void	gsx_print_writer_definition_kind(int fd, va_list args) {
	enum gsx_definition_kind	kind = va_arg(args, enum gsx_definition_kind);
	_gsx_print_writer_definition_kind(fd, kind);
}

void	gsx_print_writer_ast_param_kind(int fd, va_list args) {
	enum gsx_ast_param_kind	kind = va_arg(args, enum gsx_ast_param_kind);
	_gsx_print_writer_ast_param_kind(fd, kind);
}

void	print_registery_init_gsx(void) {
	static bool	gsx_print_init = false;
	if (gsx_print_init) return ;

	gsx_print_init = true;
	print_definition_add(view_make_cstr_const("gsx_ast"), gsx_print_writer_ast);
	print_definition_add(view_make_cstr_const("#gsx_ast"), gsx_print_writer_ast_alt);
	print_definition_add(view_make_cstr_const("gsx_ast_param_kind"), gsx_print_writer_ast_param_kind);
	print_definition_add(view_make_cstr_const("gsx_type"), gsx_print_writer_type);
	print_definition_add(view_make_cstr_const("gsx_token"), gsx_print_writer_token);
	print_definition_add(view_make_cstr_const("gsx_token_kind"), gsx_print_writer_token_kind);
	print_definition_add(view_make_cstr_const("gsx_definition_kind"), gsx_print_writer_definition_kind);
}

/*	 Entry_Point	*/

int		main(int argc, char **argv) {
	program_name = argv[0];

	if (argc != 3) {
		print(ttyerr, "%cstr%: " TERMINAL_NOTICE_ERROR ":\tincorrect number of arguments.\n", program_name);
		print(ttyerr, "\t"TERMINAL_NOTICE_USAGE": %cstr% <in.gsx> <out.html>\n", program_name);
		return (1);
	}
	const char*				file_in_path = argv[1];
	const char*				file_out_path = argv[2];

	if (!cstr_suffix(file_in_path, ".gsx")) {
		print(ttyerr, "%cstr%: " TERMINAL_NOTICE_ERROR ":\tinvalid input file extension %#cstr%.\n", program_name, file_in_path);
		print(ttyerr, "\t"TERMINAL_NOTICE_USAGE": %cstr% <in.gsx> <out.html>\n", program_name);
		return (2);
	}
	if (cstr_equals(file_in_path, file_out_path)) {
		print(ttyerr, "%cstr%: " TERMINAL_NOTICE_ERROR ":\treading and writing into the same file %#cstr% is not allowed.\n", program_name, file_out_path);
		print(ttyerr, "\t"TERMINAL_NOTICE_USAGE": %cstr% %#cstr% <out.html>\n", program_name, file_in_path);
		return (2);
	}
	if (!cstr_suffix(file_out_path, ".html")) {
		print(ttyerr, "%cstr%: " TERMINAL_NOTICE_ERROR ":\tinvalid output file extension %#cstr%.\n", program_name, file_out_path);
		print(ttyerr, "\t"TERMINAL_NOTICE_USAGE": %cstr% \"%cstr%\" <out.html>\n", program_name, file_in_path);
		return (2);
	}

	print_registery_init();
	print_registery_init_gsx();

	struct gsx_virtual_machine	vm = gsx_vm_make();
	if (!gsx_vm_load_entry(&vm, file_in_path)) program_return = 8;
	else if (!gsx_vm_run(&vm)) program_return = 16;

	gsx_vm_destroy(&vm);
	return (program_return);
// 	struct view			file_out_view = string_builder_as_view(sb);
// 	if (file_out_view.data == NULL) {
// 		print(ttyerr, "%cstr%: "TERMINAL_NOTICE_ERROR":\tan error happened while generating output file content %#cstr%.\n", program_name, file_out_path);
// 		program_return = 32;
// 		goto builder_free;
// 	}
// 	if (!os_file_dump(file_out_path, file_out_view)) {
// 		print(ttyerr, "%cstr%: "TERMINAL_NOTICE_ERROR":\tan error happened while generating output file content %#cstr%.\n", program_name, file_out_path);
// 		print(ttyerr, "\tErrno: %errno%\n");
// 		program_return = 32;
// 		goto file_out_free;
// 	}
// 	print(ttyout, "%cstr%: "TERMINAL_NOTICE_SUCCESS":\tevaluated gsx template %#cstr% into %ulong% bytes written in %#cstr%.\n", program_name, file_in_path, file_out_view.len, file_out_path);
// 
// file_out_free:
// 	free(file_out_view.data);
// builder_free:
// 	string_builder_destroy(&sb);
// boxes_free:
// 	for (size_t i = 0; i < boxes.len; ++i) {
// 		struct gsx_box*	box = da_at(struct gsx_box*, &boxes, i);
// 		gsx_box_free(box);
// 	}
// 	da_free(&boxes);
// sections_free:
// 	da_free(&sections);
// 
// 	gsx_vm_free(&vm);
// 	return (program_return);
}
