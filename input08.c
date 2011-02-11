#define A(x) int x;
#define B(x) A(x ## 1) A(x ## 2)

A(g)

#line 100000

  B(g)

int f;

int main()
{
}

#include "input08.h"

A_h(bla)
    B_h(bla)
 
tA_h(int)


// test chain of macros

#define C0(x) x
#define C1(x) C0(x)
#define C2(x) C1(x)
#define C3(x) C2(x)
#define C4(x) C3(x)
#define C5(x) C4(x)

        C5(int woosh;)

#define D0(x) x 
#define D1(x) D0(x)
#define D2(x) D1(x)
#define D3(x) D2(x) whoosh;
#define D4(x) D3(x)
#define D5(x) D4(x)

D5(int)

#define CONC(a, b) a ## b

int CONC(hey, there);
