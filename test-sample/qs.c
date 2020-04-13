#include "mylib.h"
#define BUF_SIZE 100

void display(int array[], int maxlen)
{
  int i;
  for (int i = 0; i < maxlen; ++i)
  {
    print_d(array[i]);
    print_c(' ');
  }
  print_c('\n');
  return;
}

void QuickSort(int *arr, int low, int high)
{
  if (low < high)
  {
    int i = low;
    int j = high;
    int k = arr[low];
    while (i < j)
    {
      while (i < j && arr[j] >= k)
      {
        j--;
      }

      if (i < j)
      {
        arr[i++] = arr[j];
      }

      while (i < j && arr[i] < k)
      {
        i++;
      }

      if (i < j)
      {
        arr[j--] = arr[i];
      }
    }

    arr[i] = k;

    QuickSort(arr, low, i - 1);
    QuickSort(arr, i + 1, high);
  }
}

int main()
{
  int array[BUF_SIZE] = {12, 85, 25, 16, 34, 23, 49, 95, 17, 61, 12, 85, 25, 16, 34, 23, 49, 95, 17, 61, 12, 85, 25, 16, 34, 23, 49, 95, 17, 61, 12, 85, 25, 16, 34, 23, 49, 95, 17, 61, 12, 85, 25, 16, 34, 23, 49, 95, 17, 61, 12, 85, 25, 16, 34, 23, 49, 95, 17, 61, 12, 85, 25, 16, 34, 23, 49, 95, 17, 61, 12, 85, 25, 16, 34, 23, 49, 95, 17, 61};
  int maxlen = BUF_SIZE;
  print_s("before:\n");
  display(array, maxlen);
  QuickSort(array, 0, maxlen - 1);
  print_s("after:\n");
  display(array, maxlen);
  return 0;
}