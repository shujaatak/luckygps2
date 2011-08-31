/*
 * This file is part of the luckyGPS project.
 *
 * Copyright (C) 2009 - 2011 Daniel Genrich <dg@luckygps.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "osmadressmanager.h"

/*
select "addr:city", "addr:street", "addr:postcode","addr:housenumber" as addr_housenumber
from world_point
where "addr:housenumber" is not null and "addr:street" is not null
and world_point.rowid in (SELECT pkid FROM 'idx_world_point_way' WHERE xmin >= -113.6 AND xmax <= -113.55 AND ymin >= 53.25 AND ymax <= 53.3)
*/

/*
select osm_id, "addr:interpolation"
FROM world_line
WHERE "addr:interpolation" is not null
*/

// CANNOT use map sqlite: for interpolations, node id's are not saved
// TODO: import data from temp file and insert into sqlite file, create spatial index

osmAdressManager::osmAdressManager()
{
}
