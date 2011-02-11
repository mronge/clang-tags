#include <stdio.h>

extern int bla;
/*extern int bla2 = 4;*/ // invalid

#ifdef TEST
int test = 8;
#else
int test2 = 12;
#endif

#define DOIT staticvar = 199; \
             anothervar = 89898

static int staticvar = 88;

int a;
int a = 3;

struct Test {
  int a, b;
} test2;  // gives an error if TEST is not defined

int anothervar;

int b = 4, c = 5;

typedef unsigned int u32;
typedef int (*funcpointertype)(int, int);
typedef int functype(int, int);

int (*funcp)(int, int);

__typeof(funcp) fp2;
funcpointertype fp3;

functype f;
__typeof(f) f2;

int main()
{
  int a = 4;

  int b = a;

  test2.b = 80;

  staticvar = 88;

  DOIT;
}

/*int test() = main;*/  // illegal

struct s {} t;
