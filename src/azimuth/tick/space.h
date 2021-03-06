/*=============================================================================
| Copyright 2012 Matthew D. Steele <mdsteele@alum.mit.edu>                    |
|                                                                             |
| This file is part of Azimuth.                                               |
|                                                                             |
| Azimuth is free software: you can redistribute it and/or modify it under    |
| the terms of the GNU General Public License as published by the Free        |
| Software Foundation, either version 3 of the License, or (at your option)   |
| any later version.                                                          |
|                                                                             |
| Azimuth is distributed in the hope that it will be useful, but WITHOUT      |
| ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       |
| FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for   |
| more details.                                                               |
|                                                                             |
| You should have received a copy of the GNU General Public License along     |
| with Azimuth.  If not, see <http://www.gnu.org/licenses/>.                  |
=============================================================================*/

#pragma once
#ifndef AZIMUTH_TICK_SPACE_H_
#define AZIMUTH_TICK_SPACE_H_

#include "azimuth/state/space.h"

/*===========================================================================*/

// Call this just after entering a room.  The state should already have been
// filled with the room contents and the ship should be in position.  This
// takes care of recording the room on the minimap, adjusting the camera,
// setting the music, and running the room script (if any).
void az_after_entering_room(az_space_state_t *state);

void az_tick_space_state(az_space_state_t *state, double time);

/*===========================================================================*/

#endif // AZIMUTH_TICK_SPACE_H_
