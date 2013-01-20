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

#include "azimuth/tick/baddie.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>

#include "azimuth/state/baddie.h"
#include "azimuth/state/projectile.h"
#include "azimuth/tick/script.h"
#include "azimuth/util/misc.h"
#include "azimuth/util/random.h"
#include "azimuth/util/vector.h"

/*===========================================================================*/

// The level of health at or below which a baddie can be frozen.
#define AZ_BADDIE_FREEZE_THRESHOLD 4.0

bool az_try_damage_baddie(
    az_space_state_t *state, az_baddie_t *baddie,
    const az_component_data_t *component, az_damage_flags_t damage_kind,
    double damage_amount) {
  assert(baddie->kind != AZ_BAD_NOTHING);
  assert(baddie->health > 0.0);
  assert(damage_amount >= 0.0);

  // If the damage is zero, we can quit early.
  if (damage_amount <= 0.0) return false;

  // Determine if the baddie is susceptible to this kind of damage; if not,
  // quit early.
  {
    az_damage_flags_t modified_damage_kind = damage_kind;
    modified_damage_kind &= ~AZ_DMGF_FREEZE;
    if (!modified_damage_kind) modified_damage_kind = AZ_DMGF_NORMAL;
    if (!(modified_damage_kind & ~(component->immunities))) return false;
  }

  // Damage the baddie.
  baddie->armor_flare = 1.0;
  baddie->health -= damage_amount;

  // If this was enough to kill the baddie, remove it and add debris/pickups in
  // its place.
  if (baddie->health <= 0.0) {
    // TODO add baddie debris
    az_add_random_pickup(state, baddie->data->potential_pickups,
                         baddie->position);
    const az_script_t *script = baddie->on_kill;
    // Remove the baddie.  After this point, we can no longer use the baddie
    // object.
    baddie->kind = AZ_BAD_NOTHING;
    az_run_script(state, script);
  }
  // Otherwise, if this was enough to freeze the baddie, and the damage kind
  // includes AZ_DMGF_FREEZE, freeze the baddie.
  else if ((damage_kind & AZ_DMGF_FREEZE) &&
           !(component->immunities & AZ_DMGF_FREEZE) &&
           baddie->health <= AZ_BADDIE_FREEZE_THRESHOLD) {
    baddie->frozen = 1.0;
  }
  return true;
}

/*===========================================================================*/

static bool angle_to_ship_within(az_space_state_t *state, az_baddie_t *baddie,
                                 double offset, double angle) {
  return (az_ship_is_present(&state->ship) &&
          fabs(az_mod2pi(baddie->angle + offset -
                         az_vtheta(az_vsub(state->ship.position,
                                           baddie->position)))) <= angle);
}

static bool has_line_of_sight_to_ship(az_space_state_t *state,
                                      az_baddie_t *baddie) {
  az_impact_t impact;
  az_ray_impact(state, baddie->position,
                az_vsub(state->ship.position, baddie->position),
                AZ_IMPF_BADDIE, baddie->uid, &impact);
  return (impact.type == AZ_IMP_SHIP);
}

static void fire_projectile(az_space_state_t *state, az_baddie_t *baddie,
                            az_proj_kind_t kind, double forward,
                            double firing_angle, double proj_angle_offset) {
  az_projectile_t *proj;
  if (az_insert_projectile(state, &proj)) {
    const double theta = firing_angle + baddie->angle;
    az_init_projectile(proj, kind, true,
                       az_vadd(baddie->position, az_vpolar(forward, theta)),
                       theta + proj_angle_offset);
  }
}

