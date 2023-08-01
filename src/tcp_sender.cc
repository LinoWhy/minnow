#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <algorithm>
#include <random>
#include <stop_token>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) )
  , initial_RTO_ms_( initial_RTO_ms )
  , _ack_seqno( isn_ )
  , _current_RTO_ms( initial_RTO_ms )
{}

void Timer::start_timer( uint64_t threshold_ms )
{
  if ( _current_ms == 0 ) {
    _current_ms = threshold_ms;
  }
}

bool Timer::update_timer( uint64_t ms_since_last_tick )
{
  if ( _current_ms <= ms_since_last_tick ) {
    _current_ms = 0;
    return true;
  }

  _current_ms -= ms_since_last_tick;
  return false;
}

void Timer::stop_timer()
{
  _current_ms = 0;
}

uint64_t TCPSender::_get_avaliable_size( Reader& outbound_stream )
{
  // MAX_PAYLOAD_SIZE - 2 to reserve space for SYN & FIN
  uint64_t available_num = std::min( outbound_stream.bytes_buffered(), TCPConfig::MAX_PAYLOAD_SIZE - 2 );
  uint64_t window_num = _window_size;

  // Pretend the window size is one.
  if ( window_num == 0 ) {
    window_num = 1;
  } else if ( window_num < _outstanding_num ) {
    return window_num = 0;
  } else {
    window_num -= _outstanding_num;
  }

  // Ensure FIN is sent in case of empty stream and nonzero window size
  if ( !_closed && outbound_stream.is_finished() && window_num > 0 ) {
    available_num = 1;
    _closed = true;
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
  _retransmission_queue.push( msg );
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
      _closed = true;
      msg.FIN = true;
    }

    msg.seqno = Wrap32::wrap( _next_abs_seqno, isn_ );
    _send_queue.push_back( msg );
    _next_abs_seqno += msg.sequence_length();

    // not in maybe_send?
    _timer.start_timer( _current_RTO_ms );
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

  // check if ackno is valid
  const uint64_t ack_seq = msg.ackno.value().unwrap( isn_, _next_abs_seqno );
  if ( ack_seq <= _ack_seqno.unwrap( isn_, _next_abs_seqno ) || ack_seq > _next_abs_seqno ) {
    return;
  }

  // Some data is successfully received.
  _ack_seqno = msg.ackno.value();

  while ( !_retransmission_queue.empty() ) {
    TCPSenderMessage message = _retransmission_queue.top();
    const uint64_t seqno = message.seqno.unwrap( isn_, _next_abs_seqno );

    // Remove from the retransmission queue, if all sequence number are acknowledged.
    if ( seqno + message.sequence_length() > ack_seq ) {
      break;
    }
    _retransmission_queue.pop();
    _outstanding_num -= message.sequence_length();
  }

  // reset RTO threshold & timer
  _retransmission_cnt = 0;
  _current_RTO_ms = initial_RTO_ms_;
  _timer.stop_timer();

  // Restart timer
  if ( _outstanding_num != 0 ) {
    _timer.start_timer( _current_RTO_ms );
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  bool elapsed {};

  elapsed = _timer.update_timer( ms_since_last_tick );
  if ( !elapsed ) {
    return;
  }

  // Retransmit data
  if ( !_retransmission_queue.empty() ) {
    TCPSenderMessage msg = _retransmission_queue.top();
    _send_queue.push_back( msg );
    _retransmission_queue.pop();
  }

  _retransmission_cnt++;
  // slow down retransmission timer
  _current_RTO_ms *= 2;

  // Reset & restart the timer
  _timer.stop_timer();
  _timer.start_timer( _current_RTO_ms );
}
