
/*
 * http://www.engr.mun.ca/~theo/Misc/exp_parsing.htm#classic
 */

#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>

//#define DEBUG

enum tok {
	TOK_EOF,
	TOK_NONE,

	TOK_AND, TOK_ASSIGN, TOK_CLS, TOK_CLOSE, TOK_COLON, TOK_COMMA, TOK_DIV,
	TOK_ELSE, TOK_END, TOK_EQ, TOK_FOR, TOK_GE, TOK_GOSUB, TOK_GOTO,
	TOK_GT, TOK_IF, TOK_LE, TOK_LIST, TOK_LT, TOK_MINUS, TOK_MOD, TOK_MUL,
	TOK_NE, TOK_NEXT, TOK_NUMBER, TOK_OPEN, TOK_OR, TOK_PLOT, TOK_PLUS,
	TOK_POW, TOK_PRINT, TOK_QUIT, TOK_REM, TOK_RETURN, TOK_RND, TOK_RUN,
	TOK_SQRT, TOK_SLEEP, TOK_STEP, TOK_STRING, TOK_THEN, TOK_TO, TOK_NAME,
};

typedef float val;
#define VAL_FMT "%.4g"

static val fn_print(void);
static val fn_plot(void);
static val fn_if(void);
static val fn_gosub(void);
static val fn_goto(void);
static val fn_rem(void);
static val fn_return(void);
static val fn_cls(void);
static val fn_list(void);
static val fn_end(void);
static val fn_for(void);
static val fn_next(void);
static val fn_quit(void);
static val fn_run(void);
static val fn_assign(void);
static val fn_sleep(void);
static val fn_rnd(void);
static val fn_sqrt(void);


const char *tokens[] = {

        /* Generic tokens */

        [TOK_NAME] =    "NAME",
        [TOK_NONE] =    "NONE",
        [TOK_STRING] =  "STRING",
        [TOK_EOF] =     "EOF",
        [TOK_NUMBER] =  "NUMBER",

        /* Expression tokens */

        [TOK_AND] =     "and",
        [TOK_ASSIGN] =  "=",
        [TOK_CLOSE] =   ")",
        [TOK_COLON] =   ":",
        [TOK_COMMA] =   ",",
        [TOK_DIV] =     "/",
        [TOK_EQ] =      "==",
        [TOK_GE] =      ">=",
        [TOK_GT] =      ">",
        [TOK_LE] =      "<=",
        [TOK_LT] =      "<",
        [TOK_MINUS] =   "-",
        [TOK_MOD] =     "%",
        [TOK_MUL] =     "*",
        [TOK_NE] =      "!=",
        [TOK_OPEN] =    "(",
        [TOK_OR] =      "or",
        [TOK_PLUS] =    "+",
        [TOK_POW] =     "^",

        /* Statements and functions */

        [TOK_CLS] =     "cls",
        [TOK_END] =     "end",
	[TOK_FOR] =     "for",
	  [TOK_TO] =    "to",
	  [TOK_STEP] =  "step",
        [TOK_NEXT] =    "next",
        [TOK_GOSUB] =   "gosub",
        [TOK_GOTO] =    "goto",
        [TOK_IF] =      "if",
          [TOK_THEN] =  "then",
          [TOK_ELSE] =  "else",
        [TOK_LIST] =    "list",
        [TOK_PLOT] =    "plot",
        [TOK_PRINT] =   "print",
        [TOK_QUIT] =    "quit",
        [TOK_REM] =     "rem",
        [TOK_RETURN] =  "return",
        [TOK_RND] =     "rnd",
        [TOK_RUN] =     "run",
        [TOK_SQRT] =    "sqrt",
        [TOK_SLEEP] =   "sleep",
};

#define NUM_TOKS (sizeof(tokens) / sizeof(tokens[0]))

/* Parser state */

static char *buf;
static enum tok tok;
static const char *tokname = "";
static size_t toklen;
static int cs;
static val cur_num;
static char *ts, *te;
static char *p, *pe;
static char *eof = NULL;
static int act;
static uint8_t *pc;

/* Interpreter state */

#define CALL_STACK_SIZE 16
#define LOOP_STACK_SIZE 8
#define MAX_VARS 32
#define MAX_VAR_LEN 5
#define MAX_LINES 256

struct var {
	char name[MAX_VAR_LEN+1];
	val v;
};

struct loop {
	struct var *var;
	val v_step;
	val v_end;
	int line;
};

struct line {
	uint8_t buf[64];
};

