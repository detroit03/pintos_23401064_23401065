#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdint.h>

#define Q 14 // Q = 14, ondalık kısım için 14 bit yer alır.
#define F (1 << Q) // F = 2^14 = 16384

// converter: int to fixed-point
static int cnvrtr_int_fp (int n)
{
  return n * F;
}

// converter: fixed-point to int
static int cnvrtr_fp_int (int x)
{
  return x / F;
}

// converter: fixed-point to int (en yakına)
static int cnvrtr_fp_int_round (int x)
{
  if (x >= 0)
    return (x + F / 2) / F;
  else
    return (x - F / 2) / F;
}

// fixed-point + fixed-point
static int add_fp_fp (int x, int y)
{
  return x + y;
}

// fixed-point + int
static int add_fp_int (int x, int n)
{
  return x + n * F;
}

// fixed-point - fixed-point
static int sub_fp_fp (int x, int y)
{
  return x - y;
}

// fixed-point - int
static int sub_fp_int (int x, int n)
{
  return x - n * F;
}

// fixed-point * fixed-point
static int mult_fp_fp (int x, int y)
{
  return (int)(((int64_t) x) * y / F);
}

// fixed-point * int
static int mult_fp_int (int x, int n)
{
  return x * n;
}

// fixed-point / fixed-point
static int div_fp_fp (int x, int y)
{
  return (int)(((int64_t) x) * F / y);
}

// fixed-point / int
static int div_fp_int (int x, int n)
{
  return x / n;
}

#endif


