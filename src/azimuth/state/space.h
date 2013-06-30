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
#ifndef AZIMUTH_STATE_SPACE_H_
#define AZIMUTH_STATE_SPACE_H_

#include <stdbool.h>
#include <stdint.h>

#include "azimuth/state/baddie.h"
#include "azimuth/state/camera.h"
#include "azimuth/state/dialog.h"
#include "azimuth/state/door.h"
#include "azimuth/state/gravfield.h"
#include "azimuth/state/node.h"
#include "azimuth/state/particle.h"
#include "azimuth/state/pickup.h"
#include "azimuth/state/planet.h"
#include "azimuth/state/player.h"
#include "azimuth/state/projectile.h"
#include "azimuth/state/room.h"
#include "azimuth/state/script.h"
#include "azimuth/state/ship.h"
#include "azimuth/state/speck.h"
#include "azimuth/state/uid.h"
#include "azimuth/state/upgrade.h"
#include "azimuth/state/wall.h"
#include "azimuth/util/audio.h"
#include "azimuth/util/clock.h"
#include "azimuth/util/prefs.h"
#include "azimuth/util/vector.h"

/*===========================================================================*/

// The number of different cutscenes there are, not counting AZ_SCENE_NOTHING:
#define AZ_NUM_SCENES 5

typedef enum {
  AZ_SCENE_NOTHING = 0,
  AZ_SCENE_CRUISING,
  AZ_SCENE_MOVE_OUT,
  AZ_SCENE_ARRIVAL,
  AZ_SCENE_ESCAPE,
  AZ_SCENE_HOMECOMING
} az_scene_t;

typedef struct {
  az_scene_t scene;
  az_scene_t next;
  double fade_alpha; // 0.0 to 1.0
  double time;
  double param1, param2;
} az_cutscene_state_t;

typedef struct {
  double time_remaining; // seconds
  const char *paragraph;
  int num_lines;
} az_message_t;

typedef struct {
  bool is_active;
  double active_for; // seconds
  double time_remaining; // seconds
  az_script_vm_t vm;
} az_countdown_t;

/*===========================================================================*/

typedef struct {
  enum { AZ_BDS_SHAKE, AZ_BDS_BOOM, AZ_BDS_FADE } step;
  double progress;
  az_baddie_t boss;
} az_boss_death_mode_data_t;

typedef struct {
  enum { AZ_CSS_ALIGN, AZ_CSS_USE, AZ_CSS_FINISH } step;
  double progress; // 0.0 to 1.0
  az_uid_t node_uid;
  az_vector_t position_delta;
  double angle_delta;
} az_console_mode_data_t;

typedef struct {
  enum { AZ_DLS_BEGIN, AZ_DLS_TALK, AZ_DLS_WAIT, AZ_DLS_END } step;
  double progress; // 0.0 to 1.0
  bool bottom_next;
  az_portrait_t top, bottom;
  const char *paragraph;
  int paragraph_length, chars_to_print;
  az_script_vm_t vm;
} az_dialog_mode_data_t;

typedef struct {
  enum { AZ_DWS_FADE_OUT, AZ_DWS_SHIFT, AZ_DWS_FADE_IN } step;
  double progress; // 0.0 to 1.0
  az_room_key_t destination;
  struct {
    az_door_kind_t kind;
    az_uid_t uid;
    az_vector_t position;
    double angle;
  } entrance, exit;
  double cam_start_r, cam_start_theta;
  double cam_delta_r, cam_delta_theta;
} az_doorway_mode_data_t;

typedef struct {
  enum { AZ_GOS_ASPLODE, AZ_GOS_FADE_OUT } step;
  double progress; // 0.0 to 1.0
} az_game_over_mode_data_t;

typedef struct {
  enum { AZ_PSS_FADE_OUT, AZ_PSS_FADE_IN } step;
  double fade_alpha; // 0.0 to 1.0
} az_pausing_mode_data_t;

typedef struct {
  enum { AZ_UGS_OPEN, AZ_UGS_MESSAGE, AZ_UGS_CLOSE } step;
  double progress; // 0.0 to 1.0
  az_upgrade_t upgrade;
} az_upgrade_mode_data_t;

/*===========================================================================*/

typedef struct {
  const az_planet_t *planet;
  const az_preferences_t *prefs;
  int save_file_index;
  az_clock_t clock;
  az_camera_t camera;
  az_ship_t ship;
  az_cutscene_state_t cutscene;
  az_message_t message;
  az_countdown_t countdown;
  az_soundboard_t soundboard;

  // Mode information:
  enum {
    AZ_MODE_NORMAL = 0, // flying around; the normal mode of gameplay
    AZ_MODE_BOSS_DEATH, // animating boss death
    AZ_MODE_CONSOLE, // using a save point, refill node, or comm node
    AZ_MODE_DIALOG, // engaged in story dialog
    AZ_MODE_DOORWAY, // waiting while we pass through a door
    AZ_MODE_GAME_OVER, // showing the game over animation
    AZ_MODE_PAUSING, // entering/leaving the pause screen
    AZ_MODE_UPGRADE // receiving an upgrade
  } mode;
  az_boss_death_mode_data_t boss_death_mode;
  az_console_mode_data_t console_mode;
  az_dialog_mode_data_t dialog_mode;
  az_doorway_mode_data_t doorway_mode;
  az_game_over_mode_data_t game_over_mode;
  az_pausing_mode_data_t pausing_mode;
  az_upgrade_mode_data_t upgrade_mode;

  // Space objects (these all get cleared out when we exit a room):
  double darkness, dark_goal;
  az_uid_t boss_uid;
  az_baddie_t baddies[AZ_MAX_NUM_BADDIES];
  az_door_t doors[AZ_MAX_NUM_DOORS];
  az_gravfield_t gravfields[AZ_MAX_NUM_GRAVFIELDS];
  az_node_t nodes[AZ_MAX_NUM_NODES];
  az_particle_t particles[500];
  az_pickup_t pickups[100];
  az_projectile_t projectiles[250];
  az_speck_t specks[750];
  az_timer_t timers[5];
  az_wall_t walls[AZ_MAX_NUM_WALLS];
  az_uuid_t uuids[AZ_NUM_UUID_SLOTS];
} az_space_state_t;

