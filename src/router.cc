#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  uint32_t mask {};

  if ( prefix_length == 0 ) {
    mask = 0;
  } else if ( prefix_length >= 32 ) {
    mask = UINT32_MAX;
  } else {
    mask = ( ( 1U << prefix_length ) - 1 ) << ( 32U - prefix_length );
  }
  mask &= route_prefix;

  _route_table[mask] = { prefix_length, next_hop, interface_num };
}

std::optional<Route> Router::_get_best_route( uint32_t address )
{
  Route matched {};
  uint32_t longest_prefix {};

  if ( _route_table.empty() ) {
    return {};
  }

  for ( auto [mask, route] : _route_table ) {
    uint32_t prefix = address ^ mask;

    // matched route or default one
    if ( route.prefix_length == 0 || prefix >> ( 32U - route.prefix_length ) == 0 ) {
      if ( longest_prefix < route.prefix_length ) {
        longest_prefix = route.prefix_length;
        matched = route;
      }
    }
  }

  return matched;
}

void Router::route()
{
  for ( auto& intf : interfaces_ ) {
    std::optional<InternetDatagram> received = intf.maybe_receive();
    while ( received.has_value() ) {
      InternetDatagram dgram = received.value(); // NOLINT

      // check TTL
      if ( dgram.header.ttl <= 1 ) {
        continue;
      }
      dgram.header.ttl--;
      dgram.header.compute_checksum();

      // get route
      std::optional<Route> matched = _get_best_route( dgram.header.dst );
      if ( !matched.has_value() ) {
        continue;
      }
      Route route = matched.value();

      cerr << "===============\nTarget address: " << Address::from_ipv4_numeric( dgram.header.dst ).to_string()
           << "; Get route => " << ( route.next_hop.has_value() ? route.next_hop.value().to_string() : "(direct)" )
           << " on interface " << route.interface_num << "\n===============\n";

      if ( route.next_hop.has_value() ) {
        interface( route.interface_num ).send_datagram( dgram, route.next_hop.value() );
      } else {
        interface( route.interface_num ).send_datagram( dgram, Address::from_ipv4_numeric( dgram.header.dst ) );
      }

      received = intf.maybe_receive();
    }
  }
}