static az_vector_t force_field(
    az_space_state_t *state, az_baddie_t *baddie,
    double ship_coeff, double ship_min_range, double ship_max_range,
    double wall_far_coeff, double wall_near_coeff) {
  const az_vector_t pos = baddie->position;
  az_vector_t drift = AZ_VZERO;
  if (az_ship_is_present(&state->ship) &&
      az_vwithin(state->ship.position, pos, ship_max_range)) {
    if (az_vwithin(state->ship.position, pos, ship_min_range)) {
      drift = az_vwithlen(az_vsub(state->ship.position, pos), -ship_coeff);
    } else {
      drift = az_vwithlen(az_vsub(state->ship.position, pos), ship_coeff);
    }
  }
  AZ_ARRAY_LOOP(door, state->doors) {
    if (door->kind == AZ_DOOR_NOTHING) continue;
    const az_vector_t delta = az_vsub(pos, door->position);
    const double dist = az_vnorm(delta) - AZ_DOOR_BOUNDING_RADIUS -
      baddie->data->overall_bounding_radius;
    if (dist <= 0.0) {
      drift = az_vadd(drift, az_vwithlen(delta, wall_near_coeff));
    } else {
      drift = az_vadd(drift, az_vwithlen(delta, wall_far_coeff * exp(-dist)));
    }
  }
  AZ_ARRAY_LOOP(wall, state->walls) {
    if (wall->kind == AZ_WALL_NOTHING) continue;
    const az_vector_t delta = az_vsub(pos, wall->position);
    const double dist = az_vnorm(delta) - wall->data->bounding_radius -
      baddie->data->overall_bounding_radius;
    if (dist <= 0.0) {
      drift = az_vadd(drift, az_vwithlen(delta, wall_near_coeff));
    } else {
      drift = az_vadd(drift, az_vwithlen(delta, wall_far_coeff * exp(-dist)));
    }
  }
  return drift;
}

static void drift_towards_ship(
    az_space_state_t *state, az_baddie_t *baddie, double time,
    double max_speed, double ship_force, double wall_force) {
  const az_vector_t drift = force_field(state, baddie, ship_force, 0.0, 1000.0,
                                        wall_force, 2.0 * wall_force);
  baddie->velocity =
    az_vcaplen(az_vadd(baddie->velocity, az_vmul(drift, time)), max_speed);
}

static void fly_towards_ship(
    az_space_state_t *state, az_baddie_t *baddie, double time) {
  const double turn_rate = 3.0;
  const double lateral_decel_rate = 20.0;
  const double max_speed = 40.0;
  const double forward_accel = 100.0;
  const double backward_accel = 80.0;
  const az_vector_t drift =
    (baddie->cooldown > 1.0 ?
     force_field(state, baddie, -100.0, 0.0, 500.0, 100.0, 200.0) :
     force_field(state, baddie, 100.0, 100.0, 1000.0, 100.0, 200.0));
  const double goal_theta =
    az_vtheta(baddie->cooldown <= 0.0 &&
              az_vwithin(baddie->position, state->ship.position, 120.0) ?
              az_vsub(state->ship.position, baddie->position) : drift);
  baddie->angle =
    az_angle_towards(baddie->angle, turn_rate * time, goal_theta);
  const az_vector_t unit = az_vpolar(1, baddie->angle);
  const az_vector_t lateral = az_vflatten(baddie->velocity, unit);
  const az_vector_t dvel =
    az_vadd(az_vcaplen(az_vneg(lateral), lateral_decel_rate * time),
            (az_vdot(unit, drift) >= 0.0 ?
             az_vmul(unit, forward_accel * time) :
             az_vmul(unit, -backward_accel * time)));
  baddie->velocity = az_vcaplen(az_vadd(baddie->velocity, dvel), max_speed);
}

/*===========================================================================*/

// How long it takes a baddie's armor flare to die down, in seconds:
#define AZ_BADDIE_ARMOR_FLARE_TIME 0.3
// How long it takes a baddie to unfreeze, in seconds.
#define AZ_BADDIE_THAW_TIME 8.0

