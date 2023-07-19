#pragma once

#include <algorithm>
#include <queue>
#include <stdexcept>
#include <string>
#include <string_view>

class Reader;
class Writer;

class ByteStream
{
private:
  std::deque<char> stream = {};

protected:
  uint64_t capacity_;
  // Please add any additional state to the ByteStream here, and not to the Writer and Reader interfaces.
  bool closed = {};
  bool errored = {};
  uint64_t bytes_write = {};
  uint64_t bytes_read = {};

public:
  explicit ByteStream( uint64_t capacity );

  // Helper functions (provided) to access the ByteStream's Reader and Writer interfaces
  Reader& reader();
  const Reader& reader() const;
  Writer& writer();
  const Writer& writer() const;

  // Helper functions of stream data
  bool stream_is_empty() const { return stream.empty(); }
  uint64_t stream_buffered() const { return stream.size(); }
  bool stream_push( char c )
  {
    if ( stream_buffered() >= capacity_ ) {
      return false;
    }

    stream.push_back( c );
    bytes_write++;
    return true;
  }
  void stream_pop( uint64_t len )
  {
    uint64_t _len = std::min(len, stream.size());
    for ( uint64_t i = 0; i < _len; i++ ) {
      stream.pop_front();
      bytes_read++;
    }
  }
  std::string_view stream_peek() const
  {
    static std::string s {};
    s.clear();
    for ( const auto& i : stream ) {
      s.push_back( i );
    }
    return std::string_view { s };
  }
};

class Writer : public ByteStream
{
public:
  void push( std::string data ); // Push data to stream, but only as much as available capacity allows.

  void close();     // Signal that the stream has reached its ending. Nothing more will be written.
  void set_error(); // Signal that the stream suffered an error.

  bool is_closed() const;              // Has the stream been closed?
  uint64_t available_capacity() const; // How many bytes can be pushed to the stream right now?
  uint64_t bytes_pushed() const;       // Total number of bytes cumulatively pushed to the stream
};

class Reader : public ByteStream
{
public:
  std::string_view peek() const; // Peek at the next bytes in the buffer
  void pop( uint64_t len );      // Remove `len` bytes from the buffer

  bool is_finished() const; // Is the stream finished (closed and fully popped)?
  bool has_error() const;   // Has the stream had an error?

  uint64_t bytes_buffered() const; // Number of bytes currently buffered (pushed and not popped)
  uint64_t bytes_popped() const;   // Total number of bytes cumulatively popped from stream
};

/*
 * read: A (provided) helper function thats peeks and pops up to `len` bytes
 * from a ByteStream Reader into a string;
 */
void read( Reader& reader, uint64_t len, std::string& out );
