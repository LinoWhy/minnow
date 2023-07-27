#include "wrapping_integers.hh"

#include <cmath>

using namespace std;

uint64_t diff_abs( uint64_t a, uint64_t b )
{
  return ( a > b ) ? ( a - b ) : ( b - a );
}

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + n;
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  const uint64_t round_val = 1UL << 32;
  const uint64_t offset = raw_value_ - zero_point.raw_value_;
  // round up
  uint64_t round_num = ( checkpoint + round_val / 2 ) / round_val;

  const uint64_t val_ceil = round_num * round_val + offset;
  const uint64_t val_floor = val_ceil - round_val;
  const uint64_t diff_ceil = diff_abs( checkpoint, val_ceil );
  const uint64_t diff_floor = diff_abs( checkpoint, val_floor );

  if ( diff_floor <= diff_ceil ) {
    return val_floor;
  }
  return val_ceil;
}
