#include <stdio.h>

#define EXTERN extern
#define INIT(a) 

#include "input07.h"

static int blah = 8;
int blubb = 90;

int g()
{
  int s = static_h;
  int t = static_h_2;
  int u = bla;
  int v = blah;
  int w = blubb;

  printf("%d", 0?(1?bla:blah):1);
}

int main()
{
}