static struct line lines[MAX_LINES];
static jmp_buf jmpbuf;
static bool running = false;
static struct var var_list[MAX_VARS];
static int call_stack[CALL_STACK_SIZE];
static int call_head = 0;
static struct loop loop_stack[LOOP_STACK_SIZE];
static int loop_head = 0;
static int cur_line;
struct var *cur_var;
static val statement();
static void line(void);
static int depth = 0;


static void error(const char *fmt, ...)
{
	va_list va;
	printf("\e[31;1m");
	va_start(va, fmt);
	if(running) {
		printf("Error at line %d: ", cur_line);
	} else {
		printf("Error: ");
	}
	vprintf(fmt, va);
	printf("\n");
	va_end(va);
	printf("\e[0m");
	longjmp(jmpbuf, 1);
}

#ifdef DEBUG

static void vprintd(const char *fmt, va_list va)
{
	int i;
	for(i=0; i<depth; i++) printf(" ");
	printf("\e[30;1m");
	i += vprintf(fmt, va);
	for(; i<30; i++) printf(" ");
	printf(" | %-10s | ", tokname);
	if(te > ts) {
		fwrite(ts, te-ts, 1, stdout);
	}
	printf("\n\e[0m");
}


static void printd_in(const char *fmt, ...)
{
	depth ++;
	va_list va;
	va_start(va, fmt);
	vprintd(fmt, va);
	va_end(va);
}

static void printd_out(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	vprintd(fmt, va);
	va_end(va);
	depth --;
}

static void printd(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	vprintd(fmt, va);
	va_end(va);
}

#else
#define printd(...)
#define printd_in(...)
#define printd_out(...)
#endif

static struct var *var_find(const char *name, size_t len);
static void next_compiled(void);
static void next_text(void);


/*
 * Get next token, method depends on current running state
 */

static void next()
{
	if(running) {
		next_compiled();
	} else {
		next_text();
	}
}


/*
 * Get next token from compiled line
 */

static void next_compiled(void)
{
	cur_var = NULL;
	tok = *pc++;

	if(tok == TOK_NUMBER) {
		cur_num = *(float *)pc;
		pc += sizeof(cur_num);
	} else if(tok == TOK_NAME) {
		cur_var = &var_list[*pc++];
	} else if(tok == TOK_STRING) {
		size_t len = *pc++;
		ts = (char *)pc-1;
		te = (char*)ts+len;
		toklen = te - ts;
		pc += len+1;
	}
	tokname = tokens[tok];
}



static void next_text(void)
{
	cur_var = NULL;
	tok = TOK_NONE;

	while(tok == TOK_NONE) {

		ts = p;

		     if(*p == ' ' || *p == '\t') { p++; }
		else if(*p == '\0' || *p == '\n' || *p == '\r') {
			tok = TOK_EOF;
		}
		else if(*p == ')') { tok = TOK_CLOSE; }
		else if(*p == ':') { tok = TOK_COLON; }
		else if(*p == ',') { tok = TOK_COMMA; }
		else if(*p == '/') { tok = TOK_DIV; }
		else if(*p == '-') { tok = TOK_MINUS; }
		else if(*p == '%') { tok = TOK_MOD; }
		else if(*p == '*') { tok = TOK_MUL; }
		else if(*p == '#') { tok = TOK_NE; }
		else if(*p == '(') { tok = TOK_OPEN; }
		else if(*p == '+') { tok = TOK_PLUS; }
		else if(*p == '^') { tok = TOK_POW; }
		else if(*p == '?') { tok = TOK_PRINT; }
		else if(*p == '=') {
			if(*(p+1) == '=') {
				p++;
				tok = TOK_EQ;
			} else {
				tok = TOK_ASSIGN;
			}
		} else if(*p == '>') {
			if(*(p+1) == '=') {
				p++;
				tok = TOK_GE;
			} else {
				tok = TOK_GT;
			}
		} else if(*p == '<') {
			if(*(p+1) == '=') {
				p++;
				tok = TOK_LE;
			} else {
				tok = TOK_LT;
			}
		} else if(*p == '!') {
			if(*(p+1) == '=') {
				tok = TOK_NE;
			} else {
				error("syntax error");
			}
		} else if(isdigit(*p) || *p == '.') {
			cur_num = atof(p);
			while(isdigit(*p)) p++;
			if(*p == '.') {
				p++;
				while(isdigit(*p)) p++;
			}
			if(*p == 'e' || *p == 'E') {
				p++;
				if(*p == '+' || *p == '-') p++;
				while(isdigit(*p)) p++;
			}
			p--;
			tok = TOK_NUMBER;
		} else if(isalpha(*p)) {
			ts = p;
			while(isalpha(*(p+1))) p++;
			te = p;
			tok = TOK_NAME;
		} else if(*p == '"') {
			ts = p;
			while(*p && *p != '"') p++;
			te = p;
			tok = TOK_STRING;
		} else {
			error("syntax error");
		}
	}

	te = ++p;

	//printf("ts = '%s\n", ts);
	//printf("te = '%s\n", te);

	toklen = te-ts;

	if(tok == TOK_NAME) {
		size_t i;
		for(i=0; i<NUM_TOKS; i++) {
			if(strlen(tokens[i]) == toklen &&
			   strncasecmp(tokens[i], ts, toklen) == 0) {
				tok = i;
				break;
			}
		}
	}

	if(tok == TOK_NAME) {
		cur_var = var_find(ts, te-ts);
	}

	tokname = tokens[tok];
}


