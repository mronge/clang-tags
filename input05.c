#include <stdio.h>

extern int bla;
/*extern int bla2 = 4;*/ // invalid

#ifdef TEST
int test = 8;
#else
int test2 = 12;
#endif

static int staticvar = 88;

int a;
int a = 3;

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
}

/*int test() = main;*/  // illegal

struct s {} t;
