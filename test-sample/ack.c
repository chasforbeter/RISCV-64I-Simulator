#include "mylib.h"

int ack(int m, int n)
{
  if (m == 0)
    return n + 1;
  else if (n == 0)
    return ack(m - 1, 1);
  else
    return ack(m - 1, ack(m, n - 1));
}

int main()
{
  for (int i = 0; i <= 3; ++i)
    for (int j = 0; j <= 3; ++j)
    {
      int res = ack(i, j);
      print_d(i);
      print_c(',');
      print_d(j);
      print_c(' ');
      print_c('i');
      print_c('s');
      print_c(' ');
      print_d(res);
      print_c('\n');
    }
  return 0;
}