static struct var *var_find(const char *name, size_t len)
{
	if(len > MAX_VAR_LEN) error("var name too long");

	int i;
	struct var *vfree = NULL;
	for(i=0; i<MAX_VARS; i++) {
		struct var *v = &var_list[i];
		if(len == strlen(v->name) &&
			  strncasecmp(v->name, name, len) == 0) {
			return v;
		}
		if(vfree == NULL && v->name[0] == '\0') {
			vfree = v;
		}
	}

	if(!vfree) error("out of var mem");
	strncpy(vfree->name, name, len);
	vfree->v = 0;
	return vfree;
}


bool next_is(enum tok t)
{
	if(tok == t) {
		next();
		return true;
	} else {
		return false;
	}
}


static void expect(enum tok t)
{
	printd("expect %s", tokens[t]);
	if(!next_is(t)) {
		error("expected %s", tokens[t]);
	}
}



static val expr();
static val expr_L();
static val expr_E();
static val expr_T();
static val expr_C();
static val expr_F();
static val expr_P();


static val expr()
{
	return expr_L();
}


/*  L --> C {( "and" | "or" ) C} */

static val expr_L()
{
	printd_in("expr_L");
	val v = expr_C();
	while(tok == TOK_AND || tok == TOK_OR) {
		printd("expr_L next");
		if(next_is(TOK_AND)) v = expr_C() && v;
		if(next_is(TOK_OR))  v = expr_C() || v;
	}
	printd_out("expr_L -> " VAL_FMT, v);
	return v;
}


/*  C --> E {( "<=" | "<" | "==" | "!=" | ">=" | ">") E} */

static val expr_C()
{
	printd_in("expr");
	val v = expr_E();
	printd("expr next");
	     if(next_is(TOK_LT)) v = v <  expr_E();
	else if(next_is(TOK_LE)) v = v <= expr_E();
	else if(next_is(TOK_EQ)) v = v == expr_E();
	else if(next_is(TOK_NE)) v = v != expr_E();
	else if(next_is(TOK_GE)) v = v >= expr_E();
	else if(next_is(TOK_GT)) v = v >  expr_E();
	printd_out("expr -> " VAL_FMT, v);
	return v;
}


/*  E --> T {( "+" | "-" ) T} */

static val expr_E()
{
	printd_in("expr_E");
	val v = expr_T();
	while(tok == TOK_PLUS || tok == TOK_MINUS) {
		printd("expr_E next");
		if(next_is(TOK_PLUS))  v += expr_T();
		if(next_is(TOK_MINUS)) v -= expr_T();
	}
	printd_out("expr_E -> " VAL_FMT, v);
	return v;
}


/* T --> F {( "*" | "/" | "%") F} */

static val expr_T()
{
	printd_in("expr_T");
	val v = expr_F();
	printd("expr_T next");
	while(tok == TOK_MUL || tok == TOK_DIV || tok == TOK_MOD) {
		if(next_is(TOK_MUL)) v *= expr_F();
		if(next_is(TOK_DIV)) v /= expr_F();
		if(next_is(TOK_MOD)) v = (int)v % (int)expr_F();
	}
	printd_out("expr_T -> " VAL_FMT, v);
	return v;
}


/* F --> P ["^" F] */

static val expr_F()
{
	printd_in("expr_F");
	val v = expr_P();
	printd("expr_F next");
	if(next_is(TOK_POW)) v = pow(v, expr_F());
	printd_out("expr_F -> " VAL_FMT, v);
	return v;
}


/* P --> v | "(" E ")" | "-" T | "+" T */

