/*
 * expr-list := (string | expression) (, (string | expression) )*
 * var-list := var (, var)*
 * var := A | B | C ..., | Y | Z
 * relop := < (> | = | e) | > (< | = | e) | =
 */


#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>


enum state {
	STATE_INIT,
	STATE_NUMBER,
	STATE_ALPHA,
	STATE_KEYWORD,
	STATE_STRING,
};


enum tok {
	TOK_NONE,
	TOK_NUMBER,
	TOK_PLUS,
	TOK_MINUS,
	TOK_MUL,
	TOK_MOD,
	TOK_DIV,
	TOK_EQUALS,
	TOK_VAR,
	TOK_PRINT,
	TOK_RUN,
	TOK_GOTO,
	TOK_IF,
	TOK_THEN,
	TOK_LT,
	TOK_GT,
	TOK_COMMA,
	TOK_GOSUB,
	TOK_LIST,
	TOK_RETURN,
	TOK_END,
	TOK_STRING,
	TOK_EOL,
	TOK_EOF,
};


const char *tokname[] = {
	"NONE",
	"NUMBER",
	"PLUS",
	"MINUS",
	"MUL",
	"MOD",
	"DIV",
	"EQUALS",
	"VAR",
	"PRINT",
	"RUN",
	"GOTO",
	"IF",
	"THEN",
	"LT",
	"GT",
	"COMMA",
	"GOSUB",
	"LIST",
	"RETURN",
	"END",
	"STRING",
	"EOL",
	"EOF",
};

#define NUM_TOKS (sizeof(tokname) / sizeof(tokname[0]))
#define STACK_SIZE 16

typedef double var;

static int cur = 0;
static int save = 0;
static var cap_num = 0;
static char cap_str[32];
static size_t len_str;
static enum tok tok;
static int stack[STACK_SIZE];
static int head = 0;
static var vars[26];
static char lines[256][80];
static int (*reader)(void) = getchar;
static bool running = false;

static size_t cur_line = 1;
static size_t cur_pos = 0;


static void error(const char *fmt, ...)
{
	va_list va;
	printf("\e[31;1m");
	va_start(va, fmt);
	vprintf(fmt, va);
	va_end(va);
	printf("\e[0m");
	exit(1);
}


static void printd(const char *fmt, ...)
{
	//return;
	va_list va;
	printf("\e[30;1m");
	va_start(va, fmt);
	vprintf(fmt, va);
	va_end(va);
	printf("\e[0m");
}


static int getchar_prog(void)
{
	//printd("line %d pos %d = '%c'\n", cur_line, cur_pos, lines[cur_line][cur_pos]);
	char c = lines[cur_line][cur_pos++];
	//printd("get '%c'\n", c);
	return (c == 0) ? '\n' : c;
}


static void cap_char()
{
	if(len_str < sizeof(cap_str)-1) cap_str[len_str++] = cur;
	cap_str[len_str] = '\0';
}


static enum tok find_tok(const char *s)
{
	size_t i;
	for(i=0; i<NUM_TOKS; i++) {
		if(strcasecmp(s, tokname[i]) == 0) {
			return i;
		}
	}
	fprintf(stderr, "unknown keyword '%s'\n", s);
	exit(1);
}


void next(void)
{
	int state = STATE_INIT;
	tok = TOK_NONE;

	//printd("next, save = '%c'\n", save);

	cur = save ? save : reader();
	save = '\0';

	while(tok == TOK_NONE) {
		
		//printd("'%c' -> %d\n", cur, state);

		if(cur == EOF) {
			tok = TOK_EOF;
		}

		if(state == STATE_INIT) {
			if(isdigit(cur)) {
				len_str = 0;
				cap_char();
				state = STATE_NUMBER;
			}
			if(isalpha(cur)) {
				len_str = 0;
				cap_char();
				state = STATE_ALPHA;
			}
			if(cur == '"') {
				len_str = 0;
				state = STATE_STRING;
			}
			if(cur == '+') tok = TOK_PLUS;
			if(cur == '-') tok = TOK_MINUS;
			if(cur == '*') tok = TOK_MUL;
			if(cur == '%') tok = TOK_MOD;
			if(cur == '/') tok = TOK_DIV;
			if(cur == '=') tok = TOK_EQUALS;
			if(cur == '<') tok = TOK_LT;
			if(cur == '>') tok = TOK_GT;
			if(cur == ',') tok = TOK_COMMA;
			if(cur == '\n') tok = TOK_EOL;
		}
	
		else if(state == STATE_NUMBER) {
			if(isdigit(cur) || cur == '.') {
				cap_char();
			} else {
				cap_num = atof(cap_str);
				tok = TOK_NUMBER;
				save = cur;
				break;
			}
		}

		else if(state == STATE_ALPHA) {
			if(isalpha(cur)) {
				cap_char();
				state = STATE_KEYWORD;
			}
			if(!isalpha(cur)) {
				tok = TOK_VAR;
				save = cur;
				break;
			}
		}

		else if(state == STATE_KEYWORD) {
			if(isalpha(cur)) {
				cap_char();
			} else {
				tok = find_tok(cap_str);
				save = cur;
				break;
			}
		}

		else if(state == STATE_STRING) {
			if(cur == '"') {
				tok = TOK_STRING;
			} else {
				cap_char();
			}
		}

		//printd("'%c' -> %d\n", cur, state);
	
		if(tok == TOK_NONE) {
			cur = reader();
		}
	}

	printd("*** tok %s (%.14g)\n", tokname[tok], cap_num);
}



