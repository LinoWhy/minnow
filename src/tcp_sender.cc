#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <algorithm>
#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) )
  , initial_RTO_ms_( initial_RTO_ms )
  , _ack_seqno( isn_ )
{}

uint64_t TCPSender::_get_avaliable_size( Reader& outbound_stream ) const
{
  // MAX_PAYLOAD_SIZE - 2 to reserve space for SYN & FIN
  const uint64_t available_num = std::min( outbound_stream.bytes_buffered(), TCPConfig::MAX_PAYLOAD_SIZE - 2 );
  uint64_t window_num = _window_size;

  // Pretend the window size is one.
  if ( window_num == 0 ) {
    window_num = 1;
  } else if ( window_num < _outstanding_num ) {
    return window_num = 0;
  } else {
    window_num -= _outstanding_num;
  }

  return std::min( available_num, window_num );
}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return _outstanding_num;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return _retransmission_cnt;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  TCPSenderMessage msg;

  if ( _send_queue.empty() ) {
    return {};
  }

  msg = _send_queue.front();
  _send_queue.pop_front();
  _retransmission_queue.push_back( msg );
  return msg;
}

void TCPSender::push( Reader& outbound_stream )
{
  uint64_t num {};
  std::string str {};

  num = _get_avaliable_size( outbound_stream );
  while ( _first || num > 0 ) {

    read( outbound_stream, num, str );
    TCPSenderMessage msg {};
    msg.payload = str;

    if ( _first ) {
      msg.SYN = true;
      _first = false;
    }

    if ( outbound_stream.is_finished() ) {
      msg.FIN = true;
    }

    msg.seqno = Wrap32::wrap( _next_abs_seqno, isn_ );
    _send_queue.push_back( msg );
    _next_abs_seqno += msg.sequence_length();
    _outstanding_num += msg.sequence_length();

    num = _get_avaliable_size( outbound_stream );
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  TCPSenderMessage msg {};

  msg.seqno = Wrap32::wrap( _next_abs_seqno, isn_ );
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  _window_size = msg.window_size;
  if ( !msg.ackno.has_value() ) {
    return;
  }
  _ack_seqno = msg.ackno.value();

  const uint64_t ack_seq = _ack_seqno.unwrap( isn_, _next_abs_seqno );

  while ( !_retransmission_queue.empty() ) {
    TCPSenderMessage message = _retransmission_queue.front();
    const uint64_t seqno = message.seqno.unwrap( isn_, _next_abs_seqno );

    // all sequence number are acknowledged
    if ( seqno + message.sequence_length() > ack_seq ) {
      break;
    }
    _retransmission_queue.pop_front();
    _outstanding_num -= message.sequence_length();
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  (void)ms_since_last_tick;
}
