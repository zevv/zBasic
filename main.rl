
/*
 * http://www.engr.mun.ca/~theo/Misc/exp_parsing.htm#classic
 */

#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <setjmp.h>
#include <stdarg.h>
#include <math.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>

//#define DEBUG

%% machine basic;
%% write data;

enum tok {
	TOK_AND, TOK_ASSIGN, TOK_CLS, TOK_CLOSE, TOK_COLON, TOK_COMMA, TOK_DIV,
	TOK_ELSE, TOK_END, TOK_EOF, TOK_EQ, TOK_FOR, TOK_GE, TOK_GOSUB,
	TOK_GOTO, TOK_GT, TOK_IF, TOK_LE, TOK_LIST, TOK_LT, TOK_MINUS, TOK_MOD,
	TOK_MUL, TOK_NE, TOK_NEXT, TOK_NONE, TOK_NUMBER, TOK_OPEN, TOK_OR,
	TOK_PLOT, TOK_PLUS, TOK_POW, TOK_PRINT, TOK_QUIT, TOK_REM, TOK_RETURN,
	TOK_RND, TOK_RUN, TOK_SQRT, TOK_SLEEP, TOK_STEP, TOK_STRING, TOK_THEN, TOK_TO,
	TOK_NAME,
};

typedef double val;


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

struct token {
	const char *name;
	val (*fn)(void);
};

const struct token tokens[] = {

        /* Generic tokens */

        [TOK_NAME] =    { "NAME" },
        [TOK_NONE] =    { "NONE" },
        [TOK_STRING] =  { "STRING" },

        /* Expression tokens */

        [TOK_AND] =     { "AND" },
        [TOK_ASSIGN] =  { "ASSIGN" },
        [TOK_CLOSE] =   { "CLOSE" },
        [TOK_COLON] =   { "COLON" },
        [TOK_COMMA] =   { "COMMA" },
        [TOK_DIV] =     { "DIV" },
        [TOK_EOF] =     { "EOF" },
        [TOK_EQ] =      { "EQ" },
        [TOK_GE] =      { "GE" },
        [TOK_GT] =      { "GT" },
        [TOK_LE] =      { "LE" },
        [TOK_LT] =      { "LT" },
        [TOK_MINUS] =   { "MINUS" },
        [TOK_MOD] =     { "MOD" },
        [TOK_MUL] =     { "MUL" },
        [TOK_NE] =      { "NE" },
        [TOK_NUMBER] =  { "NUMBER" },
        [TOK_OPEN] =    { "OPEN" },
        [TOK_OR] =      { "OR" },
        [TOK_PLUS] =    { "PLUS" },
        [TOK_POW] =     { "POW" },

        /* Statements and functions */

        [TOK_CLS] =     { "CLS", fn_cls },
        [TOK_END] =     { "END", fn_end },
	[TOK_FOR] =     { "FOR", fn_for },
	  [TOK_TO] =    { "TO" },
	  [TOK_STEP] =  { "STEP" },
        [TOK_NEXT] =    { "NEXT", fn_next },
        [TOK_GOSUB] =   { "GOSUB", fn_gosub },
        [TOK_GOTO] =    { "GOTO", fn_goto },
        [TOK_IF] =      { "IF", fn_if },
          [TOK_THEN] =  { "THEN" },
          [TOK_ELSE] =  { "ELSE" },
        [TOK_LIST] =    { "LIST", fn_list },
        [TOK_PLOT] =    { "PLOT", fn_plot },
        [TOK_PRINT] =   { "PRINT", fn_print },
        [TOK_QUIT] =    { "QUIT", fn_quit },
        [TOK_REM] =     { "REM", fn_rem },
        [TOK_RETURN] =  { "RETURN", fn_return },
        [TOK_RND] =     { "RND", fn_rnd },
        [TOK_RUN] =     { "RUN", fn_run },
        [TOK_SQRT] =    { "SQRT", fn_sqrt },
        [TOK_SLEEP] =   { "SLEEP", fn_sleep },
};

#define NUM_TOKS (sizeof(tokens) / sizeof(tokens[0]))



/* Parser state */

static char *buf;
static enum tok tok;
static const char *tokname = "";
static size_t toklen;
static int cs;
static val cap_num;
static char *ts, *te;
static char *p, *pe;
static char *eof = NULL;
static int act;


/* Interpreter state */

#define CALL_STACK_SIZE 16
#define LOOP_STACK_SIZE 8
#define MAX_VARS 32
#define MAX_VAR_LEN 5

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

