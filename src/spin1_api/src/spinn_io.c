
//------------------------------------------------------------------------------
//
// spinn_io.c	    Simple character I/O library for Spinnaker
//
// Copyright (C)    The University of Manchester - 2009, 2010, 2011
//
// Author           Steve Temple, APT Group, School of Computer Science
//		    Fixed point format by Paul Richmond (Univ. of Sheffield)
//
// Email            temples@cs.man.ac.uk
//
//------------------------------------------------------------------------------

#include "spinnaker.h"
#include "spinn_io.h"

#include <stdarg.h>


static const char hex[] = "0123456789abcdef";
static uint sp_ptr; 	// for 'sprintf'

#define FIXED		// Fixed point (16.16) support

#ifdef PUT_CHAR
extern void PUT_CHAR (uint c, char *stream);
#else
#define PUT_CHAR svc_put_char
#ifdef __GNUC__
extern divmod_t div10 (uint n);
extern void svc_put_char (uint c, char *stream);
#else
extern __value_in_regs divmod_t div10 (uint n);
void __svc(0) svc_put_char (uint c, char *stream);
#endif
#endif


void io_put_char (char *stream, uint c)
{
  if (stream < IO_NULL)
    {
      PUT_CHAR (c, stream);
    }
  else if (stream > IO_NULL)
    {
      stream[sp_ptr++] = c;
      stream[sp_ptr] = 0;
    }
}


void io_put_str (char *stream, char *s, int d)
{
  char *t = s;
  int n = 0;

  while (*t++)
    n++;

  while (d-- > n)
    io_put_char (stream, ' ');

  while (*s)
    io_put_char (stream, *s++);
}


void io_put_int (char *stream, int n, uint d, uint pad) // pad not used!
{
  char s[12];
  int i = 0;
  uint neg = 0;

  if (n < 0)
    {
      n = -n;
      neg = '-';
    }

  while (1)
    {
      divmod_t r = div10 (n);

      s[i++] = r.mod + '0';
      n = r.div;
      if (n == 0) break;
    }

  while (i > 0 && s[--i] == '0')
    continue;

  if (neg)
    s[++i] = neg;

  while (d-- > i + 1)
    io_put_char (stream, ' ');

  while (i >= 0)
    io_put_char (stream, s[i--]);
}


void io_put_uint (char *stream, uint n, uint d, uint pad)
{
  char s[12];
  int i = 0;

  while (1)
    {
      divmod_t r = div10 (n);

      s[i++] = r.mod + '0';
      n = r.div;
      if (n == 0) break;
    }

  while (i > 0 && s[--i] == '0')
    continue;

  while (d-- > i + 1)
    io_put_char (stream, pad);

  while (i >= 0)
    io_put_char (stream, s[i--]);
}


void io_put_zhex (char *stream, uint n, uint d)
{
  for (int i = d - 1; i >= 0; i--)
    io_put_char (stream, hex [(n >> (4 * i)) & 15]);
}


void io_put_hex (char *stream, uint n, uint d, uint pad)
{
  char s[8];
  int i = 0;

  while (1)
    {
      s[i++] = hex[n & 15];
      n = n >> 4;
      if (n == 0) break;
    }

  while (i > 0 && s[--i] == '0')
    continue;

  while (d-- > i + 1)
    io_put_char (stream, pad);

  while (i >= 0)
    io_put_char (stream, s[i--]);
}


#ifdef EXTENSIONS
void io_put_nl (char *stream)
{
  io_put_char (stream, '\n');
}
 

void io_put_mac (char *stream, uchar *s)
{
  for (uint i = 0; i < 6; i++)
    {
      io_put_zhex (stream, s[i], 2);
      if (i != 5)
	io_put_char (stream, ':');
    }
}


void io_put_ip (char *stream, uchar *s)
{
  for (uint i = 0; i < 4; i++)
    {
      io_put_int (stream, s[i], 0, 0);
      if (i != 3)
	io_put_char (stream, '.');
    }
}
#endif

