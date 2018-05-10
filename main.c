
/*
 * http://www.engr.mun.ca/~theo/Misc/exp_parsing.htm#classic
 */

//#define DEBUG_RUN
//#define DEBUG_LEX

#ifndef DEBUG_RUN
#define NDEBUG
#endif

#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <setjmp.h>
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#define ZB_FMT_VAL "%.14g"
#define ZB_FMT_IDX "%u"
#define ZB_MEM_SIZE 2048
#define ZB_VAR_NAME_LEN 7
#define ZB_VAR_COUNT 32
#define ZB_MAX_DEPTH 8

#define CSI "\033["

typedef float val;
typedef uint32_t idx;

static void printd(const char *fmt, ...);

#define VAR_TYPE_VAL 0
#define VAR_TYPE_CFUNC 1

struct var {
	char name[ZB_VAR_NAME_LEN+1];
	uint8_t type;
	union {
		val v;
		idx i;
		val (*fn)(void);
	};
};

struct loop {
	struct var *var;
	idx ptr_start;
	val v_end, v_step;
};

typedef enum {
	E_SYNTAX_ERROR, E_VAR_MEM_FULL, E_UNTERM_STR, E_MEM_FULL,
	E_EXPECTED, E_DIV_BY_ZERO, E_NESTED_RUN, E_LINE_NOT_FOUND,
	E_STACK_OVERFLOW, E_NEXT_WITHOUT_FOR, E_ASSERT, E_NOT_LVALUE,
} zb_err;

typedef enum {

	/* Operators */

	TOK_ASSIGN, TOK_MINUS, TOK_PLUS, TOK_MUL, TOK_DIV, TOK_MOD, TOK_LT,
	TOK_LE, TOK_EQ, TOK_NE, TOK_GE, TOK_GT, TOK_POW, TOK_AND, TOK_OR,
	TOK_BAND, TOK_BOR, TOK_BXOR, TOK_NOT, TOK_BNOT,

	/* Keywords */

	TOK_ELSE, TOK_FOR, TOK_GOSUB, TOK_GOTO, TOK_IF, TOK_NEXT,
	TOK_RETURN, TOK_RUN, TOK_THEN, TOK_TO, TOK_PRINT, TOK_END, TOK_STEP,
	TOK_COLON, TOK_OPEN, TOK_CLOSE, TOK_SEMI, TOK_COMMA,

	/* Other */

	TOK_CHUNK, TOK_LIT, TOK_VAR, TOK_STR, TOK_NONE, TOK_EOF,

	NUM_TOKENS
} zb_tok;

#define OPERATOR_COUNT (TOK_BNOT+1)

#define _ "\0"

static const char tokstr[] =

	"=" _ "-" _ "+" _ "*" _ "/" _ "%" _ "<" _ "<=" _ "==" _ "<>" _ ">=" _
	">" _ "**" _ "and" _ "or" _ "&" _ "|" _ "^" _ "!" _ "~" _

	"else" _ "for" _ "gosub" _ "goto" _ "if" _ "next" _ "return" _
	"run" _ "then" _ "to" _ "print" _ "end" _ "step" _ ":" _ 
	"("_ ")" _ ";" _ "," _

	"CHU" _ "LIT" _ "VAR" _ "STR" _ "NON" _ "EOF";

static const char *errmsg[] = {
	[E_SYNTAX_ERROR] = "Syntax error",
	[E_VAR_MEM_FULL] = "Too many variables",
	[E_UNTERM_STR] = "Unterminated string",
	[E_MEM_FULL] = "Pool full",
	[E_EXPECTED] = "Expected",
	[E_DIV_BY_ZERO] = "Division by zero",
	[E_NESTED_RUN] = "Nested run",
	[E_LINE_NOT_FOUND] = "Line not found",
	[E_STACK_OVERFLOW] = "Stack overflow",
	[E_NEXT_WITHOUT_FOR] = "Next without for",
	[E_ASSERT] = "Assert failed",
	[E_NOT_LVALUE] = "Not an lvalue",
};


