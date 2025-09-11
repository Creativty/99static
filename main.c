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

#define TERMINAL_NOTICE_SUCCESS TERMINAL_COLOR_GREEN "Success" TERMINAL_STYLE_RESET
#define TERMINAL_NOTICE_ERROR TERMINAL_COLOR_RED "Error" TERMINAL_STYLE_RESET
#define TERMINAL_NOTICE_TODO TERMINAL_COLOR_YELLOW "TODO" TERMINAL_STYLE_RESET
#define TERMINAL_NOTICE_HINT TERMINAL_COLOR_WHITE "Hint" TERMINAL_STYLE_RESET
#define TERMINAL_NOTICE_USAGE TERMINAL_COLOR_WHITE "Usage" TERMINAL_STYLE_RESET

#define _STRINGIFY(X) #X
#define STRINGIFY(X) _STRINGIFY(X)

#define ttyout ((int)1)
#define ttyerr ((int)2)

#ifdef DEBUG_AST_CALLS
#define print_ast_call(NAME, TOKEN) print(ttyout, NAME " :: %gsx_token%\n", (TOKEN));
#else
#define print_ast_call(NAME, TOKEN) ((void)(NAME), (void)(TOKEN))
#endif

/*	 Program	*/

const char*	program_name = NULL;
int			program_return = 0;

/*	 Forward_Declarations	*/
struct				dynamic_array;
struct				string_builder;
struct				gsx_ast;
struct				gsx_box;
struct				gsx_definition;
int					cstr_length(const char*);
struct gsx_box*		gsx_process_ast(const struct dynamic_array*, struct gsx_ast*);

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

/*	 String_View	*/

struct			view {
	char	*data;
	size_t	len;
};

