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
  if ( message.SYN ) {
    _synced = true;
    _initial_seqno = message.seqno;
  }

  if ( !_synced ) {
    return;
  }

  if ( Wrap32::wrap( _next_abs_seqno, _initial_seqno ) == message.seqno ) {
    _next_abs_seqno += message.sequence_length();
  }

  if ( !message.payload.empty() || message.FIN ) {
    reassembler.insert( _next_stream_index, message.payload, message.FIN, inbound_stream );
    _next_stream_index += message.payload.length();
  }
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