static void tick_baddie(az_space_state_t *state, az_baddie_t *baddie,
                        double time) {
  // Allow the armor flare to die down a bit.
  assert(baddie->armor_flare >= 0.0);
  assert(baddie->armor_flare <= 1.0);
  baddie->armor_flare =
    fmax(0.0, baddie->armor_flare - time / AZ_BADDIE_ARMOR_FLARE_TIME);

  // Allow the baddie to thaw a bit.
  assert(baddie->frozen >= 0.0);
  assert(baddie->frozen <= 1.0);
  baddie->frozen = fmax(0.0, baddie->frozen - time / AZ_BADDIE_THAW_TIME);
  if (baddie->frozen > 0.0) {
    baddie->velocity = AZ_VZERO;
    return;
  }

  // Cool down the baddie's weapon.
  baddie->cooldown = fmax(0.0, baddie->cooldown - time);

  // Apply velocity.
  if (az_vnonzero(baddie->velocity)) {
    az_impact_flags_t impact_flags = AZ_IMPF_BADDIE;
    if (state->ship.temp_invincibility > 0.0) impact_flags |= AZ_IMPF_SHIP;
    az_impact_t impact;
    az_circle_impact(state, baddie->data->main_body.bounding_radius,
                     baddie->position, az_vmul(baddie->velocity, time),
                     impact_flags, baddie->uid, &impact);
    baddie->position = impact.position;
    if (impact.type != AZ_IMP_NOTHING) {
      // Push the baddie slightly away from the impact point (so that we're
      // hopefully no longer in contact with the object we hit).
      baddie->position = az_vadd(baddie->position,
                                 az_vwithlen(impact.normal, 0.5));
      // Bounce the baddie off the object.
      baddie->velocity =
        az_vsub(baddie->velocity,
                az_vmul(az_vproj(baddie->velocity, impact.normal), 1.5));
      // If we hit the ship, damage the ship.
      if (impact.type == AZ_IMP_SHIP) {
        az_damage_ship(state, baddie->data->main_body.impact_damage, true);
        // TODO apply force to the ship to push it away
      }
    }
  }

  // Perform kind-specific logic.
  switch (baddie->kind) {
    case AZ_BAD_NOTHING: AZ_ASSERT_UNREACHABLE();
    case AZ_BAD_LUMP:
      baddie->angle = az_mod2pi(baddie->angle + 3.0 * time);
      break;
    case AZ_BAD_TURRET:
    case AZ_BAD_ARMORED_TURRET:
      // Aim gun:
      baddie->components[0].angle =
        fmax(-1.0, fmin(1.0, az_mod2pi(az_angle_towards(
          baddie->angle + baddie->components[0].angle, 2.0 * time,
          az_vtheta(az_vsub(state->ship.position, baddie->position))) -
                                       baddie->angle)));
      // Fire:
      if (baddie->cooldown <= 0.0 &&
          angle_to_ship_within(state, baddie, baddie->components[0].angle,
                               AZ_DEG2RAD(6)) &&
          has_line_of_sight_to_ship(state, baddie)) {
        fire_projectile(state, baddie, AZ_PROJ_GUN_NORMAL, 20.0,
                        baddie->components[0].angle, 0.0);
        baddie->cooldown = 0.5;
      }
      break;
    case AZ_BAD_ZIPPER:
      if (az_vdot(baddie->velocity, az_vpolar(1, baddie->angle)) < 0.0) {
        baddie->angle = az_mod2pi(baddie->angle + AZ_PI);
      }
      baddie->velocity = az_vpolar(200.0, baddie->angle);
      break;
    case AZ_BAD_BOUNCER:
      if (az_vnonzero(baddie->velocity)) {
        baddie->angle = az_vtheta(baddie->velocity);
      }
      baddie->velocity = az_vpolar(150.0, baddie->angle);
      break;
    case AZ_BAD_ATOM:
      drift_towards_ship(state, baddie, time, 70, 100, 100);
      for (int i = 0; i < baddie->data->num_components; ++i) {
        az_component_t *component = &baddie->components[i];
        component->angle = az_mod2pi(component->angle + 3.5 * time);
        component->position = az_vpolar(4.0, component->angle);
        component->position.x *= 5.0;
        component->position =
          az_vrotate(component->position, i * AZ_DEG2RAD(120));
      }
      break;
    case AZ_BAD_SPINER:
      drift_towards_ship(state, baddie, time, 40, 10, 100);
      baddie->angle = az_mod2pi(baddie->angle + 0.4 * time);
      if (baddie->cooldown <= 0.0 &&
          az_vwithin(baddie->position, state->ship.position, 200)) {
        for (int i = 0; i < 360; i += 45) {
          fire_projectile(state, baddie, AZ_PROJ_SPINE,
                          baddie->data->main_body.bounding_radius,
                          AZ_DEG2RAD(i), 0.0);
        }
        baddie->cooldown = 2.0;
      }
      break;
    case AZ_BAD_CLAM:
      {
        const double ship_theta =
          az_vtheta(az_vsub(state->ship.position, baddie->position));
        baddie->angle =
          az_angle_towards(baddie->angle, 0.2 * time, ship_theta);
        const double max_angle = AZ_DEG2RAD(40);
        const double old_angle = baddie->components[0].angle;
        const double new_angle =
          (baddie->cooldown <= 0.0 &&
           fabs(az_mod2pi(baddie->angle - ship_theta)) < AZ_DEG2RAD(10) &&
           has_line_of_sight_to_ship(state, baddie) ?
           fmin(max_angle, max_angle -
                (max_angle - old_angle) * pow(0.00003, time)) :
           fmax(0.0, old_angle - 1.0 * time));
        baddie->components[0].angle = new_angle;
        baddie->components[1].angle = -new_angle;
        if (baddie->cooldown <= 0.0 && new_angle >= 0.95 * max_angle) {
          for (int i = -1; i <= 1; ++i) {
            fire_projectile(state, baddie,
                (i == 0 ? AZ_PROJ_FIREBALL_SLOW : AZ_PROJ_FIREBALL_FAST),
                0.5 * baddie->data->main_body.bounding_radius,
                AZ_DEG2RAD(i * 5), 0.0);
          }
          baddie->cooldown = 3.0;
        }
      }
      break;
    case AZ_BAD_NIGHTBUG:
      fly_towards_ship(state, baddie, time);
      if (baddie->state == 0) {
        baddie->param = fmax(0.0, baddie->param - time / 3.5);
        if (baddie->cooldown < 0.5 &&
            az_vwithin(baddie->position, state->ship.position, 250.0) &&
            angle_to_ship_within(state, baddie, 0, AZ_DEG2RAD(3)) &&
            has_line_of_sight_to_ship(state, baddie)) {
          baddie->state = 1;
        }
      } else {
        assert(baddie->state == 1);
        baddie->param = fmin(1.0, baddie->param + time / 0.5);
        if (baddie->cooldown <= 0.0 && baddie->param == 1.0) {
          for (int i = 0; i < 2; ++i) {
            fire_projectile(state, baddie, AZ_PROJ_FIREBALL_FAST, 20.0, 0.0,
                            az_random(-AZ_DEG2RAD(10), AZ_DEG2RAD(10)));
          }
          baddie->cooldown = 5.0;
          baddie->state = 0;
        }
      }
      break;
    case AZ_BAD_SPINE_MINE:
      drift_towards_ship(state, baddie, time, 20, 20, 20);
      baddie->angle = az_mod2pi(baddie->angle - 0.5 * time);
      if (az_vwithin(baddie->position, state->ship.position, 150.0) &&
          has_line_of_sight_to_ship(state, baddie)) {
        for (int i = 0; i < 360; i += 20) {
          fire_projectile(state, baddie, AZ_PROJ_SPINE,
              baddie->data->main_body.bounding_radius,
              AZ_DEG2RAD(i) + az_random(AZ_DEG2RAD(-10), AZ_DEG2RAD(10)), 0.0);
        }
        baddie->kind = AZ_BAD_NOTHING;
      }
      break;
    case AZ_BAD_BROKEN_TURRET:
      if (baddie->state == 2) {
        baddie->components[0].angle -= 5.0 * time;
        if (baddie->components[0].angle <= -1.0) {
          baddie->components[0].angle = -1.0;
          baddie->state = 0;
        }
      } else if (baddie->state == 1) {
        baddie->components[0].angle += 5.0 * time;
        if (baddie->components[0].angle >= 1.0) {
          baddie->components[0].angle = 1.0;
          baddie->state = 0;
        }
      } else {
        assert(baddie->state == 0);
        // Try to aim gun (but sometimes twitch randomly):
        const int aim = az_randint(-1, 1);
        baddie->components[0].angle = fmax(-1.0, fmin(1.0, az_mod2pi(
            (aim == 0 ?
             az_angle_towards(
                baddie->angle + baddie->components[0].angle, 2 * time,
                az_vtheta(az_vsub(state->ship.position, baddie->position))) -
               baddie->angle :
             baddie->components[0].angle + aim * 2 * time))));
        // Fire:
        if (baddie->cooldown <= 0.0 &&
            angle_to_ship_within(state, baddie, baddie->components[0].angle,
                                 AZ_DEG2RAD(6)) &&
            has_line_of_sight_to_ship(state, baddie)) {
          fire_projectile(state, baddie, AZ_PROJ_GUN_NORMAL, 20.0,
                          baddie->components[0].angle, 0.0);
          baddie->cooldown = 1.0;
        }
        // Randomly go crazy:
        if (az_random(0.0, 2.5) < time) {
          baddie->state = az_randint(1, 2);
          const az_vector_t spark_start =
            az_vadd(baddie->position,
                    az_vpolar(20, baddie->angle +
                              baddie->components[0].angle));
          const double spark_angle =
            baddie->angle + baddie->components[0].angle +
            (baddie->state == 1 ? -AZ_DEG2RAD(65) : AZ_DEG2RAD(65));
          for (int i = 0; i < 8; ++i) {
            const double theta =
              spark_angle + az_random(-AZ_DEG2RAD(25), AZ_DEG2RAD(25));
            az_add_speck(state, (az_color_t){255, 200, 100, 255}, 4.0,
                         spark_start, az_vpolar(az_random(10, 70), theta));
          }
        }
      }
      break;
    case AZ_BAD_ZENITH_CORE:
      // Lie dormant for 31 seconds.
      if (baddie->state == 0) {
        baddie->state = 1;
        baddie->cooldown = 31.0;
      }
      if (baddie->cooldown <= 0.0) {
        // Turn towards the ship.
        baddie->angle = az_angle_towards(
            baddie->angle, AZ_DEG2RAD(30) * time,
            az_vtheta(az_vsub(state->ship.position, baddie->position)));
        // Fire a beam, piercing through the ship and other baddies.
        const az_vector_t beam_start =
          az_vadd(baddie->position, az_vpolar(120, baddie->angle));
        az_impact_t impact;
        az_ray_impact(state, beam_start, az_vpolar(1000, baddie->angle),
                      (AZ_IMPF_BADDIE | AZ_IMPF_SHIP), baddie->uid, &impact);
        const az_vector_t beam_delta = az_vsub(impact.position, beam_start);
        const double beam_damage = 100.0 * time;
        // Damage the ship and any baddies within the beam.
        if (az_ship_is_present(&state->ship) &&
            az_ray_hits_ship(&state->ship, beam_start, beam_delta,
                             NULL, NULL)) {
          az_damage_ship(state, beam_damage, false);
          az_loop_sound(&state->soundboard, AZ_SND_BEAM_NORMAL);
          az_loop_sound(&state->soundboard, AZ_SND_BEAM_PHASE);
        }
        AZ_ARRAY_LOOP(other, state->baddies) {
          if (other->kind == AZ_BAD_NOTHING || other == baddie) continue;
          const az_component_data_t *component;
          if (az_ray_hits_baddie(other, beam_start, beam_delta,
                                 NULL, NULL, &component)) {
            az_try_damage_baddie(state, other, component, AZ_DMGF_PIERCE,
                                 beam_damage);
          }
        }
        // Add particles for the beam.
        az_particle_t *particle;
        if (az_insert_particle(state, &particle)) {
          particle->kind = AZ_PAR_BEAM;
          particle->color = (az_color_t){
            (az_clock_mod(6, 1, state->clock) < 3 ? 255 : 64),
            (az_clock_mod(6, 1, state->clock + 2) < 3 ? 255 : 64),
            (az_clock_mod(6, 1, state->clock + 4) < 3 ? 255 : 64), 192};
          particle->position = beam_start;
          particle->velocity = AZ_VZERO;
          particle->angle = baddie->angle;
          particle->lifetime = 0.0;
          particle->param1 =
            az_vnorm(az_vsub(impact.position, particle->position));
          particle->param2 = 6 + az_clock_zigzag(6, 1, state->clock);
          for (int i = 0; i < 5; ++i) {
            az_add_speck(state, particle->color, 1.0, impact.position,
                         az_vpolar(az_random(20.0, 70.0),
                                   az_vtheta(impact.normal) +
                                   az_random(-AZ_HALF_PI, AZ_HALF_PI)));
          }
        }
      }
      break;
    case AZ_BAD_BOX:
    case AZ_BAD_ARMORED_BOX:
      // Do nothing.
      break;
  }
}

void az_tick_baddies(az_space_state_t *state, double time) {
  AZ_ARRAY_LOOP(baddie, state->baddies) {
    if (baddie->kind == AZ_BAD_NOTHING) continue;
    assert(baddie->health > 0.0);
    tick_baddie(state, baddie, time);
  }
}

/*===========================================================================*/
