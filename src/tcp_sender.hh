#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include <deque>
#include <queue>

class Timer
{
  uint64_t _current_ms {};

public:
  void start_timer( uint64_t threshold_ms );
  void stop_timer();
  bool update_timer( uint64_t ms_since_last_tick );
};

struct cmp
{
  bool operator()( const TCPSenderMessage& left, const TCPSenderMessage& right )
  {
    Wrap32 zero { 0 };
    return left.seqno.unwrap( zero, 0 ) > right.seqno.unwrap( zero, 0 );
  }
};

class TCPSender
{
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;

  bool _first = true;
  bool _closed {};
  bool _received {};
  bool _attempted {};
  Wrap32 _ack_seqno;
  uint16_t _window_size {};
  uint64_t _next_abs_seqno {};
  uint64_t _outstanding_num {};
  uint64_t _retransmission_cnt {};
  std::deque<TCPSenderMessage> _send_queue {};
  std::priority_queue<TCPSenderMessage, std::vector<TCPSenderMessage>, cmp> _retransmission_queue {};
  uint64_t _current_RTO_ms;
  Timer _timer {};

  uint64_t _get_avaliable_size( bool& syn, Reader& outbound_stream, bool& fin );

public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};
