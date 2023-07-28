#include "tcp_receiver.hh"
#include "tcp_receiver_message.hh"
#include "wrapping_integers.hh"
#include <algorithm>

using namespace std;

TCPReceiverMessage::TCPReceiverMessage( std::optional<Wrap32> _ackno, uint16_t _window_size )
  : ackno( _ackno ), window_size( _window_size )
{}

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  // Initial states
  if ( !_synced && message.SYN ) {
    _synced = true;
    _initial_seqno = message.seqno;
    _next_abs_seqno = 0;
    _next_stream_index = 0;
    _last_bytes_pushed = 0;
  }

  // Begin with sync
  if ( !_synced ) {
    return;
  }

  const uint64_t next_seqno = message.seqno.unwrap( _initial_seqno, _next_abs_seqno );
  // Push data thats (partially) inside the window size.
  if ( next_seqno + message.sequence_length() > _next_abs_seqno
       || next_seqno < _next_abs_seqno + inbound_stream.available_capacity() ) {
    uint64_t stream_index = next_seqno - 1;

    // message with "SYN" shall start with zero.
    // This message shall be pushed in case of "SYN + FIN"
    if ( message.SYN ) {
      stream_index = 0;
    }
    reassembler.insert( stream_index, message.payload, message.FIN, inbound_stream );
  }

  // Reset for next connect
  if ( inbound_stream.is_closed() ) {
    _synced = false;
  }

  const uint64_t current_bytes_pushed = inbound_stream.bytes_pushed();
  const uint64_t bytes_pushed = current_bytes_pushed - _last_bytes_pushed;
  _last_bytes_pushed = current_bytes_pushed;

  _next_stream_index += bytes_pushed;
  // 1 for SYN, !_synced for FIN
  _next_abs_seqno = _next_stream_index + 1 + !_synced;
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  std::optional<Wrap32> ack_seqno = {};
  const uint64_t size = std::min( inbound_stream.available_capacity(), 65535UL );

  if ( _next_abs_seqno > 0 ) {
    ack_seqno = Wrap32::wrap( _next_abs_seqno, _initial_seqno );
  }
  return TCPReceiverMessage { ack_seqno, static_cast<uint16_t>( size ) };
}