static jmp_buf jmpbuf;
static bool running = false;
static struct var var_list[MAX_VARS];
static char lines[256][80];
static int call_stack[CALL_STACK_SIZE];
static int call_head = 0;
static struct loop loop_stack[LOOP_STACK_SIZE];
static int loop_head = 0;
static int cur_line;
static void statement();
static void line(char *b);
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
	printf(" / %-10s", tokname);
	if(te > ts) {
		printf(" / '");
		fwrite(ts, te-ts, 1, stdout);
		printf("'");
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


static void next(void)
{
	tok = TOK_NONE;
	cap_num = 0.;

	%%{
		number = digit+ ( '.' digit+ )? ( [eE] ('+'|'-')? digit+ )?;
		ws = [ \t\r\n]+;
		string = '"' ([^"\\] | '\\"' | '\\\\') +  '"';

		main := |*
			ws        => {};
			number    => { tok = TOK_NUMBER;  fbreak; };
			'('       => { tok = TOK_OPEN;    fbreak; };
			')'       => { tok = TOK_CLOSE;   fbreak; };
			'+'       => { tok = TOK_PLUS;    fbreak; };
			'-'       => { tok = TOK_MINUS;   fbreak; };
			'%'       => { tok = TOK_MOD;     fbreak; };
			':'       => { tok = TOK_COLON;   fbreak; };
			'/'       => { tok = TOK_DIV;     fbreak; };
			'^'       => { tok = TOK_POW;     fbreak; };
			'*'       => { tok = TOK_MUL;     fbreak; };
			'='       => { tok = TOK_ASSIGN;  fbreak; };
			'<'       => { tok = TOK_LT;      fbreak; };
			'<='      => { tok = TOK_LE;      fbreak; };
			'=='      => { tok = TOK_EQ;      fbreak; };
			'#'       => { tok = TOK_NE;      fbreak; };
			'!='      => { tok = TOK_NE;      fbreak; };
			'<>'      => { tok = TOK_NE;      fbreak; };
			'>='      => { tok = TOK_GE;      fbreak; };
			'>'       => { tok = TOK_GT;      fbreak; };
			','       => { tok = TOK_COMMA;   fbreak; };
			'?'       => { tok = TOK_PRINT;   fbreak; };
			string    => { tok = TOK_STRING;  fbreak; };
			alpha+    => { tok = TOK_NAME;    fbreak; };
		*|;
	}%%

	while(tok == TOK_NONE) {

		if(cs >= basic_first_final) {
			tok = TOK_EOF;
		}

		if(cs == basic_error) {
			error("parsing error");
		}

		%% write exec;
	}

	toklen = te-ts;

	if(tok == TOK_NUMBER) {
		cap_num = atof(ts);
	}

	if(tok == TOK_NAME) {
		size_t i;
		for(i=0; i<NUM_TOKS; i++) {
			if(strlen(tokens[i].name) == toklen &&
			   strncasecmp(tokens[i].name, ts, toklen) == 0) {
				tok = i;
				break;
			}
		}
	}
	
	tokname = tokens[tok].name;
}


static struct var *var_find(bool create, const char *name, size_t len)
{
	printd("var find");

	if(len > MAX_VAR_LEN) {
		error("oversized var name");
	}

	int i;
	struct var *v;
	struct var *vfree = NULL;
	for(i=0; i<MAX_VARS; i++) {
		struct var *v = &var_list[i];
		if(len == strlen(v->name) &&
			  strncmp(v->name, name, len) == 0) {
			return v;
		}
		if(vfree == NULL && v->name[0] == '\0') {
			vfree = v;
		}
	}

	if(!create || !vfree) {
		return NULL;
	}

	strncpy(vfree->name, name, len);
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
	printd("expect %s", tokens[t].name);
	if(!next_is(t)) {
		error("expected %s", tokens[t].name);
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


/* 
 *  L --> C {( "and" | "or" ) C}
 */

static val expr_L()
{
	printd_in("expr_L");
	val v = expr_C();
	while(tok == TOK_AND || tok == TOK_OR) {
		printd("expr_L next");
		if(next_is(TOK_AND)) v = expr_C() && v;
		if(next_is(TOK_OR))  v = expr_C() || v;
	}
	printd_out("expr_L -> %.14g", v);
	return v;
}

/* 
 *  C --> E {( "<=" | "<" | "==" | "!=" | ">=" | ">") E}
 */

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
	printd_out("expr -> %.14g", v);
	return v;
}


/* 
 *  E --> T {( "+" | "-" ) T}
 */

static val expr_E()
{
	printd_in("expr_E");
	val v = expr_T();
	while(tok == TOK_PLUS || tok == TOK_MINUS) {
		printd("expr_E next");
		if(next_is(TOK_PLUS))  v += expr_T();
		if(next_is(TOK_MINUS)) v -= expr_T();
	}
	printd_out("expr_E -> %.14g", v);
	return v;
}


/* 
 * T --> F {( "*" | "/" | "%") F}
 */

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
	printd_out("expr_T -> %.14g", v);
	return v;
}


/* 
 * F --> P ["^" F]
 */

static val expr_F()
{
	printd_in("expr_F");
	val v = expr_P();
	printd("expr_F next");
	if(next_is(TOK_POW)) v = pow(v, expr_F());
	printd_out("expr_F -> %.14g", v);
	return v;
}


/* P --> v | "(" E ")" | "-" T */

static val expr_P()
{
	val v = 0;
	printd_in("expr_P");
	if(tok == TOK_NUMBER) {
		v = cap_num;
		next();
	} else if(tok == TOK_NAME) {
		struct var *var = var_find(false, ts, toklen);
		if(var == NULL) error("unknown var");
		next();
		v = var->v;
	} else if(next_is(TOK_OPEN)) {
		v = expr();
		expect(TOK_CLOSE);
		printd("After open");
	} else if(next_is(TOK_MINUS)) {
		v = - expr_T();
	} else {
		val (*fn)(void) = tokens[tok].fn;
		if(fn == NULL) error("syntax error");
		next();
		expect(TOK_OPEN);
		v =  fn();
		expect(TOK_CLOSE);
	}
	printd_out("expr_P -> %.14g", v);
	return v;
}
	

int find_next_line(int idx)
{
	for(idx++;idx<256; idx++) {
		if(lines[idx][0]) {
			return idx;
		}
	}
	return 0;
}


static void jump_to(int linenum)
{
	cur_line = linenum;
	printd(">> %d: %s", linenum, lines[linenum]);
	line(lines[linenum]);
}



static void statement()
{
	printd("statement");
	if(tok == TOK_NAME) {
		fn_assign();
	} else {
		val (*fn)(void) = tokens[tok].fn;
		if(fn) {
			next();
			(void)fn();
		}
	}
}


static void line(char *b)
{
	%% write init;
	
	p = b;
	pe = p + strlen(p);
		
	next();

	if(tok == TOK_NUMBER) {
		int v = cap_num;
		if(v < 0 || v > 255) error("line number out of range");
		size_t n = 0;
		strncpy(lines[v], te, sizeof(lines[v]));
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
		jump_to(l);
		l = find_next_line(cur_line);
	}
	return 0;
}


static val fn_if()
{
	val v = expr();

	expect(TOK_THEN);

	printd("if expr = %.14g", v);

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
			printf("%.14g", v);
		}
	} while(next_is(TOK_COMMA));
	printf("\n");
	return 0;
}


static val fn_assign(void)
{
	struct var *var = var_find(true, ts, toklen);
	if(var == NULL) error("no room for var");
	next();
	expect(TOK_ASSIGN);
	var->v = expr();
	return var->v;
}


static val fn_goto(void)
{
	int l = expr();
	jump_to(l);
	return 0;
}


static val fn_gosub()
{
	if(call_head < CALL_STACK_SIZE-1) {
		call_stack[call_head++] = cur_line;
		int l = expr();
		jump_to(l);
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
		if(lines[i][0]) {
			printf("%d %s", i, lines[i]);
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
	struct var *var = var_find(false, "color", 5);
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

	struct var *var = var_find(true, ts, toklen);
	struct loop *loop = &loop_stack[loop_head++];
	loop->line = find_next_line(cur_line);
	loop->var = var;

	next();
	expect(TOK_ASSIGN);

	var->v = expr();
	expect(TOK_TO);
	loop->v_end = expr();
	if(next_is(TOK_STEP)) {
		loop->v_step = expr();
	} else {
		loop->v_step = 1;
	}

	return 0;
}


static val fn_next(void)
{
	if(loop_head == 0) {
		error("next without for");
	}
	
	struct loop *loop = &loop_stack[loop_head-1];

	if(tok == TOK_NAME) {
		struct var *var = var_find(false, ts, toklen);
		if(var != loop->var) error("for/next nesting mismatch");
		next();
	}

	struct var *var = loop->var;
	var->v += loop->v_step;
	if((loop->v_step > 0 && var->v <= loop->v_end) ||
	   (loop->v_step < 0 && var->v >= loop->v_end)) {
		jump_to(loop->line);
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
	static char inp[80];
	srand(time(NULL));

	//signal(SIGINT, on_sig);

	while(fgets(inp, sizeof(inp), stdin) != NULL) {

		if(setjmp(jmpbuf) == 0) {
			cur_line = 0;
			line(inp);
		} else {
			running = false;
		};
	}
	
	return 0;
}

/* 
 * vi: ft=c
 */