int				view_index(struct view string, const char* to_find) {
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

int				view_index_last(struct view string, const char* to_find) {
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

int				view_index_char(struct view string, const char to_find) {
	if (to_find == '\0') return (-1);
	for (int i = 0; i < (int)string.len; ++i) {
		if (string.data[i] == to_find) return (i);
	}
	return (-1);
}

int				view_index_view(struct view string, struct view to_find) {
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

struct view		view_slice(struct view string, size_t begin, size_t end) {
	if (begin > string.len) return (view_make(NULL, 0ul));
	if (end > string.len) end = string.len;
	size_t	len = end - begin;
	return (view_make(&string.data[begin], len));
}

struct view		view_drop(struct view string, int count) {
	if (count < 0) count = 0;
	if (count >= (int)string.len) return ((struct view){ NULL, 0ul });
	else return ((struct view){ &string.data[count], (int)string.len - count });
}

struct view		view_take(struct view string, int count) {
	if (count >= (int)string.len) count = (int)string.len;
	if (count <= 0) return ((struct view){ NULL, 0ul });
	else return ((struct view){ string.data, count });
}

struct view		view_drop_take(struct view string, int drop, int take) {
	return (view_take(view_drop(string, drop), take));
}

bool			view_equals_cstr(struct view lhs, const char* rhs) {
	return (lhs.len == (size_t)cstr_length(rhs) && view_index(lhs, rhs) == 0);
}

bool			view_equals(struct view lhs, struct view rhs) {
	return (lhs.len == rhs.len && view_index_view(lhs, rhs) == 0);
}

struct view		view_trim(struct view string, const char *charset) {
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

bool			view_prefix(struct view string, const char* prefix) {
	return (view_index(string, prefix) == 0);
}

bool			view_suffix(struct view string, const char* suffix) {
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

struct					dynamic_array {
	void	*items;
	size_t	len;
	size_t	cap;
	size_t	unit;
};

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

/*	 String_Builder	*/

struct					string_builder {
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

char*					string_builder_as_cstr(struct string_builder sb) {
	return (cstr_clone_len(sb.bytes.items, sb.bytes.len));
}

struct view				string_builder_as_view(struct string_builder sb) {
	char*	data = string_builder_as_cstr(sb);
	return ((struct view){ .data = data, .len = cstr_length(data) });
}

void					string_builder_write_cstr(struct string_builder* sb, const char* str) {
	if (sb == NULL || str == NULL) return ;
	for (size_t i = 0; str[i]; ++i)
		da_append(&sb->bytes, (void *)&str[i]);
}

void					string_builder_write_view(struct string_builder* sb, struct view string) {
	if (sb == NULL || string.data == NULL) return ;
	for (size_t i = 0; i < string.len; ++i)
		da_append(&sb->bytes, (void *)&string.data[i]);
}

void					string_builder_write_char(struct string_builder* sb, char c) {
	if (sb == NULL || c == '\0') return ;
	da_append(&sb->bytes, (void *)&c);
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
	assert(print_registery_is_init && "Cannot add print definition before default definitions");
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

void						_print_writer_view_alt(int fd, struct view v, char c) {
	write(fd, &c, 1ul);
	write(fd, v.data, v.len);
	write(fd, &c, 1ul);
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
	/* NOTE(xenobas): char is promoted to int when passed through ... */
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
	char		w = '"';
	if (v.len >= 2 && view_prefix(v, "\"") && view_suffix(v, "\"")) w = '`';

	_print_writer_view_alt(fd, v, w);
}

void						print_writer_view_alt(int fd, va_list args) {
	struct view	v = va_arg(args, struct view);
	char		w = '"';
	if (v.len >= 2 && view_prefix(v, "\"") && view_suffix(v, "\""))
		w = '`';

	_print_writer_view_alt(fd, v, w);
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
	if (data == NULL) return ((struct view){ NULL, 0ul });
	return ((struct view){ data, size });
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
	close(fd);
	return (ok);
}

/*	 GSX_Section	*/

struct	gsx_section {
	struct view	text;
	bool		is_comment;
	bool		is_expression;
	bool		is_terminated;
};

bool	gsx_load_sections(struct view string, struct dynamic_array* sections) {
	if (sections == NULL) return (false);

	int		i = 0;
	bool	ok = true;
	while (i < (int)string.len) {
		bool				is_expression = false;
		bool				is_terminated = false;

		int					len = 0;
		struct view			prefix = view_drop(string, i);
		bool				is_comment = view_prefix(prefix, "<!--");
		while (len < (int)prefix.len) {
			struct view		suffix = view_drop(prefix, len);
			is_terminated = view_prefix(suffix, is_comment ? "-->" : "<!--");
			if (is_terminated) {
				if (is_comment) len += cstr_length("-->");
				break ;
			}
			len++;
		}
		struct view		text = view_drop_take(string, i, len);
		if (is_comment) {
			if (!is_terminated) {
				/* TODO(xenobas): Show location of unterminated comment. */
				ok = false;
			}
			struct view	comment = view_trim(view_drop(text, cstr_length("<!--")), " \t");
			is_expression = (is_terminated && view_prefix(comment, "gsx:"));
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

/*	 GSX_Definition	*/

typedef struct gsx_box*	(*gsx_definition_internal)(struct dynamic_array*);

enum					gsx_definition_kind {
	GSX_DEFINITION_INVALID,

	GSX_DEFINITION_VARIABLE,
	GSX_DEFINITION_FUNCTION,
	GSX_DEFINITION_INTERNAL,
};

struct					gsx_definition {
	struct view				name;
	struct dynamic_array		params;
	enum gsx_definition_kind	kind;
	union {
		struct gsx_ast*			ast;
		gsx_definition_internal	internal;
	}							data;
};

struct gsx_definition*	gsx_definition_get(const struct dynamic_array* definitions, struct view name) {
	if (definitions == NULL) return (NULL);
	for (size_t i = 0; i < definitions->len; ++i) {
		struct gsx_definition	*definition = &da_at(struct gsx_definition, definitions, i);
		if (view_equals(definition->name, name))
			return (definition);
	}
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
	GSX_TOKEN_ARRAY_OPEN,
	GSX_TOKEN_ARRAY_CLOSE,
	GSX_TOKEN_ARRAY_SEPARATOR,
	GSX_TOKEN_PARAMS_DELIMITER,
	GSX_TOKEN_DESTRUCTURE_SEPARATOR,
};

struct				gsx_token {
	struct view		text;
	enum gsx_token_kind	kind;
};

bool				gsx_token_is_expression(enum gsx_token_kind kind) {
	if (kind == GSX_TOKEN_CALL) return (true);
	if (kind == GSX_TOKEN_STRING) return (true);
	if (kind == GSX_TOKEN_INTEGER) return (true);
	if (kind == GSX_TOKEN_IDENT) return (true);
	return (false);
}

/*	 GSX_Lexer	*/

struct				gsx_lexer {
	struct view	text;
	size_t		index;
	size_t		offset;
	bool		ok;
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
	struct view		text = view_drop_take(lexer->text, drop, take);
	struct gsx_token	token = { .text = text, .kind = kind };
	lexer->offset = lexer->index;
	return (token);
}

bool				gsx_lexer_tokenize(struct view text, struct dynamic_array* tokens) {
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

/*	 GSX_Parser	*/

struct				gsx_parser {
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
		print(ttyerr, "%cstr%:\t"TERMINAL_NOTICE_ERROR": Expected %gsx_token_kind%, got %gsx_token% instead\n", program_name, kind, gsx_parser_peek(parser));
		return (parser->ok = false);
	}
	return (true);
}

bool				gsx_parser_expect_any(struct gsx_parser* parser, enum gsx_token_kind* kinds, size_t kinds_count) {
	if (!gsx_parser_accept_any(parser, kinds, kinds_count)) {
		print(ttyerr, "%cstr%:\t"TERMINAL_NOTICE_ERROR": Expected ", program_name);
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

/*	 GSX_Syntax_Tree	*/

enum							gsx_ast_kind {
	GSX_AST_INVALID,
	GSX_AST_CALL,
	GSX_AST_IDENT,
	GSX_AST_LITERAL,
	GSX_AST_DEFINE,
};

struct							gsx_ast_call {
	struct gsx_token		name;
	struct dynamic_array	args;
};

struct							gsx_ast_ident {
	struct gsx_token	name;
};

struct							gsx_ast_literal {
	struct gsx_token	token;
};

enum							gsx_ast_define_param_kind {
	GSX_DEFINE_PARAM_INVALID,

	GSX_DEFINE_PARAM_IDENT,
	GSX_DEFINE_PARAM_PATTERN,
	GSX_DEFINE_PARAM_DESTRUCTURE,
};

struct							gsx_ast_destructure {
	struct dynamic_array	parts;
};

struct							gsx_ast_define_param {
	enum gsx_ast_define_param_kind	kind;
	struct gsx_ast_destructure*		destructure;
};

struct							gsx_ast_define {
	struct gsx_token		token;
	struct gsx_token		name;
	struct gsx_token		delimiter;
	struct dynamic_array	params;
	struct gsx_ast*			value;
};

struct							gsx_ast {
	enum gsx_ast_kind		kind;
	union {
		struct gsx_ast_call*		call;
		struct gsx_ast_ident*		ident;
		struct gsx_ast_literal*		literal;
		struct gsx_ast_define*		define;
	}					data;
};

void							gsx_ast_free_define_param(struct gsx_ast_define_param* param) {
	(void)param;
}

void							gsx_ast_free(struct gsx_ast* node) {
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
			print(ttyerr, "%cstr%:\t" TERMINAL_NOTICE_TODO ": gsx_ast_free(kind = GSX_AST_DEFINE)\n", program_name);
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

bool							gsx_ast_is_value_void(struct gsx_ast* ast) {
	if (ast != NULL && ast->kind == GSX_AST_CALL) return (false);
	if (ast != NULL && ast->kind == GSX_AST_IDENT) return (false);
	if (ast != NULL && ast->kind == GSX_AST_LITERAL) return (false);
	return (true);
}

struct gsx_ast_define_param*	gsx_ast_new_define_param(enum gsx_ast_define_param_kind kind, struct gsx_ast_destructure* destructure) {
	struct gsx_ast_define_param*	param = malloc(sizeof(struct gsx_ast_define_param));
	if (param != NULL) {
		param->kind = kind;
		param->destructure = destructure;
	}
	return (param);
}

struct gsx_ast*					gsx_ast_parse_ident(struct gsx_parser* parser) {
	if (!gsx_parser_expect(parser, GSX_TOKEN_IDENT)) return (NULL);
	struct gsx_token		name = parser->current;
	print_ast_call("gsx_ast_parse_ident", name);
	struct gsx_ast*			node = malloc(sizeof(struct gsx_ast) + sizeof(struct gsx_ast_ident));
	struct gsx_ast_ident*	ident = (struct gsx_ast_ident*)&((char *)node)[sizeof(struct gsx_ast)];
	if (node != NULL) {
		ident->name = name;
		node->kind = GSX_AST_IDENT;
		node->data.ident = ident;
	}
	return (node);
}

struct gsx_ast*					gsx_ast_parse_literal(struct gsx_parser* parser) {
	static enum gsx_token_kind	lit_kinds[] = { GSX_TOKEN_INTEGER, GSX_TOKEN_STRING, GSX_TOKEN_CHAR };
	const size_t				lit_kinds_count = sizeof(lit_kinds) / sizeof(lit_kinds[0]);
	if (!gsx_parser_expect_any(parser, lit_kinds, lit_kinds_count)) return (NULL);
	struct gsx_token		token = parser->current;
	print_ast_call("gsx_ast_parse_literal", token);
	struct gsx_ast*			node = malloc(sizeof(struct gsx_ast) + sizeof(struct gsx_ast_literal));
	struct gsx_ast_literal*	literal = (struct gsx_ast_literal*)&((char *)node)[sizeof(struct gsx_ast)];
	if (node != NULL) {
		literal->token = token;
		node->kind = GSX_AST_LITERAL;
		node->data.literal = literal;
	}
	return (node);
}

struct gsx_ast*					gsx_ast_parse_define_param(struct gsx_parser* parser) {
	static enum gsx_token_kind	define_param_kinds[] = {
		GSX_TOKEN_CALL, /* NOTE(xenobas): Value */
		GSX_TOKEN_IDENT, /* NOTE(xenobas): Named parameter or Value */
		GSX_TOKEN_ARRAY_OPEN, /* NOTE(xenobas): Destructure pattern matching */
		GSX_TOKEN_INTEGER, GSX_TOKEN_STRING, GSX_TOKEN_CHAR, /* NOTE(xenobas): Literal pattern matching */
	};
	const size_t				define_param_kinds_count = sizeof(define_param_kinds) / sizeof(define_param_kinds[0]);
	if (!gsx_parser_expect_any(parser, define_param_kinds, define_param_kinds_count)) return (NULL);
	struct gsx_token		token = parser->current;
	print_ast_call("gsx_ast_parse_define_param", token);
	if (token.kind == GSX_TOKEN_CALL) { /* NOTE(xenobas): Value */
	}
	return (NULL);
}

struct gsx_ast*					gsx_ast_parse_define(struct gsx_parser* parser, struct gsx_definition* definition) {
	if (parser == NULL || definition == NULL) return (NULL);

	struct gsx_token					token = parser->current;
	print_ast_call("gsx_ast_parse_define", token);
	if (token.kind != GSX_TOKEN_CALL) return (NULL);

	struct view						text = view_take(token.text, token.text.len - 1ul); /* EXAMPLE(xenobas): core::define */
	if (!view_equals_cstr(text, "core::define")) return (NULL);
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
	return (print(ttyerr, "%cstr%:\t" TERMINAL_NOTICE_TODO ": gsx_ast_parse_define\n", program_name), NULL);
}

struct gsx_ast*					gsx_ast_parse_call(struct gsx_parser* parser) {
	static enum gsx_token_kind	arg_kinds[] = { GSX_TOKEN_CALL, GSX_TOKEN_IDENT, GSX_TOKEN_INTEGER, GSX_TOKEN_STRING };
	const size_t				arg_kinds_count = sizeof(arg_kinds) / sizeof(arg_kinds[0]);
	if (!gsx_parser_expect(parser, GSX_TOKEN_CALL)) return (NULL);

	struct gsx_token		name = parser->current;
	print_ast_call("gsx_ast_parse_call", name);
	struct view			text = view_take(name.text, name.text.len - 1ul);
	struct gsx_definition*	definition = gsx_definition_get(parser->definitions, text);
	if (definition == NULL) {
		print(ttyerr, "%cstr%:\t" TERMINAL_NOTICE_ERROR ": Undeclared function %#view% was called!\n", program_name, text);
		return (NULL);
	}
	if (view_equals_cstr(text, "core::define")) {
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
		} else if (token.kind == GSX_TOKEN_INTEGER || token.kind == GSX_TOKEN_STRING) {
			node = gsx_ast_parse_literal(parser);
		} else {
			if (!gsx_parser_expect_any(parser, arg_kinds, arg_kinds_count)) break ;
		}
		if (node == NULL) break ;
		da_append(&args, &node);
	}
	if (args.len < args_count) {
		print(ttyerr, "%cstr%:\t"TERMINAL_NOTICE_ERROR": Insufficient arguments passed to %#view%!\n", program_name, text);
		print(ttyerr, "\tExpected %ulong% parameters, got %ulong% arguments instead.\n", args_count, args.len);

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

struct gsx_ast*					gsx_ast_parse(const struct dynamic_array* definitions, const struct dynamic_array* tokens) {
	struct gsx_parser	parser = { 0 };
	parser.ok = true;
	parser.tokens = tokens;
	parser.definitions = definitions;

	struct gsx_ast*		ast = NULL;
	struct gsx_token	token = gsx_parser_peek(&parser);
	print_ast_call("gsx_ast_parse", token);
	if (token.kind == GSX_TOKEN_STRING || token.kind == GSX_TOKEN_CHAR || token.kind == GSX_TOKEN_INTEGER)
		ast = gsx_ast_parse_literal(&parser);
	else if (token.kind == GSX_TOKEN_IDENT)
		ast = gsx_ast_parse_ident(&parser);
	else if (token.kind == GSX_TOKEN_CALL) {
		ast = gsx_ast_parse_call(&parser);
		if (ast != NULL && !gsx_parser_eof(&parser)) {
			struct view	name = ast->data.call->name.text;
			size_t			extra_count = 0u;
			while (!gsx_parser_eof(&parser)) {
				gsx_parser_next(&parser);
				extra_count++;
			}

			print(ttyerr, "%cstr%:\t"TERMINAL_NOTICE_ERROR": Extra arguments passed to %#view%!\n", program_name, name);
			print(ttyerr, "\tExpected %ulong% parameters, got %ulong% arguments instead.\n", ast->data.call->args.len, ast->data.call->args.len + extra_count);
			return (gsx_ast_free(ast), parser.ok = false, NULL);
		}
	} else {
		struct view	name = token.text;
		print(ttyerr, "%cstr%:\t"TERMINAL_NOTICE_ERROR": Expected \"expression\", got %#view% instead.\n", program_name, name);
		return (parser.ok = false, NULL);
	}

	if (!gsx_parser_eof(&parser)) {
		struct view	name = token.text;
		print(ttyerr, "%cstr%:\t"TERMINAL_NOTICE_ERROR": Multi value expression %#view% is not allowed!\n", program_name, name);
		print(ttyerr, "\t| remaining tokens = [ ");
		while (!gsx_parser_eof(&parser)) {
			struct gsx_token	token = gsx_parser_next(&parser);
			print(ttyerr, "%gsx_token%", token);
			if (!gsx_parser_eof(&parser))
				print(ttyerr, ", ");
		}
		print(ttyerr, " ]\n");
		return (gsx_ast_free(ast), parser.ok = false, NULL);
	} else return (ast);
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
	enum gsx_box_kind	kind;
	union {
		char					character;
		long					integer;
		char*					str;
		struct dynamic_array	array;
	}					data;
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
			struct dynamic_array	array = da_make(sizeof(struct gsx_box*));
			for (size_t i = 0ul; i < array.len; ++i) {
				struct gsx_box*	box = da_at(struct gsx_box*, &ref->data.array, i);
				da_append(&array, gsx_box_clone(box));
			}
			return (gsx_box_new_array(array));
		} break ;
		default: return (NULL);
	}
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

/*	 GSX_Interpreter	*/

struct gsx_box*		gsx_process_ast_literal(const struct dynamic_array* definitions, struct gsx_ast_literal* ast) {
	/* NOTE(xenobas): do I remove the argument definitions? */
	struct gsx_token	token = ast->token;

	(void)definitions;
	switch (token.kind) {
		case GSX_TOKEN_STRING: {
			return (gsx_box_new_view(view_make(token.text.data + 1, token.text.len - 2)));
		} break ;
		case GSX_TOKEN_INTEGER: {
			long	integer = view_conv_long(token.text);
			return (gsx_box_new_integer(integer));
		} break ;
		default: return (NULL);
	}
}

struct gsx_box*		gsx_process_ast_ident(const struct dynamic_array* definitions, struct gsx_ast_ident* ident) {
	struct view			text = ident->name.text;
	struct gsx_definition*	definition = gsx_definition_get(definitions, text);
	struct gsx_box*			retval = gsx_process_ast(definitions, definition->data.ast);
	if (retval == NULL)
		print(ttyerr, "%cstr%:\t" TERMINAL_NOTICE_ERROR ": Referenced undeclared identifier %#view%!\n", program_name, text);
	return (retval);
}

struct gsx_box*		gsx_process_ast_call(const struct dynamic_array* definitions, struct gsx_ast_call* call) {
	struct dynamic_array			args = call->args;
	struct view						text = { .data = call->name.text.data, .len = call->name.text.len - 1ul };
	struct gsx_box*					retval = NULL;
	struct gsx_definition*			definition = gsx_definition_get(definitions, text);
	if (definition == NULL) {
		print(ttyerr, "%cstr%:\t" TERMINAL_NOTICE_ERROR ": Called undeclared function %#view%!\n", program_name, text);
		goto gsx_process_ast_call_return;
	}

	enum gsx_definition_kind		kind = definition->kind;
	struct dynamic_array			params = definition->params;
	if (params.len != args.len) {
		print(ttyerr, "%cstr%:\t"TERMINAL_NOTICE_ERROR": Called function %#view% with incorrect number of arguments\n", program_name, text);
		print(ttyerr, "\tExpected %ulong% parameters, got %ulong% arguments instead.\n", params.len, args.len);
		goto gsx_process_ast_call_return;
	}
	switch (kind) {
		case GSX_DEFINITION_FUNCTION: {
			retval = gsx_box_new_view(text);
		} break ;
		case GSX_DEFINITION_INTERNAL: {
			gsx_definition_internal		function = definition->data.internal;
			if (function == NULL) {
				print(ttyerr, "%cstr%:\t" TERMINAL_NOTICE_ERROR ": Unimplemented internal function %#view%.\n", program_name, text);
				print(ttyerr, "\tQuestion: How did you even get here?\n");
				goto gsx_process_ast_call_return;
			}

			bool					args_ok = true;
			struct dynamic_array	args_boxed = da_make(sizeof(struct gsx_box*));
			for (size_t i = 0ul; i < args.len; ++i) {
				struct gsx_ast*	ast = da_at(struct gsx_ast*, &args, i);
				struct gsx_box*	box = gsx_process_ast(definitions, ast);
				if (box == NULL) {
					print(ttyerr, "%cstr%:\t" TERMINAL_NOTICE_ERROR ": Cannot pass void value to %#view% in argument hole %ulong%.\n", program_name, text, i);
					args_ok = false;
					continue ;
				}
				da_append(&args_boxed, &box);
			}
			if (!args_ok) {
				for (size_t i = 0ul; i < args_boxed.len; ++i) {
					struct gsx_box*	box = da_at(struct gsx_box*, &args_boxed, i);
					gsx_box_free(box);
				}
				da_free(&args_boxed);
				goto gsx_process_ast_call_return;
			}
			retval = function(&args_boxed);
			if (retval == NULL) {
				print(ttyerr, "%s:\t" TERMINAL_NOTICE_ERROR ": Disallowed function %#view% returns void expression.\n", program_name, text);
				print(ttyerr, "\tHint: Might be supported in the future.\n");
				goto gsx_process_ast_call_return;
			}
		} break ;
		case GSX_DEFINITION_VARIABLE: {
			print(ttyerr, "%cstr%:\t" TERMINAL_NOTICE_ERROR ": Cannot call variable %#view% as if it was a function\n", program_name, text);
			print(ttyerr, "\tQuestion: How did you even get here?\n");
			goto gsx_process_ast_call_return;
		} break ;
		case GSX_DEFINITION_INVALID: {
			print(ttyerr, "%cstr%:\t" TERMINAL_NOTICE_ERROR ": Called function %#view% with invalid registered definition.\n", program_name, text);
			print(ttyerr, "\tQuestion: How did you even get here?\n");
			goto gsx_process_ast_call_return;
		} break ;
		default: {
			print(ttyerr, "%cstr%:\t" TERMINAL_NOTICE_ERROR ": Called function %#view% with unreachable registered definition.\n", program_name, text);
			print(ttyerr, "\tQuestion: How did you even get here?\n");
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

struct gsx_box*		gsx_process_expression(struct dynamic_array* definitions, struct view comment) {
	assert(comment.data != NULL && comment.len > 0ul && "Invalid argument passed to gsx_process_expression");
	struct view	expression = comment;
	expression = view_drop(expression, cstr_length("<!--"));
	expression = view_take(expression, expression.len - cstr_length("-->"));
	/* TODO(xenobas): Handle this mess post tokenization not during processing of raw strings. */
	expression = view_trim(expression, " \t");
	for (size_t i = 0; i < expression.len; ++i) { /* NOTE(xenobas): Do not allow for multiline expressions... for now that is. */
		if (expression.data[i] == '\n') {
			struct view	text = view_trim(expression, " \r\n\t");
			print(ttyerr, "%cstr%:\t"TERMINAL_NOTICE_ERROR": Unsupported multiline gsx comment\n\t|\t", program_name);
			if (text.len > 0) print(ttyerr, "%view%\n", text);
			else print(ttyerr, "<empty comment>\n");
			print(ttyerr, "\t|\n");
			return (NULL);
		}
	}
	expression = view_drop(expression, cstr_length("gsx:"));
	expression = view_trim(expression, " \t");

	struct gsx_box*			production = NULL;
	struct dynamic_array	tokens = da_make(sizeof(struct gsx_token));
	if (!gsx_lexer_tokenize(expression, &tokens)) {
		for (size_t i = 0; i < tokens.len; ++i) {
			struct gsx_token	token = da_at(struct gsx_token, &tokens, i);

			token.text = view_trim(token.text, " \n\r\t");
			print(ttyerr, "%gsx_token%\n", token);
		}
		goto gsx_process_expression_return;
	}
	if (false) {
		print(ttyout, "tokens = { ");
		for (size_t i = 0; i < tokens.len; ++i) {
			struct gsx_token	token = da_at(struct gsx_token, &tokens, i);
			print(ttyout, "%gsx_token%", token);
			if (i + 1 < tokens.len)
				print(ttyout, ", ");
		}
		print(ttyout, "}\n");
	}

	struct gsx_ast*			syntax_tree = gsx_ast_parse(definitions, &tokens);
	if (syntax_tree == NULL)
		goto gsx_process_expression_return;
	/* print(ttyout, "%gsx_ast%\n", syntax_tree); */
	production = gsx_process_ast(definitions, syntax_tree);
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

			struct view	text = view_trim(section.text, " \r\n\t");
			print(ttyerr, "%cstr%:\t"TERMINAL_NOTICE_ERROR": Unterminated comment\n\t|\t", program_name);
			if (text.len > 0) print(ttyerr, "%view%\n", text);
			else print(ttyerr, "<empty text>\n");
			print(ttyerr, "\t|\n");
			continue ;
		}
		struct gsx_box*	box = NULL;
		if (section.is_expression) {
			box = gsx_process_expression(definitions, section.text);
			ok = (ok && (box != NULL));
		} else if (!section.is_comment) {
			box = gsx_box_new_view(section.text);
			ok = (ok && (box != NULL));
		}
		if (box != NULL) da_append(boxes, &box);
	}
	return (ok);
}

/*	 GSX_Internal	*/

struct gsx_box*		gsx_internal_stub(struct dynamic_array* args) {
	(void)args;
	return (NULL);
}

struct gsx_box*		gsx_internal_include(struct dynamic_array* args) {
	(void)args;
	return (gsx_box_new_string("core::include"));
}

struct gsx_box*		gsx_internal_replace(struct dynamic_array* args) {
	struct gsx_box*	text = da_at(struct gsx_box*, args, 0);
	struct gsx_box*	pattern = da_at(struct gsx_box*, args, 1);
	struct gsx_box*	replace = da_at(struct gsx_box*, args, 2);
	(void)pattern;
	(void)text;
	return (gsx_box_clone(replace));
}

void				gsx_init_definitions(struct dynamic_array* definitions) {
	bool					arg_placeholder = true;

	char*					define_title_name = cstr_clone("TITLE");
	char*					define_title_val = cstr_clone("\"index.html\"");
	struct gsx_ast*			define_title_ast = NULL;
	{
		struct gsx_ast*			node = malloc(sizeof(struct gsx_ast) + sizeof(struct gsx_ast_literal));
		struct gsx_ast_literal*	literal = (struct gsx_ast_literal*)&((char *)node)[sizeof(struct gsx_ast)];
		if (node != NULL) {
			literal->token.text = view_make_cstr(define_title_val);
			literal->token.kind = GSX_TOKEN_STRING;

			node->kind = GSX_AST_LITERAL;
			node->data.literal = literal;
		}
		define_title_ast = node;
	}
	struct gsx_definition	define_title = {
		.name = view_make_cstr(define_title_name),
		.params = { 0 },
		.kind = GSX_DEFINITION_VARIABLE,
		.data = { .ast = define_title_ast },
	};
	da_append(definitions, &define_title);

	char*					define_define_name = cstr_clone("core::define");
	struct dynamic_array	define_define_params = da_make(sizeof(bool));
	da_append(&define_define_params, &arg_placeholder);
	struct gsx_definition	define_define = {
		.name = view_make_cstr(define_define_name),
		.params = define_define_params,
		.kind = GSX_DEFINITION_INTERNAL,
		.data = { .internal = gsx_internal_stub },
	};
	da_append(definitions, &define_define);

	char*					define_env_name = cstr_clone("core::env");
	struct dynamic_array	define_env_params = da_make(sizeof(bool));
	da_append(&define_env_params, &arg_placeholder);
	struct gsx_definition	define_env = {
		.name = view_make_cstr(define_env_name),
		.params = define_env_params,
		.kind = GSX_DEFINITION_INTERNAL,
		.data = { .internal = gsx_internal_stub },
	};
	da_append(definitions, &define_env);

	char*					define_include_name = cstr_clone("core::include");
	struct dynamic_array	define_include_params = da_make(sizeof(bool));
	da_append(&define_include_params, &arg_placeholder);
	struct gsx_definition	define_include = {
		.name = view_make_cstr(define_include_name),
		.params = define_include_params,
		.kind = GSX_DEFINITION_INTERNAL,
		.data = { .internal = gsx_internal_include },
	};
	da_append(definitions, &define_include);

	char*					define_factorial_name = cstr_clone("core::factorial");
	struct dynamic_array	define_factorial_params = da_make(sizeof(bool));
	da_append(&define_factorial_params, &arg_placeholder);
	struct gsx_definition	define_factorial = {
		.name = view_make_cstr(define_factorial_name),
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
		.name = view_make_cstr(define_mul_name),
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
		.name = view_make_cstr(define_add_name),
		.params = define_add_params,
		.kind = GSX_DEFINITION_INTERNAL,
		.data = { .internal = gsx_internal_stub },
	};
	da_append(definitions, &define_add);

	char*					define_replace_name = cstr_clone("core::replace");
	struct dynamic_array	define_replace_params = da_make(sizeof(struct gsx_ast_define_param*));
	for (size_t i = 0ul; i < 3ul; ++i) {
		struct gsx_ast_define_param*	define_replace_param = gsx_ast_new_define_param(GSX_DEFINE_PARAM_IDENT, NULL);
		da_append(&define_replace_params, &define_replace_param);
	}
	struct gsx_definition	define_replace = {
		.name = view_make_cstr(define_replace_name),
		.params = define_replace_params,
		.kind = GSX_DEFINITION_INTERNAL,
		.data = { .internal = gsx_internal_replace },
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

/*	GSX_Print_Format	*/

void	_gsx_print_writer_token_kind(int fd, enum gsx_token_kind kind) {
	switch (kind) {
		case GSX_TOKEN_INTEGER: _print_writer_cstr(fd, "integer"); break ;
		case GSX_TOKEN_STRING: _print_writer_cstr(fd, "string"); break ;
		case GSX_TOKEN_INVALID: _print_writer_cstr(fd, "invalid"); break ;
		case GSX_TOKEN_IDENT: _print_writer_cstr(fd, "identifier"); break ;
		case GSX_TOKEN_CALL: _print_writer_cstr(fd, "function call"); break ;
		case GSX_TOKEN_CHAR: _print_writer_cstr(fd, "character"); break ;
		case GSX_TOKEN_ARRAY_OPEN: _print_writer_cstr(fd, "array open"); break ;
		case GSX_TOKEN_ARRAY_CLOSE: _print_writer_cstr(fd, "array close"); break ;
		case GSX_TOKEN_ARRAY_SEPARATOR: _print_writer_cstr(fd, "array separator"); break ;
		case GSX_TOKEN_PARAMS_DELIMITER: _print_writer_cstr(fd, "parameters delimiter"); break ;
		case GSX_TOKEN_DESTRUCTURE_SEPARATOR: _print_writer_cstr(fd, "destructure separator"); break ;
		default: {
			_print_writer_cstr(fd, "unknown (");
			_print_writer_long(fd, (long)kind);
			_print_writer_char(fd, ')');
		} break ;
	}
}

void	_gsx_print_writer_token(int fd, struct gsx_token token) {
	_print_writer_cstr(fd, "{ token ");
	_print_writer_view_alt(fd, token.text, '`');
	_print_writer_char(fd, ' ');
	_gsx_print_writer_token_kind(fd, token.kind);
	_print_writer_cstr(fd, " }");
}

void	gsx_print_writer_token_kind(int fd, va_list args) {
	enum gsx_token_kind	kind = va_arg(args, enum gsx_token_kind);
	_gsx_print_writer_token_kind(fd, kind);
}

void	gsx_print_writer_token(int fd, va_list args) {
	struct gsx_token	token = va_arg(args, struct gsx_token);
	_gsx_print_writer_token(fd, token);
}

void	_gsx_print_writer_ast(int fd, struct gsx_ast* node, unsigned indent) {
	if (node == NULL) {
		if (indent == 0u) _print_writer_cstr(fd, "(ast::null)");
		return ;
	}

	for (unsigned i = 0u; i < indent; ++i) print(fd, "\t");
	switch (node->kind) {
		case GSX_AST_CALL: {
			struct gsx_ast_call*	call = node->data.call;
			_print_writer_cstr(fd, "- call ");
			_gsx_print_writer_token(fd, call->name);
			_print_writer_char(fd, '\n');
			for (size_t i = 0; i < call->args.len; ++i) {
				struct gsx_ast*	node = da_at(struct gsx_ast*, &call->args, i);
				_gsx_print_writer_ast(fd, node, indent + 1);
			}
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

void	gsx_print_writer_ast(int fd, va_list args) {
	struct gsx_ast*	node = va_arg(args, struct gsx_ast*);
	_gsx_print_writer_ast(fd, node, 0u);
}

void	print_registery_init_gsx(void) {
	static bool	gsx_print_init = false;
	if (gsx_print_init) return ;

	gsx_print_init = true;
	print_definition_add(view_make_cstr_const("gsx_token"), gsx_print_writer_token);
	print_definition_add(view_make_cstr_const("gsx_token_kind"), gsx_print_writer_token_kind);
	print_definition_add(view_make_cstr_const("gsx_ast"), gsx_print_writer_ast);
}

/*	 Entry_Point	*/

int		main(int argc, char **argv) {
	program_name = argv[0];

	if (argc != 3) {
		print(ttyerr, "%cstr%:\t"TERMINAL_NOTICE_ERROR": Incorrect number of arguments.\n", program_name);
		print(ttyerr, "\t"TERMINAL_NOTICE_USAGE": %cstr% <in.gsx> <out.html>\n", program_name);
		return (1);
	}
	const char*				file_in_path = argv[1];
	const char*				file_out_path = argv[2];

	if (!cstr_suffix(file_in_path, ".gsx")) {
		print(ttyerr, "%cstr%:\t"TERMINAL_NOTICE_ERROR": Invalid input file extension %#cstr%.\n", program_name, file_in_path);
		print(ttyerr, "\t"TERMINAL_NOTICE_USAGE": %cstr% <in.gsx> <out.html>\n", program_name);
		return (2);
	}
	if (cstr_equals(file_in_path, file_out_path)) {
		print(ttyerr, "%cstr%:\t"TERMINAL_NOTICE_ERROR": Reading and writing into the same file %#cstr% is not allowed.\n", program_name, file_out_path);
		print(ttyerr, "\t"TERMINAL_NOTICE_USAGE": %cstr% %#cstr% <out.html>\n", program_name, file_in_path);
		return (2);
	}
	if (!cstr_suffix(file_out_path, ".html")) {
		print(ttyerr, "%cstr%:\t"TERMINAL_NOTICE_ERROR": Invalid output file extension %#cstr%.\n", program_name, file_out_path);
		print(ttyerr, "\t"TERMINAL_NOTICE_USAGE": %cstr% \"%cstr%\" <out.html>\n", program_name, file_in_path);
		return (2);
	}

	struct view			file_in_view = os_file_view(file_in_path);
	if (file_in_view.data == NULL) {
		print(ttyerr, "%cstr%:\t"TERMINAL_NOTICE_ERROR": Could not read file %#cstr%.\n", program_name, file_in_path);
		print(ttyerr, "\t"TERMINAL_NOTICE_USAGE": %cstr% <in.gsx> %`cstr%\n", program_name, file_out_path);
		return (4);
	}

	print_registery_init();
	print_registery_init_gsx();

	struct dynamic_array	definitions = da_make(sizeof(struct gsx_definition));
	gsx_init_definitions(&definitions);

	struct dynamic_array	sections = da_make(sizeof(struct gsx_section));
	if (!gsx_load_sections(file_in_view, &sections)) {
		print(ttyerr, "%cstr%:\t"TERMINAL_NOTICE_ERROR": Invalid gsx file %#cstr%.\n", program_name, file_in_path);
		print(ttyerr, "\t"TERMINAL_NOTICE_HINT": Maybe the file has an unterminated comment somewhere?\n");
		program_return = 8;
		goto sections_free;
	}

	struct dynamic_array	boxes = da_make(sizeof(struct gsx_box*));
	if (!gsx_process_sections(&definitions, &sections, &boxes)) {
		print(ttyerr, "%cstr%:\t"TERMINAL_NOTICE_ERROR": An error happened while processing %#cstr%.\n", program_name, file_in_path);
		program_return = 16;
		goto boxes_free;
	}

	struct string_builder	sb = string_builder_make();
	for (int i = 0; i < (int)boxes.len; ++i) {
		/* struct gsx_section	section = da_at(struct gsx_section, &sections, i); */
		struct gsx_box*		box	= da_at(struct gsx_box*, &boxes, i);
		if (box == NULL) continue ;
		if (box->kind == GSX_BOX_STRING) string_builder_write_cstr(&sb, box->data.str);
		if (box->kind == GSX_BOX_CHAR) string_builder_write_char(&sb, box->data.character);
		if (box->kind == GSX_BOX_INTEGER) string_builder_write_long(&sb, box->data.integer);
		/* struct view		string = view_trim(view_make_cstr_const(str), " \r\n\t"); */
		/* if (section.is_expression && string.len == 0ul) */
		/* 	continue ; */
		/* string_builder_write_cstr(&sb, str); */
	}

	struct view			file_out_view = string_builder_as_view(sb);
	if (file_out_view.data == NULL) {
		print(ttyerr, "%cstr%:\t"TERMINAL_NOTICE_ERROR": An error happened while generating output file content %#cstr%.\n", program_name, file_out_path);
		program_return = 32;
		goto builder_free;
	}
	if (!os_file_dump(file_out_path, file_out_view)) {
		print(ttyerr, "%cstr%:\t"TERMINAL_NOTICE_ERROR": An error happened while generating output file content %#cstr%.\n", program_name, file_out_path);
		print(ttyerr, "\tErrno: %errno%\n");
		program_return = 32;
		goto file_out_free;
	}
	print(ttyout, "%cstr%:\t"TERMINAL_NOTICE_SUCCESS": Evaluated gsx template %#cstr% into %ulong% bytes written in %#cstr%.\n", program_name, file_in_path, file_out_view.len, file_out_path);

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
