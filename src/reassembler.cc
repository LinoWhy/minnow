#include "reassembler.hh"

#include <algorithm>
#include <cstring>
#include <optional>

using namespace std;

bool Reassembler::_check_str( uint64_t& first_index, std::string& data, Writer& output ) const
{
  const uint64_t cap = output.available_capacity();

  // discard bytes beyond the available capacity
  if ( output.available_capacity() == 0 || first_index + data.length() < _unassembled_index
       || first_index >= _unassembled_index + output.available_capacity() ) {
    return false;
  }

  // adjust data that overlaps the first unassembled index
  if ( first_index < _unassembled_index ) {
    data.erase( 0, _unassembled_index - first_index );
    first_index = _unassembled_index;
  }

  // adjust data that overlaps the capacity
  if ( first_index + data.length() > _unassembled_index + cap ) {
    data.erase( _unassembled_index + cap - first_index );
  }

  return true;
}

bool Reassembler::_push_str( uint64_t first_index, std::string& data, Writer& output )
{
  if ( first_index == _unassembled_index ) {
    output.push( data );
    _unassembled_index += data.length();

    if ( _eof_index == first_index ) {
      output.close();
    }
    return true;
  }
  return false;
}

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  bool valid = {};
  bool pushed = {};
  std::optional<ReassemblerBuffer> peek = {};
  ReassemblerBuffer buf = {};

  // check & tailor data according to capacity
  valid = _check_str( first_index, data, output );
  if ( !valid ) {
    return;
  }

  // store the EOF index
  if ( is_last_substring ) {
    _eof_index = first_index;
  }

  // push data to output if it fits
  pushed = _push_str( first_index, data, output );
  if ( !pushed ) {
    // push data into unassembled buffer
    buffer_insert( first_index, data );
  } else {
    // update buffer since unassembled index is changed
    buffer_update();

    // check unassembled buffer
    peek = buffer_peak();
    if ( !peek.has_value() ) {
      return;
    }

    buf = peek.value();
    valid = _check_str( buf.first, buf.second, output );
    if ( !valid ) {
      return;
    }

    pushed = _push_str( buf.first, buf.second, output );
    while ( pushed ) {
      buffer_pop();

      peek = buffer_peak();
      if ( !peek.has_value() ) {
        break;
      }

      buf = peek.value();
      valid = _check_str( buf.first, buf.second, output );
      if ( !valid ) {
        break;
      }

      pushed = _push_str( buf.first, buf.second, output );
    }
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return _unassembled_bytes;
}

void Reassembler::buffer_insert( uint64_t first_index, std::string& data )
{
  auto it = _unassembled_buffer.begin();
  const auto tail = _unassembled_buffer.end();
  const uint64_t end_index = first_index + data.length();

  // sort buffer according to the first index
  while ( it != tail && it->first <= first_index ) {
    const uint64_t it_end = it->first + it->second.length();

    // inserted data is inside the current one
    if ( end_index <= it_end ) {
      return;
    }

    // current is inside the inserted data
    if ( it->first == first_index && it_end < end_index ) {
      _unassembled_bytes -= it->second.length();
      it = _unassembled_buffer.erase( it );
      continue;
    }

    // partially overlapped
    if ( it_end > first_index && it_end < end_index ) {
      it->second.erase( first_index - it->first );
      _unassembled_bytes -= ( it_end - first_index );
    }
    it++;
  }
  // insert data into buffer
  _unassembled_buffer.insert( it, ReassemblerBuffer { first_index, data } );
  _unassembled_bytes += data.length();

  // remove or tailor the next overlapped data
  while ( it != tail && it->first < end_index ) {
    // totally overlapped
    if ( it->first + it->second.length() <= end_index ) {
      _unassembled_bytes -= it->second.length();
      it = _unassembled_buffer.erase( it );
    } else {
      // partially overlapped
      const uint64_t num = end_index - it->first;
      it->second.erase( 0, num );
      it->first = end_index;
      _unassembled_bytes -= num;
      it++;
    }
  }
}

std::optional<ReassemblerBuffer> Reassembler::buffer_peak()
{
  if ( _unassembled_buffer.empty() ) {
    return std::nullopt;
  }
  return _unassembled_buffer.front();
}

void Reassembler::buffer_pop()
{
  if ( _unassembled_buffer.empty() ) {
    return;
  }

  const uint64_t len = _unassembled_buffer.front().second.length();
  _unassembled_buffer.pop_front();
  _unassembled_bytes -= len;
}

bool Reassembler::buffer_empty()
{
  return _unassembled_buffer.empty();
}

void Reassembler::buffer_update()
{
  auto it = _unassembled_buffer.begin();
  const auto tail = _unassembled_buffer.end();

  // remove totally overlapped data
  while ( it != tail && it->first + it->second.length() < _unassembled_index ) {
    _unassembled_bytes -= it->second.length();
    it = _unassembled_buffer.erase( it );
  }
}
