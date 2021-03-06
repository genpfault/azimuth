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
#ifndef AZIMUTH_SYSTEM_RESOURCE_H_
#define AZIMUTH_SYSTEM_RESOURCE_H_

#include <stdbool.h>

#include "azimuth/util/rw.h"

/*===========================================================================*/

// Get the path to the user-specific directory for storing persistent data for
// this application (e.g. preferences or save files) as a NUL-terminated string
// (without the trailing slash).  If the directory doesn't already exist,
// create it.  If anything fails, this will return a NULL pointer.
const char *az_get_app_data_directory(void);

// An az_resource_reader_fn_t for reading game resources.
bool az_system_resource_reader(const char *name, az_reader_t *reader);

/*===========================================================================*/

#endif // AZIMUTH_SYSTEM_RESOURCE_H_
