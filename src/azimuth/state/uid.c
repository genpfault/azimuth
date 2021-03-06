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

#include "azimuth/state/uid.h"

#include <assert.h>

/*===========================================================================*/

#define AZ_UID_INDEX_MASK (UINT64_C(0xFFFF))
#define AZ_UID_COUNT_STEP (UINT64_C(0x10000))

#define SHIP_UID ((az_uid_t)(-1))
const az_uid_t AZ_NULL_UID = 0u;
const az_uid_t AZ_SHIP_UID = SHIP_UID;
const az_uuid_t AZ_NULL_UUID = { .type = AZ_UUID_NOTHING };
const az_uuid_t AZ_SHIP_UUID = { .type = AZ_UUID_SHIP, .uid = SHIP_UID };

void az_assign_uid(int index, az_uid_t *uid) {
  assert(index >= 0);
  const uint64_t index64 = index;
  assert(index64 == (index64 & AZ_UID_INDEX_MASK));
  *uid = ((*uid + AZ_UID_COUNT_STEP) & ~AZ_UID_INDEX_MASK) | index64;
}

int az_uid_index(az_uid_t uid) {
  return (int)(uid & AZ_UID_INDEX_MASK);
}

/*===========================================================================*/