static val expr_P()
{
	val v = 0;
	printd_in("expr_P");
	if(tok == TOK_NUMBER) {
		v = cur_num;
		next();
	} else if(tok == TOK_NAME) {
		if(!cur_var) error("no cur var");
		v = cur_var->v;
		printd("get from %s = " VAL_FMT, cur_var->name, v);
		next();
	} else if(next_is(TOK_OPEN)) {
		v = expr();
		expect(TOK_CLOSE);
		printd("After open");
	} else if(next_is(TOK_MINUS)) {
		v = - expr_T();
	} else if(next_is(TOK_PLUS)) {
		v = expr_T();
	} else {
		v = statement();
	}
	printd_out("expr_P -> " VAL_FMT, v);
	return v;
}
	

int find_next_line(int idx)
{
	for(idx++;idx<256; idx++) {
		if(lines[idx].buf[0] != TOK_EOF) {
			return idx;
		}
	}
	return 0;
}


static void run_line(int n)
{
	printd(">>> %d", n);
	cur_line = n;
	pc = lines[cur_line].buf;
	line();
}


static val statement()
{
	printd("statement");
	if(tok == TOK_NAME) {
		fn_assign();
	} 
	else if(next_is(TOK_CLS))    return fn_cls();
        else if(next_is(TOK_END))    return fn_end();
	else if(next_is(TOK_FOR))    return fn_for();
        else if(next_is(TOK_NEXT))   return fn_next();
        else if(next_is(TOK_GOSUB))  return fn_gosub();
        else if(next_is(TOK_GOTO))   return fn_goto();
        else if(next_is(TOK_IF))     return fn_if();
        else if(next_is(TOK_LIST))   return fn_list();
        else if(next_is(TOK_PLOT))   return fn_plot();
        else if(next_is(TOK_PRINT))  return fn_print();
        else if(next_is(TOK_QUIT))   return fn_quit();
        else if(next_is(TOK_REM))    return fn_rem();
        else if(next_is(TOK_RETURN)) return fn_return();
        else if(next_is(TOK_RND))    return fn_rnd();
        else if(next_is(TOK_RUN))    return fn_run();
        else if(next_is(TOK_SQRT))   return fn_sqrt();
        else if(next_is(TOK_SLEEP))  return fn_sleep();

	return 0;
}


static void compile(void)
{
	int n = cur_num;
	if(n < 0 || n > 255) error("line number out of range");
	printd("%d ***", n);

	struct line *l = &lines[n];
	uint8_t *p = lines[n].buf;
	uint8_t *pe = p + sizeof(lines[n]);

	do {
		next();
		printd("> %3d: %s", p-lines[n].buf, tokname);
		if(pe-p < 1) goto too_long;
		*p++ = tok;
		if(tok == TOK_NUMBER) {
			if(pe-p < sizeof(cur_num)) goto too_long;
			memcpy(p, &cur_num, sizeof(cur_num));
			p += sizeof(cur_num);
		} else if(tok == TOK_NAME) {
			if(pe-p < 1) goto too_long;
			int i = cur_var - var_list;
			*p++ += i;
		} else if(tok == TOK_STRING) {
			size_t l = toklen-2;
			if(pe-p < l+1) goto too_long;
			*p++ = l;
			char *str = (char *)p;
			memcpy(p, ts+1, l);
			p += l;
			*p = '\0';
			p ++;
		}
	} while(tok != TOK_EOF);
	return;

too_long:
	error("line %d too long", n);
}


static void line(void)
{
	next();

	if(tok == TOK_NUMBER) {
		if(!running) {
			compile();
		}
	} else {
		for(;;) {
			statement();
			if(tok != TOK_COLON) break;
			next();
		}
		expect(TOK_EOF);
	}
}


void on_sig(int a)
{
	printf("\nBREAK");
	running = false;
}


/*
 * Statement handlers
 */

static val fn_run()
{
	int l = find_next_line(0);
	running = true;
	while(running && l != 0) {
		run_line(l);
		l = find_next_line(cur_line);
	}
	return 0;
}


static val fn_if()
{
	val v = expr();

	expect(TOK_THEN);

	printd("if expr = " VAL_FMT, v);

	if(v) {
		statement();
	} else {
		do {
			next();
		} while(tok != TOK_EOF && tok != TOK_ELSE && tok != TOK_COLON);

		if(next_is(TOK_ELSE)) {
			statement();
		}
	}
	return 0;
}


static val fn_print(void)
{
	do {
		if(tok == TOK_STRING) {
			fwrite(ts+1, toklen-2, 1, stdout);
			next();
		} else {
			val v = expr();
			printf(VAL_FMT, v);
		}
	} while(next_is(TOK_COMMA));
	printf("\n");
	return 0;
}


