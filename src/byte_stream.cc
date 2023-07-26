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

  bytes_write_ += data.size();
  stream_push_( data );
}

void Writer::close()
{
  closed_ = true;
}

void Writer::set_error()
{
  errored_ = true;
}

bool Writer::is_closed() const
{
  return closed_;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - stream_buffered_();
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_write_;
}

string_view Reader::peek() const
{
  return stream_peek_();
}

bool Reader::is_finished() const
{
  return closed_ && stream_is_empty_();
}

bool Reader::has_error() const
{
  return errored_;
}

void Reader::pop( uint64_t len )
{
  const uint64_t _len = std::min( bytes_buffered(), len );

  stream_pop_( _len );
  bytes_read_ += _len;
}

uint64_t Reader::bytes_buffered() const
{
  return stream_buffered_();
}

uint64_t Reader::bytes_popped() const
{
  return bytes_read_;
}

bool ByteStream::stream_is_empty_() const
{
  return _stream.empty();
}

uint64_t ByteStream::stream_buffered_() const
{
  return _stream.size();
}

void ByteStream::stream_push_( std::string& data )
{
  _stream.append( data );
}

void ByteStream::stream_pop_( uint64_t len )
{
  _stream.erase( 0, len );
}

std::string_view ByteStream::stream_peek_() const
{
  return std::string_view { _stream };
}