/*===========================================================================*/

// Remove all objects (baddies, doors, etc.), but leave other fields unchanged.
void az_clear_space(az_space_state_t *state);

// Add all room objects to the space state, on top of whatever objects are
// already there.  You may want to call az_clear_space first to ensure that
// there is room for the new objects.  Note that this function does not make
// any changes to the ship or any other fields.
void az_enter_room(az_space_state_t *state, const az_room_t *room);

// Set the current message (displayed at the bottom of the screen) to the given
// paragraph.  This will automatically intialize the various fields of
// state->message appropriately.
void az_set_message(az_space_state_t *state, const char *paragraph);

// Add and init a new baddie and return a pointer to it, or return NULL if the
// baddie array is already full.
az_baddie_t *az_add_baddie(az_space_state_t *state, az_baddie_kind_t kind,
                           az_vector_t position, double angle);

bool az_insert_particle(az_space_state_t *state, az_particle_t **particle_out);

void az_add_beam(az_space_state_t *state, az_color_t color, az_vector_t start,
                 az_vector_t end, double lifetime, double semiwidth);

void az_add_speck(az_space_state_t *state, az_color_t color, double lifetime,
                  az_vector_t position, az_vector_t velocity);

// Add a new projectile object and return a pointer to it, or return NULL if
// the projectile array is full.
az_projectile_t *az_add_projectile(
    az_space_state_t *state, az_proj_kind_t kind, bool fired_by_enemy,
    az_vector_t position, double angle, double power);

// Add a pickup of a randomly chosen kind, among those kinds allowed by the
// potential_pickups argument.
void az_add_random_pickup(az_space_state_t *state,
                          az_pickup_flags_t potential_pickups,
                          az_vector_t position);

// Look up the specified object based on its UID and return true, or return
// false if the object with that UID doesn't currently exist.
bool az_lookup_baddie(az_space_state_t *state, az_uid_t uid,
                      az_baddie_t **baddie_out);
bool az_lookup_door(az_space_state_t *state, az_uid_t uid,
                    az_door_t **door_out);
bool az_lookup_gravfield(az_space_state_t *state, az_uid_t uid,
                         az_gravfield_t **gravfield_out);
bool az_lookup_node(az_space_state_t *state, az_uid_t uid,
                    az_node_t **node_out);
bool az_lookup_wall(az_space_state_t *state, az_uid_t uid,
                    az_wall_t **wall_out);

/*===========================================================================*/

typedef enum {
  AZ_IMP_NOTHING = 0,
  AZ_IMP_BADDIE,
  AZ_IMP_DOOR_INSIDE,
  AZ_IMP_DOOR_OUTSIDE,
  AZ_IMP_SHIP,
  AZ_IMP_WALL
} az_impact_type_t;

typedef uint_fast8_t az_impact_flags_t;
#define AZ_IMPF_NOTHING      ((az_impact_flags_t)0)
#define AZ_IMPF_BADDIE       ((az_impact_flags_t)(1u << AZ_IMP_BADDIE))
#define AZ_IMPF_DOOR_INSIDE  ((az_impact_flags_t)(1u << AZ_IMP_DOOR_INSIDE))
#define AZ_IMPF_DOOR_OUTSIDE ((az_impact_flags_t)(1u << AZ_IMP_DOOR_OUTSIDE))
#define AZ_IMPF_SHIP         ((az_impact_flags_t)(1u << AZ_IMP_SHIP))
#define AZ_IMPF_WALL         ((az_impact_flags_t)(1u << AZ_IMP_WALL))

typedef struct {
  az_impact_type_t type;
  union {
    struct {
      az_baddie_t *baddie;
      const az_component_data_t *component;
    } baddie;
    az_door_t *door;
    az_wall_t *wall;
  } target;
  double angle; // only used for the az_arc_*_impact functions
  az_vector_t position;
  // Normal vector.  For ray impacts, the length is not guaranteed, but for
  // circle impacts it will point from the point of contact to position.
  az_vector_t normal;
} az_impact_t;

void az_ray_impact(
    az_space_state_t *state, az_vector_t start, az_vector_t delta,
    az_impact_flags_t skip_types, az_uid_t skip_uid, az_impact_t *impact_out);

void az_circle_impact(
    az_space_state_t *state, double circle_radius,
    az_vector_t start, az_vector_t delta,
    az_impact_flags_t skip_types, az_uid_t skip_uid, az_impact_t *impact_out);

void az_arc_circle_impact(
    az_space_state_t *state, double circle_radius,
    az_vector_t start, az_vector_t spin_center, double spin_angle,
    az_impact_flags_t skip_types, az_uid_t skip_uid, az_impact_t *impact_out);

/*===========================================================================*/

#endif // AZIMUTH_STATE_SPACE_H_