#ifdef FIXED
void io_put_fixed (char *stream, int n, uint d, uint a, uint pad)
{
  char s[25];

// <lap>
  uint neg = 0;

  if (n < 0)
    {
      n = -n;
      neg = '-';
    }
// </lap>

  uint f = n;

  // fractional part

  if (a > 12) // maximum precision of 12 to prevent overflow
    a = 12;

  uint i = 0;

  while (i < a)
    {
      f &= 0x0000ffff;
      f *= 10;
      s[a - ++i] = (f >> 16) + '0';
    }

  // set carry for rounding

  f &= 0x0000ffff;
  f *= 10;

  uint c = (f >> 16) > 4;

  // carry any rounding

  f = 0;
  while ((c) && (f < a))
    {
      if (s[f] >= '9')
	{
	  s[f++] = '0';
	}
      else
	{
	  s[f++]++;
	  c = 0;
	}
    }

  // add decimal

  if (a)
    s[i++] = '.';

  // integer part

  n = (n >> 16) + c; // add the carry

  while (1)
    {
      divmod_t r = div10 (n);

      s[i++] = r.mod + '0';
      n = r.div;
      if (n == 0)
	break;
    }

// <lap>
  if (neg)
    s[i++] = neg;
// </lap>

  while (d-- > (i+1))
    io_put_char (stream, pad);

  while (i > 0)
    io_put_char (stream, s[--i]);
}
#endif


void io_printf (char *stream, char *f, ...) 
{
  va_list ap;
 
  if (stream == IO_NULL) // Slight optimisation
    return;

  if (stream > IO_NULL) // 'sprintf' - initialise string
    {
      sp_ptr = 0;
      stream[0] = 0;
    }

  va_start (ap, f);

  while (1)
    {
      char c = *f++;

      if (c == 0)
	return;

      if (c != '%')
	{
	  io_put_char (stream, c);
	  continue;
	}

      c = *f++;

      if (c == 0)
	return;

      char pad = ' ';

      if (c == '0')
	pad = c;

      uint size = 0;

      while (c >= '0' && c <= '9')
	{
	  size = 10 * size + c - '0';
	  c = *f++;
	  if (c == 0)
	    return;
	}

#ifdef FIXED
      uint precision = 6;

      if (c == '.')
	{
	  precision = 0;
	  c = *f++;
	  while (c >= '0' && c <= '9')
	    {
	      precision = 10 * precision + c - '0';
	      c = *f++;
	      if (c == 0)
		return;
	    }
	}
#endif

#ifdef EXTENSIONS
      uint t;
#endif

      switch (c)
	{
	case 'c':
	  io_put_char (stream, va_arg (ap, uint));
	  break;

	case 's':
	  io_put_str (stream, va_arg (ap, char *), size);
	  break;

	case 'd':
	  io_put_int (stream, va_arg (ap, int), size, pad);
	  break;

	case 'u':
	  io_put_uint (stream, va_arg (ap, uint), size, pad);
	  break;
#ifdef FIXED
        case 'f':
	  io_put_fixed (stream, va_arg (ap, int), size, precision, pad);
	  break;
#endif
	case 'x': // hex, digits as needed
	  io_put_hex (stream, va_arg (ap, uint), size, pad);
	  break;

	case 'z': // zero prefixed hex, exactly "size" digits
	  io_put_zhex (stream, va_arg (ap, uint), size);
	  break;
#ifdef EXTENSIONS
	case 'h': // pointer to network short (hex)
	  t = va_arg (ap, uint);
	  t = * (ushort *) t;
	  io_put_zhex (stream, ntohs (t), size);
	  break;

	case 'i': // pointer to network short (decimal)
	  t = va_arg (ap, uint);
	  t = * (ushort *) t;
	  io_put_uint (stream, ntohs (t), size, pad);
	  break;

	case 'p': // pointer to IP address
	  io_put_ip (stream, va_arg (ap, uchar *));
	  break;

	case 'm': // pointer to MAC address
	  io_put_mac (stream, va_arg (ap, uchar *));
	  break;
#endif
	default:
	  io_put_char (stream, c);
	}
    }
  //  va_end (ap);
}
 

