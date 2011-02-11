int main() {
  int a = 4 + 5;

  int b = 
  /* Nested /* comments */ 12; // are handled correctly

  int c =
#ifdef __APPLE__
    12
#else
    18
#endif
    ;
}