static struct var vars[ZB_VAR_COUNT];
static uint8_t mem[ZB_MEM_SIZE];
static idx cur = 0, end = 0;
static jmp_buf jmpbuf;
static bool running = false;
static struct loop loop_stack[ZB_MAX_DEPTH];
static idx loop_head;


static void error(zb_err e, const char *ctx)
{
	const char *msg = errmsg[e];
	if(!ctx) ctx = "";
	fprintf(stderr, CSI "31m%s %s\033[0m\n", msg, ctx);
	longjmp(jmpbuf, 1);
}


#define error_if(exp, e, msg) { if(exp) error(e, msg); }


static const char *tokname(idx i)
{
	idx j = 0;
	if(i >= NUM_TOKENS) return "?";
	const char *p = tokstr;
	for(j=0; j<i; j++) while(*p++);
	return p;
}


static bool match(const char *p, const char *s, size_t len)
{
	size_t i;
	for(i=0; i<len; i++) {
		if(*p++ != *s++) return false;
	}
	return *p == '\0';
}


static zb_tok find_tok(const char *name, size_t len)
{
	const char *p = tokstr;
	zb_tok i;
	for(i=0; i<NUM_TOKENS; i++) {
		if(match(p, name, len)) return i;
		while(*p++);
	}
	return TOK_NONE;
}


static void dump_vars(void)
{
	int i;
	for(i=0; i<ZB_VAR_COUNT; i++) {
		struct var *var = &vars[i];
		if(var->name[0] == 0) continue;
		if(var->type == VAR_TYPE_VAL) {
			printf("  %d: %s = " ZB_FMT_VAL "\n", i, var->name, var->v);
		}
		if(var->type == VAR_TYPE_CFUNC) {
			printf("  %d: %s = CFUNC\n", i, var->name);
		}
	}
}


static idx find_var(const char *name, size_t len)
{
	if(len > ZB_VAR_NAME_LEN) len = ZB_VAR_NAME_LEN;
	idx i, ifree=ZB_VAR_COUNT;
	for(i=0; i<ZB_VAR_COUNT; i++) {
		if(match(vars[i].name, name, len)) {
			return i;
		}
		if(ifree == ZB_VAR_COUNT && vars[i].name[0] == '\0') {
			ifree = i;
		}
	}
	error_if(ifree == ZB_VAR_COUNT, E_VAR_MEM_FULL, NULL);
	strncpy(vars[ifree].name, name, len);
	printd("var alloc " ZB_FMT_IDX " -> %s at ", i, vars[ifree].name);
	return ifree;
}


static void put_buf(const void *buf, size_t len)
{
	error_if(end + len >= ZB_MEM_SIZE, E_MEM_FULL, NULL);

	memcpy(mem+end, buf, len);
	end += len;

}


static void put_tok(uint8_t c)
{
	put_buf(&c, 1);
}


static void put_lit(val v)
{
	put_tok(TOK_LIT);
	int vi = v;
	uint8_t b[1 + sizeof(val)];
	size_t n=0;
	if(v == vi && vi < 128) {
		b[0] = vi;
		n = 1;
	} else if(v == vi && vi < 32512) {
		b[0] = (vi >> 8) | 0x80;
		b[1] = vi & 0xff;
		n = 2;
	} else {
		b[0] = 0xff;
		*(val *)(&b[1]) = v;
		n = 1 + sizeof(val);
	}
	put_buf(b, n);
}


static void put_var(idx i)
{
	uint8_t b[2] = { TOK_VAR, i };
	put_buf(b, sizeof(b));
}


static void put_str(const char *src, size_t len)
{
	put_tok(TOK_STR);
	put_tok(len);
	put_buf(src, len);
	put_tok('\0');
}


static void put_chunk(void)
{
	uint8_t buf[4] = { TOK_CHUNK, 0, 0, 0 };
	put_buf(buf, sizeof(buf));
}


