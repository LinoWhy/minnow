#include <algorithm>
#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  const uint64_t available = available_capacity();

  if ( data.size() > available ) {
    data.erase( available );
  }

  bytes_write += data.size();
  stream_push( std::move( data ) );
}

void Writer::close()
{
  closed = true;
}

void Writer::set_error()
{
  errored = true;
}

bool Writer::is_closed() const
{
  return closed;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - stream_buffered();
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_write;
}

string_view Reader::peek() const
{
  return stream_peek();
}

bool Reader::is_finished() const
{
  return closed && stream_is_empty();
}

bool Reader::has_error() const
{
  return errored;
}

void Reader::pop( uint64_t len )
{
  const uint64_t _len = std::min( bytes_buffered(), len );

  stream_pop( _len );
  bytes_read += _len;
}

uint64_t Reader::bytes_buffered() const
{
  return stream_buffered();
}

uint64_t Reader::bytes_popped() const
{
  return bytes_read;
}
