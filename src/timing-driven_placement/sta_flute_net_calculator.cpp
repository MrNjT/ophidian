/*
 *
 * This file is part of Ophidian.
 * Ophidian is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Ophidian is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Ophidian.  If not, see <http://www.gnu.org/licenses/>.
 *
*/

#include "sta_flute_net_calculator.h"
#include "../timing/transition.h"
#include <map>
#include <set>

#include <boost/units/pow.hpp>
#include <boost/units/cmath.hpp>

namespace ophidian {
namespace timingdriven_placement {

sta_flute_net_calculator::sta_flute_net_calculator(const timing::graph & graph, const placement::placement & placement, const timing::library &timing_lib, netlist::netlist & netlist) :
    m_graph(graph), m_placement(placement),
    m_timing_lib(timing_lib) {
    netlist.register_net_property(&m_net_sources);

    std::vector< entity::entity >net_pins;
    for(auto net_it : netlist.net_system())
    {
        entity::entity net = net_it.first;
        net_pins = netlist.net_pins(net);
        bool source_found = false;
        for(auto pin : net_pins)
        {
            switch( timing_lib.pin_direction(netlist.pin_std_cell(pin)) )
            {
            case standard_cell::pin_directions::INPUT:
                break;
            case standard_cell::pin_directions::OUTPUT:
                m_net_sources[netlist.net_system().lookup(net)] = pin;
                source_found = true;
                break;
            case standard_cell::pin_directions::NOT_ASSIGNED:
            default:
                assert(false);
                break;
            }
            if(source_found)
                break;
        }
    }
}

sta_flute_net_calculator::~sta_flute_net_calculator() {
}

void sta_flute_net_calculator::update_net(timing::sta_timing_net_edge_calculator *tnet, entity::entity net, timing::graph_nodes_timing & nodes_timing)
{
    interconnection::rc_tree tree;
    auto tap_map = m_flute.create_tree(m_placement, net, tree, m_timing_lib);
    entity::entity source_pin = m_net_sources[m_placement.netlist().net_system().lookup(net)];

    nodes_timing.load(m_graph.rise_node(source_pin), tree.lumped());
    nodes_timing.load(m_graph.fall_node(source_pin), tree.lumped());

    timing::elmore delay(tree, tap_map.at(source_pin));
    timing::elmore_second_moment second(tree, delay);
    for( auto tap : tap_map )
    {
        if(tap.first == source_pin) continue;
        tnet->elmore_delay( m_graph.rise_node(tap.first), delay.at(tap.second) );
        tnet->elmore_delay( m_graph.fall_node(tap.first), delay.at(tap.second) );
        auto elmore_slew = boost::units::sqrt( second.at(tap.second)+second.at(tap.second) - boost::units::pow<2>(delay.at(tap.second)) );
        tnet->elmore_slew( m_graph.rise_node(tap.first), elmore_slew );
        tnet->elmore_slew( m_graph.fall_node(tap.first), elmore_slew );
    }
}



} /* namespace timing */
} /* namespace ophidian */