static val fn_assign(void)
{
	struct var *var = cur_var;
	next();
	printd("assign to %s", var->name);
	expect(TOK_ASSIGN);
	var->v = expr();
	return var->v;
}


static val fn_goto(void)
{
	int l = expr();
	run_line(l);
	return 0;
}


static val fn_gosub()
{
	if(call_head < CALL_STACK_SIZE-1) {
		call_stack[call_head++] = cur_line;
		int l = expr();
		run_line(l);
	}
	return 0;
}


static val fn_rem(void)
{
	while(tok != TOK_EOF) next();
	return 0;
}


static val fn_return(void)
{
	if(call_head > 0) {
		cur_line = call_stack[--call_head];
	}
	return 0;
}


static val fn_list(void)
{
	int i;
	for(i=0; i<256; i++) {
		struct line *line = &lines[i];
		uint8_t *p = line->buf;
		if(*p != TOK_EOF) {
			printf("%d ", i);
			while(*p != TOK_EOF) {
				if(*p == TOK_NUMBER) {
					val v = *(float *)(p+1);
					printf(VAL_FMT " ", v);
					p += sizeof(float);
				} else if(*p == TOK_NAME) {
					printf("%s ", var_list[*(++p)].name);
				} else if(*p == TOK_STRING) {
					size_t len = *++p;
					printf("\"%s\" ", p);
					p += len + 1;
				} else {
					printf("%s ", tokens[*p]);
				}
				p++;
			}
			printf("\n");
		}
	}
	return 0;
}


static val fn_plot(void)
{
	static int colorcode[] = { 30, 34, 32, 36, 31, 35, 33, 37 };

	int x = expr();
	expect(TOK_COMMA);
	int y = expr();
	int color = 0;
	struct var *var = var_find("color", 5);
	if(var) color = (int)var->v % 16;
	
	printf("\e[s\e[%d;%dH", y, x*2);
	printf("\e[%d;%d;7m  \e[0m\e[u", color >= 8, colorcode[color % 8]);

	fflush(stdout);
	next();
	return 0;
}


static val fn_cls(void)
{
	printf("\e[2J\e[H");
	fflush(stdout);
	return 0;
}


static val fn_end(void)
{
	running = false;
	return 0;
}


static val fn_for(void)
{
	if(loop_head >= LOOP_STACK_SIZE) error("loop stack overflow");
	if(cur_line == 0) error("not running");

	if(tok != TOK_NAME) error("expected NAME");

	struct loop *loop = &loop_stack[loop_head++];
	loop->line = find_next_line(cur_line);
	loop->var = cur_var;

	next();
	expect(TOK_ASSIGN);

	loop->var->v = expr();

	expect(TOK_TO);
	loop->v_end = expr();
	if(next_is(TOK_STEP)) {
		loop->v_step = expr();
	} else {
		loop->v_step = 1;
	}
	
	printd("for on %s: " VAL_FMT " step " VAL_FMT, loop->var->name, loop->var->v, loop->v_step);

	return 0;
}


static val fn_next(void)
{
	if(loop_head == 0) {
		error("next without for");
	}
	
	struct loop *loop = &loop_stack[loop_head-1];

	if(tok == TOK_NAME) {
		if(cur_var != loop->var) error("for/next nesting mismatch");
		next();
	}
	

	struct var *var = loop->var;
	printd("next on %s: " VAL_FMT, var->name, var->v);
	var->v += loop->v_step;


	if((loop->v_step > 0 && var->v <= loop->v_end) ||
	   (loop->v_step < 0 && var->v >= loop->v_end)) {
		run_line(loop->line);
	} else {
		loop_head --;
		loop_stack[loop_head].var = NULL;
	}

	return 0;
}


static val fn_quit(void)
{
	exit(0);
}


static val fn_sleep(void)
{
	usleep(expr() * 1.0e6);
	return 0;
}


static val fn_rnd(void)
{
	return rand();
}


static val fn_sqrt(void)
{
	return sqrt(expr());
}


/*
 * Main: read lines and feed to line() function
 */

int main(int argc, char **argv)
{
	static char inp[120];
	srand(time(NULL));

	//signal(SIGINT, on_sig);

	while(fgets(inp, sizeof(inp), stdin) != NULL) {

		if(setjmp(jmpbuf) == 0) {
			cur_line = 0;
			p = inp;
			pe = p + strlen(inp);
			line();
		} else {
			running = false;
		};
	}
	
	return 0;
}

/* 
 * vi: ft=c
 */
