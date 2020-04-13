#include "mylib.h"
#define MAX 10
int arr1[MAX][MAX] = {0}, arr2[MAX][MAX] = {0}, arr3[MAX][MAX] = {0};

void matrix_mult()
{
  for (int i = 0; i < MAX; i++)
    for (int j = 0; j < MAX; j++)
    {
      int ret = 0;
      for (int k = 0; k < MAX; k++)
      {
        ret += arr1[i][k] * arr2[k][j];
      }
      arr3[i][j] = ret;
    }
}

int main()
{

  for (int i = 0; i < MAX; i++)
    for (int j = 0; j < MAX; j++)
    {
      arr1[i][j] = j;
    }
  for (int i = 0; i < MAX; i++)
    for (int j = 0; j < MAX; j++)
    {
      arr2[i][j] = j;
    }

  print_s("arr1\n");
  for (int i = 0; i < MAX; i++)
  {
    for (int j = 0; j < MAX; j++)
    {
      print_d(arr1[i][j]);
      print_c(' ');
    }
    print_c('\n');
  }
  print_s("arr2\n");
  for (int i = 0; i < MAX; i++)
  {
    for (int j = 0; j < MAX; j++)
    {
      print_d(arr2[i][j]);
      print_c(' ');
    }
    print_c('\n');
  }

  matrix_mult();

  print_s("arr3\n");
  for (int i = 0; i < MAX; i++)
  {
    for (int j = 0; j < MAX; j++)
    {
      print_d(arr3[i][j]);
      print_c(' ');
    }
    print_c('\n');
  }
  return 0;
}