static void set_chunk_len(idx ptr, idx len)
{
	mem[ptr+1] = len;
}


static void set_chunk_line(idx ptr, idx line)
{
	mem[ptr+2] = line >> 8;
	mem[ptr+3] = line & 0xff;
}


static bool is_digit(char c)
  { return c >= '0' && c <= '9'; }

static bool is_alpha(char c)
  { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }

static bool is_alnum(char c)
  { return is_alpha(c) || is_digit(c); }

static bool is_hexdigit(char c)
  { return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

static bool lex(const char *line)
{
	idx start = end;
	const char *p = line;
	bool first = true;
	bool exec = true;

	put_chunk();

	for(;;) {

		while(*p == ' ' || *p == '\t' || *p == '\r') p++;
		const char *q = p;
		idx prev = end;

		if(*p == '\n' || *p == '\0') {
			put_tok(TOK_EOF);
			break;
		} else if(is_digit(*p)) {
			val v = atof(p);
			if(first) {
				set_chunk_line(start, v);
				exec = false;
			} else {
				put_lit(v);
			}
			if(*p == '0' && *(p+1) == 'x') {
				p+=2;
				while(is_hexdigit(*p)) p++;
			} else {
				while(is_alnum(*p)) p++;
				if(*p == '.') p++;
				while(is_alnum(*p)) p++;
			}
			p--;
		} else if(is_alpha(*p)) {
			const char *ps = p;
			while(is_alnum(*p) || *p == '$') p++;
			zb_tok tok = find_tok(ps, p-ps);
			if(tok == TOK_NONE) {
				put_var(find_var(ps, p-ps));
			} else {
				put_tok(tok);
			}
			p--;
		} else if(*p == '"') {
			const char *ps = ++p;
			while(*p != '"') {
				error_if(*p == '\0', E_UNTERM_STR, NULL);
				p++;
			}
			put_str(ps, p-ps);
		} else if(*p == '\'') {
			p++;
			put_lit(*p++);
			if(*p != '\'') error(E_UNTERM_STR, NULL);
		} else if(*p == '=') {
			if(*(p+1) == '=') {
				put_tok(TOK_EQ);
				p++;
			} else {
				put_tok(TOK_ASSIGN);
			}
		} else if(*p == '<') {
			if(*(p+1) == '=') {
				put_tok(TOK_LE);
				p++;
			} else if(*(p+1) == '>') {
				put_tok(TOK_NE);
				p++;
			} else {
				put_tok(TOK_LT);
			}
		} else if(*p == '>') {
			if(*(p+1) == '=') {
				put_tok(TOK_GE);
				p++;
			} else {
				put_tok(TOK_GT);
			}
		} else if(*p == '*') {
			if(*(p+1) == '*') {
				put_tok(TOK_POW);
				p++;
			} else {
				put_tok(TOK_MUL);
			}
		} else if(*p == '!') {
			if(*(p+1) == '=') {
				put_tok(TOK_NE);
				p++;
			} else {
				put_tok(TOK_NOT);
			}
		}
		else if(*p == ':') put_tok(TOK_COLON);
		else if(*p == '-') put_tok(TOK_MINUS);
		else if(*p == '+') put_tok(TOK_PLUS);
		else if(*p == '&') put_tok(TOK_BAND);
		else if(*p == '|') put_tok(TOK_BOR);
		else if(*p == '^') put_tok(TOK_BXOR);
		else if(*p == '~') put_tok(TOK_BNOT);
		else if(*p == '%') put_tok(TOK_MOD);
		else if(*p == '^') put_tok(TOK_POW);
		else if(*p == '(') put_tok(TOK_OPEN);
		else if(*p == ')') put_tok(TOK_CLOSE);
		else if(*p == '/') put_tok(TOK_DIV);
		else if(*p == '/') put_tok(TOK_MOD);
		else if(*p == ',') put_tok(TOK_COMMA);
		else if(*p == ';') put_tok(TOK_SEMI);
		else error(E_SYNTAX_ERROR, p);

		p++;

#ifdef DEBUG_LEX
		if(first) putchar('\n');
		printf(CSI "36m%5d | ", (int)prev);
		size_t l = fwrite(q, 1, p-q, stdout);
		for(;l<10; l++) putchar(' ');
		printf(" | ");
		size_t i;
		for(i=0; i<end-prev; i++) printf("%02x ", mem[prev+i]);
		printf(CSI "0m\n");
#endif
		first = false;
	}

	set_chunk_len(start, end - start);

	return exec;
}


static zb_tok cur_tok(void)
{
	assert(cur < ZB_MEM_SIZE);
	return (zb_tok)mem[cur];
}


static bool cur_is(zb_tok tok)
{
	return cur_tok() == tok;
}


static bool next_is(zb_tok tok)
{
	if(cur_tok() == tok) {
		cur++;
		return true;
	}
	return false;
}


static void expect(zb_tok tok)
{
	error_if(!next_is(tok), E_EXPECTED, tokname(tok));
}


static val get_lit(void)
{
	printd("get_lit");
	val v = 0;
	expect(TOK_LIT);
	uint8_t b0 = mem[cur++];
	if(b0 == 0xff) {
		v = *(val *)(mem+cur);
		cur += sizeof(val);
	} else {
		if(b0 & 0x80) {
			uint8_t b1 = mem[cur++];
			v = (((b0 & 0x7f) << 8) | b1);
		} else {
			v = b0;
		}
	}
	return v;
}


static idx get_var_idx(void)
{
	expect(TOK_VAR);
	idx i = mem[cur++];
	return i;
}


static val get_var(idx *i)
{
	*i = get_var_idx();
	val v = 0;
	struct var *var = &vars[*i];
	if(var->type == VAR_TYPE_VAL) {
		v = var->v;
	} else if(var->type == VAR_TYPE_CFUNC) {
		expect(TOK_OPEN);
		v = var->fn();
		expect(TOK_CLOSE);
		printd("cfunc %s() = " ZB_FMT_VAL, var->name, v);
	}
	return v;
}


static idx get_str_idx(void)
{
	expect(TOK_STR);
	size_t len = mem[cur];
	idx ptr = cur + 1;
	cur += len + 2;
	return ptr;
}


static const char *get_str(void)
{
	idx ptr = get_str_idx();
	return (char *)(mem + ptr);
}


static void get_chunk(idx *len, idx *line)
{
	expect(TOK_CHUNK);
	if(len) *len = mem[cur];
	if(line) *line = (mem[cur+1] << 8) + mem[cur+2];
	cur += 3;
}


static void dummy_iterator(zb_tok tok, val v, idx i) {}


static zb_tok get_tok(val *v, idx *i)
{
	zb_tok tok = cur_tok();
	printd("get_tok");

	if(cur_is(TOK_LIT)) {
		*v = get_lit();
	} else if(cur_is(TOK_STR)) {
		*i = get_str_idx();
	} else if(cur_is(TOK_VAR)) {
		*i = get_var_idx();
		*v = vars[*i].v;
	} else if(cur_is(TOK_CHUNK)) {
		get_chunk(NULL, i);
	} else {
		cur ++;
	}

	return tok;
}


static void iter_chunk(void (*fn)(zb_tok tok, val v, idx i))
{
	if(fn == NULL) fn = dummy_iterator;

	for(;;) {
		val v = 0;
		idx i = 0;
		zb_tok tok = get_tok(&v, &i);
		if(tok == TOK_EOF) break;
		fn(tok, v, i);
	}
}


static void on_list(zb_tok tok, val v, idx i)
{
	if(tok == TOK_CHUNK) {
		printf(ZB_FMT_IDX " ", i);
	} else if(tok == TOK_LIT) {
		printf(ZB_FMT_VAL " ", v);
	} else if(tok == TOK_STR) {
		printf("\"%s\" ", mem+i);
	} else if(tok == TOK_VAR) {
		printf("%s ", vars[i].name);
	} else {
		printf("%s ", tokname(tok));
	}
}


static void list_chunk(void)
{
	idx save = cur;
	printf(ZB_FMT_IDX ") ", cur);
	iter_chunk(on_list);
	printf("\n");
	cur = save;
}


static void printd(const char *fmt, ...)
{
#ifdef DEBUG_RUN
	static int in_printd = 0;
	if(in_printd) return;
	in_printd ++;

	idx save = cur;

	printf(CSI "30;1m");
	printf("%4d | ", (int)cur);

	va_list va;
	va_start(va, fmt);
	int i = vprintf(fmt, va);
	va_end(va);

	for(; i<30; i++) putchar(' ');
	printf("| %-10.10s", tokname(cur_tok()));

	if(cur_is(TOK_LIT)) {
		printf("| " ZB_FMT_VAL, get_lit());
	} else if(cur_is(TOK_STR)) {
		printf("| \"%s\"", get_str());
	} else if(cur_is(TOK_VAR)) {
		idx i = mem[cur+1];
		struct var *v = &vars[i];
		printf("| %s=" ZB_FMT_VAL, v->name, v->v);
	} else if(cur_is(TOK_CHUNK)) {
		idx len, line;
		get_chunk(&len, &line);
		printf("| chunk line=" ZB_FMT_IDX " len=" ZB_FMT_IDX, line, len);
	}
	printf("\n\033[0m");

	cur = save;
	in_printd--;
#endif
}


static bool cur_is_operator(void)
{
	return cur_tok() < OPERATOR_COUNT;
}


static uint8_t op_prec[OPERATOR_COUNT] = {
	[TOK_OR]     = 0,
	[TOK_AND]    = 1,
	[TOK_BAND]   = 1,
	[TOK_BOR]    = 1,
	[TOK_BXOR]   = 1,
	[TOK_ASSIGN] = 2,
	[TOK_LT]     = 3,
	[TOK_LE]     = 3,
	[TOK_EQ]     = 3,
	[TOK_NE]     = 3,
	[TOK_GE]     = 3,
	[TOK_GT]     = 3,
	[TOK_PLUS]   = 4,
	[TOK_MINUS]  = 4,
	/* unary minus = 5 */
	[TOK_MUL]    = 6,
	[TOK_DIV]    = 6,
	[TOK_MOD]    = 6,
	[TOK_POW]    = 7,
};

static int cur_prec(void)
{
	zb_tok t = cur_tok();
	return op_prec[t];
}


static val P(idx *i);

static val E(int p)
{
	idx lvalue = ZB_VAR_COUNT;
	val v = P(&lvalue);

	int prec;

	while(cur_is_operator() && (prec = cur_prec()) >= p) {

		printd("  E %d %d", prec, p);

		zb_tok tok = get_tok(NULL, NULL);
		int q = (tok == TOK_POW || tok == TOK_ASSIGN) ? prec : prec + 1;

		val v1 = v;
		val v2 = E(q);

		     if(tok == TOK_PLUS) v = v1 + v2;
		else if(tok == TOK_MINUS) v = v1 - v2;
		else if(tok == TOK_MUL) v = v1 * v2;
		else if(tok == TOK_DIV) v = v1 / v2;
		else if(tok == TOK_MOD) v = (int)v1 % (int)v2;
		else if(tok == TOK_POW) v = pow(v1, v2);
		else if(tok == TOK_LT) v = v1 < v2;
		else if(tok == TOK_LE) v = v1 <= v2;
		else if(tok == TOK_EQ) v = v1 == v2;
		else if(tok == TOK_NE) v = v1 != v2;
		else if(tok == TOK_GE) v = v1 >= v2;
		else if(tok == TOK_GT) v = v1 > v2;
		else if(tok == TOK_AND) v = v1 && v2;
		else if(tok == TOK_OR) v = v1 || v2;
		else if(tok == TOK_BAND) v = (int)v1 & (int)v2;
		else if(tok == TOK_BOR) v = (int)v1 | (int)v2;
		else if(tok == TOK_BXOR) v = (int)v1 ^ (int)v2;
		else if(tok == TOK_ASSIGN) {
			error_if(lvalue == ZB_VAR_COUNT, E_NOT_LVALUE, NULL);
			struct var *var = &vars[lvalue];
			var->v = v = v2;
			var->type = VAR_TYPE_VAL;
		}
		else error(E_ASSERT, NULL);

		printd("    " ZB_FMT_VAL " %s " ZB_FMT_VAL " = " ZB_FMT_VAL, v1, tokname(tok), v2, v);
	}

	printd("  ret " ZB_FMT_VAL, v);
	return v;
}


static val P(idx *i)
{
	val v = 0;

	if(cur_is(TOK_LIT)) {
		v = get_lit();
	} else if(cur_is(TOK_VAR)) {
		v = get_var(i);
	} else if(next_is(TOK_MINUS)) {
		v = -E(5); /* unary minus precedence */
	} else if(next_is(TOK_NOT)) {
		v = !E(8); /* logical not precedence */
	} else if(next_is(TOK_BNOT)) {
		v = ~(int)E(8); /* logical not precedence */
	} else if(next_is(TOK_OPEN)) {
		v = E(0);
		expect(TOK_CLOSE);
	} else {
		error(E_EXPECTED, "expression");
	}

	return v;
}


static val expr(void)
{
	return E(0);
}


static val fn_print(void)
{
	printd("print");
	for(;;) {
		if(cur_is(TOK_STR)) {
			printf("%s", get_str());
		} else {
			printf(ZB_FMT_VAL " ", expr());
		}
		if(!next_is(TOK_SEMI)) break;
	}
	putchar('\n');

	return 0;
}


static bool run_chunk(bool once);


static void run(idx ptr)
{
	idx save = cur;
	cur = ptr;

	printd("run");

	while(running) {
		bool ret = run_chunk(false);
		if(ret) break;
	}

	cur = save;
}


static void fn_run(void)
{
	error_if(running, E_NESTED_RUN, NULL);
	running = true;
	loop_head = 0;
	run(0);
	running = false;
}


static idx find_line(idx line)
{
	idx save = cur;
	cur = 0;

	for(;;) {
		idx ptr = cur;
		idx len, line2;
		get_chunk(&len, &line2);
		if(line == line2) {
			cur = save;
			return ptr;
		}
		cur = ptr + len;
	}

	error(E_LINE_NOT_FOUND, NULL);
	return 0;
}


static void fn_goto(void)
{
	idx line = get_lit();
	idx ptr = find_line(line);
	printd("goto " ZB_FMT_IDX " at " ZB_FMT_IDX, line, ptr);
	cur = ptr;
}


static void fn_gosub(void)
{
	idx line = get_lit();
	idx ptr = find_line(line);
	printd("gosub " ZB_FMT_IDX " at " ZB_FMT_IDX, line, ptr);
	run(ptr);
}


static void fn_for(void)
{
	if(loop_head == ZB_MAX_DEPTH) error(E_NEXT_WITHOUT_FOR, NULL);
	struct loop *loop = &loop_stack[loop_head ++];
	loop->var = &vars[get_var_idx()];
	expect(TOK_ASSIGN);
	loop->var->v = expr();
	expect(TOK_TO);
	loop->v_end = expr();
	loop->v_step = next_is(TOK_STEP) ? expr() : 1;
	loop->ptr_start = cur;

	printd("for %s = " ZB_FMT_VAL " to " ZB_FMT_VAL " step " ZB_FMT_VAL, 
			loop->var->name, loop->var->v, loop->v_end, loop->v_step);
}


static void fn_next(void)
{
	if(loop_head == 0) error(E_NEXT_WITHOUT_FOR, NULL);
	struct loop *loop = &loop_stack[loop_head-1];
	struct var *var = loop->var;
	var->v += loop->v_step;

	if((loop->v_step > 0 && var->v <= loop->v_end) ||
	   (loop->v_step < 0 && var->v >= loop->v_end)) {
		printd("next %s = " ZB_FMT_VAL " to " ZB_FMT_VAL,
				loop->var->name, loop->var->v, loop->v_end);
		cur = loop->ptr_start;
	} else {
		loop_head--;
		printd("next %s done", loop->var->name);
	}
}


static void fn_if(void)
{
	val v = expr();
	printd("if expr = " ZB_FMT_VAL, v);
	expect(TOK_THEN);
	if(v) {
		run_chunk(true);
	} else {
		while(!cur_is(TOK_EOF) && !cur_is(TOK_ELSE) && !cur_is(TOK_COLON)) {
			val v; idx i;
			get_tok(&v, &i);
		}
		if(next_is(TOK_ELSE)) { /* consume else */ }
	}
}


static void fn_else(void)
{
	while(!cur_is(TOK_EOF) && !cur_is(TOK_COLON)) {
		val v; idx i;
		get_tok(&v, &i);
	}
}


static bool run_chunk(bool once)
{
	do {
		printd("run_chunk");

		     if(cur_is(TOK_CHUNK)) get_chunk(NULL, NULL);
		else if(next_is(TOK_PRINT)) fn_print();
		else if(next_is(TOK_RUN)) fn_run();
		else if(next_is(TOK_GOTO)) fn_goto();
		else if(next_is(TOK_GOSUB)) fn_gosub();
		else if(next_is(TOK_RETURN)) return true;
		else if(next_is(TOK_FOR)) fn_for();
		else if(next_is(TOK_NEXT)) fn_next();
		else if(next_is(TOK_IF)) fn_if();
		else if(next_is(TOK_ELSE)) fn_else();
		else if(next_is(TOK_NEXT)) return true;
		else if(next_is(TOK_COLON)) /* next statement */;
		else if(next_is(TOK_END)) running = false;
		else if(next_is(TOK_EOF)) break;
		else expr();
	} while(!once);

	return false;
}


static void handle_line(const char *line)
{
	idx save = end;

	bool exec = lex(line);

	if(0) {
		list_chunk();
		cur = save;
	}

	if(exec) {
		cur = save;
		run_chunk(false);
		end = save;
		cur = save;
	}
}


static void zb_register_cfunc(const char *name, val (*fn)(void))
{
	idx i = find_var(name, strlen(name));
	struct var *var = &vars[i];
	var->type = VAR_TYPE_CFUNC;
	var->fn = fn;
}


static val fn_rnd(void)
{
	return rand() / (val)RAND_MAX;
}


static val fn_putc(void)
{
	return putchar(expr());
}


static val fn_plot(void)
{
	static int colorcode[] = { 30, 34, 32, 36, 31, 35, 33, 37 };

	int x = expr();
	expect(TOK_COMMA);
	int y = expr();
	expect(TOK_COMMA);
	int color = expr();

	printf(CSI "s\033[%d;%dH", y, x*2);
	printf(CSI "%d;%d;7m  \033[0m\033[u", color >= 8, colorcode[color % 8]);
	fflush(stdout);

	return 0;
}


static val fn_cls(void)
{
	return 0;
}


static val fn_exit(void)
{
	exit(expr());
}


int main(int argc, char **argv)
{
	srand(time(NULL));

	char line[120];

	zb_register_cfunc("rnd", fn_rnd);
	zb_register_cfunc("putc", fn_putc);
	zb_register_cfunc("plot", fn_plot);
	zb_register_cfunc("cls", fn_cls);
	zb_register_cfunc("exit", fn_exit);

	while(fgets(line, sizeof(line), stdin) != NULL) {
		char *p = strchr(line, '\n'); if(p) *p = '\0';
		if(setjmp(jmpbuf) == 0) {
			handle_line(line);
		} else {
			running = false;
		};
	}

	if(0) dump_vars();
	if(0) list_chunk();

	return 0;
}

/*
 * vi: ft=c
 */

