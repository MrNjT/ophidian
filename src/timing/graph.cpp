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

#include "graph.h"

namespace ophidian {
namespace timing {

graph::graph() :
		m_pins(m_graph),
		m_node_edges(m_graph),
		m_arc_types(m_graph),
		m_arcs(m_graph){
}

graph::~graph() {
}

graph::node graph::node_create(entity::entity pin, edges node_edge, std::unordered_map< entity::entity, node > & map) {
	auto new_node = m_graph.addNode();
	if(map.find(pin) == map.end())
		map[pin] = new_node;
	m_pins[new_node] = pin;
	m_node_edges[new_node] = node_edge;
	return new_node;
}

graph::node graph::rise_node_create(entity::entity pin) {
	return node_create(pin, edges::RISE, m_rise_nodes);
}
graph::node graph::fall_node_create(entity::entity pin) {
	return node_create(pin, edges::FALL, m_fall_nodes);
}

void graph::node_edge(node u, edges e) {
	m_node_edges[u] = e;
}

graph::edge graph::edge_create(node u, node v, edge_types type, entity::entity tarc) {
	auto edge = m_graph.addArc(u, v);
	m_arcs[edge] = tarc;
	m_arc_types[edge] = type;
	return edge;
}

void graph::edge_source(edge e, node u) {
	m_graph.changeSource(e, u);
}

}
/* namespace timing */
} /* namespace ophidian */
