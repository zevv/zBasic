/*
 * expr-list := (string | expression) (, (string | expression) )*
 * var-list := var (, var)*
 * var := A | B | C ..., | Y | Z
 * relop := < (> | = | e) | > (< | = | e) | =
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>


enum tok {
	TOK_NONE,
	TOK_NUMBER,
	TOK_OPEN,
	TOK_CLOSE,
	TOK_PLUS,
	TOK_MINUS,
	TOK_MUL,
	TOK_DIV,
	TOK_EOF,
};


const char *tokname[] = {
	"NONE",
	"NUMBER",
	"OPEN",
	"CLOSE",
	"PLUS",
	"MINUS",
	"MUL",
	"DIV",
	"EOF",
};

enum state {
	STATE_INIT,
	STATE_NUMBER,
};

static size_t len;
static int cur = 0;
static int cap_num = 0;
enum tok tok;

void next(void)
{
	len = 0;

	tok = TOK_NONE;
	cap_num = 0;

	int state = STATE_INIT;

	if(cur == 0) cur = getchar();

	while(tok == TOK_NONE) {

		if(cur == EOF) {
			tok = TOK_EOF;
		}

		if(state == STATE_INIT) {
			if(isdigit(cur)) {
				cap_num = cur - '0';
				state = STATE_NUMBER;
			}
			if(cur == '(') tok = TOK_OPEN;
			if(cur == ')') tok = TOK_CLOSE;
			if(cur == '+') tok = TOK_PLUS;
			if(cur == '-') tok = TOK_MINUS;
			if(cur == '*') tok = TOK_MUL;
			if(cur == '/') tok = TOK_DIV;
		}
	
		else if(state == STATE_NUMBER) {
			if(isdigit(cur)) {
				cap_num = cap_num * 10 + cur - '0';
			} else {
				tok = TOK_NUMBER;
				break;
			}
		}

		cur = getchar();
	}

	printf("*** tok %s (%d)\n", tokname[tok], cap_num);
}



void expect(bool ok)
{
	if(!ok ) {
		fprintf(stderr, "syntax error");
		exit(1);
	}
}


/* number := digit digit */

int num()
{
	expect(tok == TOK_NUMBER);
	printf("num -> %d\n", cap_num);
	return cap_num;
}

	

int factor();

/* 
 * term := factor ((* | /) factor)* 
 */

int term()
{
	int v = factor();

	while(tok == TOK_MUL || tok == TOK_DIV) {
		if(tok == TOK_MUL) { next(); v *= factor(); }
		if(tok == TOK_DIV) { next(); v /= factor(); }
	}

	printf("term -> %d\n", v);

	return v;
}


/* 
 * expression := (+ | - | e) term ((+ | -) term)*
 */

int expr()
{
	int v = term();

	while(tok == TOK_PLUS || tok == TOK_MINUS) {
		if(tok == TOK_PLUS)  { next(); v += term(); }
		if(tok == TOK_MINUS) { next(); v -= term(); }
	}

	printf("expr -> %d\n", v);

	return v;
}


/* 
 * factor := var | number | (expression) 
 */

int factor()
{
	int v = 0;

	expect(tok == TOK_NUMBER || tok == TOK_OPEN);

	if(tok == TOK_NUMBER) {
		v = num();
		next();
	}

	if(tok == TOK_OPEN) {
		next();
		v = expr();
		expect(tok == TOK_CLOSE);
	}

	printf("factor -> %d\n", v);

	return v;
}



int main(int argc, char **argv)
{
	if(0) {
		next();
		while(tok != TOK_EOF) {
			printf("%d (%d)\n", tok, cap_num);
			next();
		}
	} else {
		next();
		printf("%d\n", expr());
	}
	return 0;
}

/*
 * End
 */