void expect(enum tok t)
{
	if(tok != t) {
		error("Expected %s\n", tokname[t]);
		exit(1);
	}
}


/* number := digit digit */

var num()
{
	expect(TOK_NUMBER);
	printd("num -> %.14g\n", cap_num);
	return cap_num;
}

	

var factor();

/* 
 * term := factor ((* | /) factor)* 
 */

var term()
{
	var v = factor();

	if(tok == TOK_MUL) {
		next();
		v = v * factor();
	}
	else if(tok == TOK_DIV) {
		next();
		v = v / factor();
	}
	else if(tok == TOK_MOD) {
		next();
		v = (int)v % (int)factor();
	}

	printd("term -> %.14g\n", v);

	return v;
}


/* 
 * expression := (+ | - | e) term ((+ | -) term)*
 */

var expr()
{
	var v = term();

	while(tok == TOK_PLUS || tok == TOK_MINUS) {
		if(tok == TOK_PLUS)  { next(); v += term(); }
		if(tok == TOK_MINUS) { next(); v -= term(); }
	}

	printd("expr -> %.14g\n", v);

	return v;
}


/* 
 * factor := var | number | (expression) 
 */

var factor()
{
	var v = 0;

	if(tok == TOK_NUMBER) {
		v = num();
		next();
	}

	else if(tok == TOK_VAR) {
		int idx = toupper(cap_str[0]) - 'A';
		next();
		v = vars[idx];
	}

	printd("factor -> %.14g\n", v);

	return v;
}

void statement();


int find_next_line(int idx)
{
	for(;idx<256; idx++) {
		if(lines[idx][0]) {
			return idx;
		}
	}
	return 0;
}


void jump_to(int linenum)
{
	printd(">> %d: %s\n", linenum, lines[linenum]);
	cur = '\0';
	if(lines[linenum][0]) {
		cur_line = linenum;
		cur_pos = 0;
		next();
		statement();
	}
}


void run()
{
	next();
	printd("run in\n");
	int l = find_next_line(0);
	running = true;
	while(running && l != 0) {
		reader = getchar_prog;
		jump_to(l);
		l = find_next_line(cur_line+1);
	}
	cur = '\0';
	printd("run out\n");
	reader = getchar;
}


void _if()
{
	next();
	var v1 = expr();
	enum tok t = tok;
	next();
	var v2 = expr();
	expect(TOK_THEN);
	next();

	printd("if %.14g %s %14g\n", v1, tokname[t], v2);

	if( (t == TOK_EQUALS && v1 == v2) ||
	    (t == TOK_GT     && v1 >  v2) ||
	    (t == TOK_LT     && v1 <  v2) ) {
		statement();
	} else {
		do {
			next();
		} while(tok != TOK_EOL);
	}
}


void print(void)
{
	printd("print\n");
	do {
		next();
		if(tok == TOK_STRING) {
			printf("%s", cap_str);
			next();
		} else {
			var v = expr();
			printf("%.14g", v);
		}
	} while(tok == TOK_COMMA);
	printf("\n");
}

void statement()
{

	if(tok == TOK_PRINT) {
		print();
	} 

	else if(tok == TOK_VAR) {
		int idx = toupper(cap_str[0]) - 'A';
		next();
		expect(TOK_EQUALS);
		next();
		vars[idx] = expr();
	}

	else if(tok == TOK_RUN) {
		run();
	} 

	else if(tok == TOK_GOTO) {
		next();
		int l = expr();
		jump_to(l);
	}

	else if(tok == TOK_IF) {
		_if();
	}

	else if(tok == TOK_GOSUB) {
		next();
		if(head < STACK_SIZE-1) {
			stack[head++] = cur_line;
			int l = expr();
			jump_to(l);
		}
	}
	
	else if(tok == TOK_RETURN) {
		if(head > 0) {
			cur_line = stack[--head];
		}
	}

	else if(tok == TOK_END) {
		running = false;
	}
	
	else if(tok == TOK_LIST) {
		int i;
		for(i=0; i<256; i++) {
			if(lines[i][0]) {
				printf("%d %s\n", i, lines[i]);
			}
		}
	}
}


void line()
{
	fflush(stdout);

	if(tok == TOK_NUMBER) {
		int v = cap_num;
		if(v < 1 || v > 255) {
			fprintf(stderr, "Line number out of range\n");
			exit(1);
		}
		size_t n = 0;
		lines[v][n] = '\0';
		char c = reader();
		while(c != '\n') {
			lines[v][n++] = c;
			lines[v][n] = '\0';
			c = reader();
		}
		printd("line %d: %s\n", v, lines[v]);
	} else {
		statement();
	}

	next();
}


int main(int argc, char **argv)
{
	if(0) {
		next();
		while(tok != TOK_EOF) {
			printd("%d (%d)\n", tok, cap_num);
			next();
		}
	} else {
		next();
		while(tok != TOK_EOF) {
			line();
		}
	}
	return 0;
}

/*
 * End
 */
