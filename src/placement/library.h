/*
 * library.h
 *
 *  Created on: Jan 31, 2016
 *      Author: csguth
 */

#ifndef SRC_PLACEMENT_LIBRARY_H_
#define SRC_PLACEMENT_LIBRARY_H_

#include <property.h>
#include <boost/bimap.hpp>
#include <vector>
#include <standard_cells.h>
#include "../geometry/geometry.h"
namespace openeda {
namespace placement {

class library : public entity::property {

	using point = geometry::point<double>;
	using polygon = geometry::polygon<point>;
	using multipolygon = geometry::multi_polygon<polygon>;
	using entity2index_map = boost::bimap<entity::entity, std::size_t>;

	std::vector< multipolygon > m_geometries;
	entity2index_map m_mapping;


public:
	library(openeda::standard_cell::standard_cells * std_cells);
	virtual ~library();

	void create(entity::entity e, std::size_t index);
	void destroy(entity::entity e, std::size_t index);

	multipolygon geometry(entity::entity e) const {
		return m_geometries[m_mapping.left.at(e)];
	}

	void geometry(entity::entity e, multipolygon geometry);


};

} /* namespace placement */
} /* namespace openeda */

#endif /* SRC_PLACEMENT_LIBRARY_H_ */