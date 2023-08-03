#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"
#include <algorithm>

using namespace std;

constexpr uint64_t ARP_CYCLE_MS = 5000;
constexpr uint64_t CACHE_LAST_MS = 30000;

EthElem::EthElem( const EthernetAddress& addr, const uint64_t& t ) : address( addr ), timer( t ) {}

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  EthernetFrame msg_dgram {};
  uint32_t ip_address = next_hop.ipv4_numeric();

  msg_dgram.header.src = ethernet_address_;
  msg_dgram.header.type = EthernetHeader::TYPE_IPv4;
  msg_dgram.payload = serialize( dgram );

  // known ip address
  auto search = _ip_eth_map.find( ip_address );
  if ( search != _ip_eth_map.end() && search->second.address != EthernetAddress {} && search->second.timer > 0 ) {
    msg_dgram.header.dst = search->second.address;
    _send_msg.push( msg_dgram );
    return;
  }

  // unknown ip address but the ARP message was sent recently
  if ( search != _ip_eth_map.end() && search->second.timer > 0 ) {
    return;
  }

  // new unknown ip address
  auto [elem, inserted] = _ip_eth_map.insert( { ip_address, { EthernetAddress {}, ARP_CYCLE_MS } } );
  if ( !inserted ) {
    elem->second.address = EthernetAddress {};
    elem->second.timer = ARP_CYCLE_MS;
  }

  ARPMessage arp {};
  arp.opcode = ARPMessage::OPCODE_REQUEST;
  arp.sender_ethernet_address = ethernet_address_;
  arp.sender_ip_address = ip_address_.ipv4_numeric();
  arp.target_ethernet_address = {};
  arp.target_ip_address = ip_address;

  EthernetFrame msg {};
  msg.header.src = ethernet_address_;
  msg.header.dst = ETHERNET_BROADCAST;
  msg.header.type = EthernetHeader::TYPE_ARP;
  msg.payload = serialize( arp );

  _send_msg.push( msg );
  auto [it, _] = _wait_msg.insert( { ip_address, {} } );
  it->second.push_back( msg_dgram );
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  EthernetHeader header = frame.header;

  if ( header.dst != ethernet_address_ && header.dst != ETHERNET_BROADCAST ) {
    return {};
  }

  // IPv4 datagram
  InternetDatagram dgram {};
  if ( header.type == EthernetHeader::TYPE_IPv4 && parse( dgram, frame.payload ) ) {
    return dgram;
  }

  // Is valid arp message?
  ARPMessage arp {};
  if ( header.type != EthernetHeader::TYPE_ARP || !parse( arp, frame.payload ) ) {
    return {};
  }

  // Trust all ARP message
  auto [elem, inserted]
    = _ip_eth_map.insert( { arp.sender_ip_address, { arp.sender_ethernet_address, CACHE_LAST_MS } } );
  if ( !inserted ) {
    elem->second.address = arp.sender_ethernet_address;
    elem->second.timer = CACHE_LAST_MS;
  }
  // EthElem elm = { arp.sender_ethernet_address, CACHE_LAST_MS };
  // _ip_eth_map[arp.sender_ip_address] = elm;

  // Reply ARP request
  if ( arp.opcode == ARPMessage::OPCODE_REQUEST && arp.target_ip_address == ip_address_.ipv4_numeric() ) {
    ARPMessage reply {};
    reply.opcode = ARPMessage::OPCODE_REPLY;
    reply.sender_ethernet_address = ethernet_address_;
    reply.sender_ip_address = ip_address_.ipv4_numeric();
    reply.target_ethernet_address = arp.sender_ethernet_address;
    reply.target_ip_address = arp.sender_ip_address;

    EthernetFrame msg {};
    msg.header.src = ethernet_address_;
    msg.header.dst = header.src;
    msg.header.type = EthernetHeader::TYPE_ARP;
    msg.payload = serialize( reply );

    _send_msg.push( msg );
  }

  // Send all waiting messages, since ip map may be updated
  auto search = _wait_msg.find( arp.sender_ip_address );
  if ( search == _wait_msg.end() ) {
    return {};
  }

  while ( !search->second.empty() ) {
    EthernetFrame msg = search->second.front();
    search->second.pop_front();
    msg.header.dst = arp.sender_ethernet_address;
    _send_msg.push( msg );
  }

  return {};
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  for ( auto& [_, elem] : _ip_eth_map ) {
    if ( elem.timer > ms_since_last_tick ) {
      elem.timer -= ms_since_last_tick;
    } else {
      elem.timer = 0;
    }
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  EthernetFrame msg;

  if ( _send_msg.empty() ) {
    return {};
  }

  msg = _send_msg.front();
  _send_msg.pop();
  return msg;
}
