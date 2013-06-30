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

#include "azimuth/view/hud.h"

#include <assert.h>
#include <math.h>

#include <GL/gl.h>

#include "azimuth/constants.h"
#include "azimuth/state/dialog.h"
#include "azimuth/state/ship.h"
#include "azimuth/state/space.h"
#include "azimuth/state/upgrade.h"
#include "azimuth/util/clock.h"
#include "azimuth/util/misc.h"
#include "azimuth/util/vector.h"
#include "azimuth/view/dialog.h"
#include "azimuth/view/string.h"

/*===========================================================================*/

#define HUD_MARGIN 2
#define HUD_PADDING 4
#define HUD_BAR_HEIGHT 9

static void draw_hud_bar(float left, float top, float cur, float max) {
  // Draw bar:
  glBegin(GL_QUADS);
  glVertex2f(left, top);
  glVertex2f(left, top + HUD_BAR_HEIGHT);
  glVertex2f(left + cur, top + HUD_BAR_HEIGHT);
  glVertex2f(left + cur, top);
  glEnd();
  // Draw outline:
  glColor3f(1, 1, 1); // white
  glBegin(GL_LINE_LOOP);
  glVertex2f(left + 0.5f, top + 0.5f);
  glVertex2f(left + 0.5f, top + HUD_BAR_HEIGHT + 0.5f);
  glVertex2f(left + max + 0.5f, top + HUD_BAR_HEIGHT + 0.5f);
  glVertex2f(left + max + 0.5f, top + 0.5f);
  glEnd();
}

static void draw_hud_shields_energy(const az_player_t *player,
                                    az_clock_t clock) {
  const int max_power = (player->max_shields > player->max_energy ?
                         player->max_shields : player->max_energy);
  const int height = 25 + 2 * HUD_PADDING;
  const int width = 50 + 2 * HUD_PADDING + max_power;

  glPushMatrix(); {
    glTranslatef(HUD_MARGIN, HUD_MARGIN, 0);

    glColor4f(0, 0, 0, 0.75); // tinted-black
    glBegin(GL_QUADS);
    glVertex2i(0, 0);
    glVertex2i(0, height);
    glVertex2i(width, height);
    glVertex2i(width, 0);
    glEnd();

    glTranslatef(HUD_PADDING, HUD_PADDING, 0);

    glColor3f(1, 1, 1); // white
    az_draw_string(8, AZ_ALIGN_LEFT, 0, 1, "SHIELD");
    az_draw_string(8, AZ_ALIGN_LEFT, 0, 16, "ENERGY");

    if (player->shields <= AZ_SHIELDS_VERY_LOW_THRESHOLD) {
      if (az_clock_mod(2, 3, clock)) glColor3f(1, 0, 0);
      else glColor3f(0.2, 0, 0);
    } else if (player->shields <= AZ_SHIELDS_LOW_THRESHOLD) {
      glColor3f(0.5 + 0.1 * az_clock_zigzag(6, 5, clock), 0, 0);
    } else glColor3f(0, 0.75, 0.75); // cyan
    draw_hud_bar(50, 0, player->shields, player->max_shields);
    glColor3f(0.75, 0, 0.75); // magenta
    draw_hud_bar(50, 15, player->energy, player->max_energy);
  } glPopMatrix();
}

/*===========================================================================*/

static void set_gun_color(az_gun_t gun) {
  switch (gun) {
    case AZ_GUN_NONE: AZ_ASSERT_UNREACHABLE();
    case AZ_GUN_CHARGE: glColor3f(1, 1, 1); break;
    case AZ_GUN_FREEZE: glColor3f(0, 1, 1); break;
    case AZ_GUN_TRIPLE: glColor3f(0, 1, 0); break;
    case AZ_GUN_HOMING: glColor3f(0, 0.375, 1); break;
    case AZ_GUN_PHASE:  glColor3f(1, 1, 0); break;
    case AZ_GUN_BURST:  glColor3f(0.75, 0.375, 0); break;
    case AZ_GUN_PIERCE: glColor3f(1, 0, 1); break;
    case AZ_GUN_BEAM:   glColor3f(1, 0, 0); break;
  }
}

static void draw_hud_gun_name(float left, float top, az_gun_t gun) {
  if (gun != AZ_GUN_NONE) {
    glColor3f(1, 1, 1);
    glBegin(GL_LINE_STRIP);
    glVertex2f(left + 3.5f, top + 0.5f);
    glVertex2f(left + 0.5f, top + 0.5f);
    glVertex2f(left + 0.5f, top + 10.5f);
    glVertex2f(left + 3.5f, top + 10.5f);
    glEnd();
    glBegin(GL_LINE_STRIP);
    glVertex2f(left + 51.5f, top + 0.5f);
    glVertex2f(left + 54.5f, top + 0.5f);
    glVertex2f(left + 54.5f, top + 10.5f);
    glVertex2f(left + 51.5f, top + 10.5f);
    glEnd();

    set_gun_color(gun);
    const char *name = az_gun_name(gun);
    az_draw_string(8, AZ_ALIGN_CENTER, left + 28, top + 2, name);
  }
}

static void draw_hud_ordnance(float left, float top, bool is_rockets,
                              int cur, int max, az_ordnance_t selected) {
  // Draw nothing if the player doesn't have this kind of ordnance yet.
  if (max <= 0) return;

  // Draw the selection indicator.
  if ((is_rockets && selected == AZ_ORDN_ROCKETS) ||
      (!is_rockets && selected == AZ_ORDN_BOMBS)) {
    glColor3f(1, 1, 1);
    glBegin(GL_LINE_STRIP);
    glVertex2f(left + 3.5f, top + 0.5f);
    glVertex2f(left + 0.5f, top + 0.5f);
    glVertex2f(left + 0.5f, top + 10.5f);
    glVertex2f(left + 3.5f, top + 10.5f);
    glEnd();
    glBegin(GL_LINE_STRIP);
    glVertex2f(left + 51.5f, top + 0.5f);
    glVertex2f(left + 54.5f, top + 0.5f);
    glVertex2f(left + 54.5f, top + 10.5f);
    glVertex2f(left + 51.5f, top + 10.5f);
    glEnd();
  }

  // Draw quantity string.
  if (cur >= max) glColor3f(1, 1, 0);
  else glColor3f(1, 1, 1);
  az_draw_printf(8, AZ_ALIGN_RIGHT, left + 32, top + 2, "%d", cur);

  // Draw icon.
  if (is_rockets) {
    glColor3f(1, 0.25, 0.25);
    az_draw_string(8, AZ_ALIGN_LEFT, left + 36, top + 2, "R");
  } else {
    glColor3f(0.25, 0.25, 1);
    az_draw_string(8, AZ_ALIGN_LEFT, left + 36, top + 2, "B");
  }
}

static void draw_hud_weapons_selection(const az_player_t *player) {
  const bool wide = player->max_rockets > 0 || player->max_bombs > 0;
  if (player->gun1 == AZ_GUN_NONE && !wide) return;
  const bool tall = player->gun2 != AZ_GUN_NONE || player->max_bombs > 0;

  const int height = (tall ? 25 : 10) + 2 * HUD_PADDING;
  const int width = (wide ? 115 : 55) + 2 * HUD_PADDING;
  glPushMatrix(); {
    glTranslatef(AZ_SCREEN_WIDTH - HUD_MARGIN - width, HUD_MARGIN, 0);

    glColor4f(0, 0, 0, 0.75); // tinted-black
    glBegin(GL_QUADS);
    glVertex2i(0, 0);
    glVertex2i(0, height);
    glVertex2i(width, height);
    glVertex2i(width, 0);
    glEnd();

    glTranslatef(HUD_PADDING, HUD_PADDING, 0);

    draw_hud_gun_name(0, 0, player->gun1);
    draw_hud_gun_name(0, 15, player->gun2);
    draw_hud_ordnance(60, 0, true, player->rockets, player->max_rockets,
                      player->ordnance);
    draw_hud_ordnance(60, 15, false, player->bombs, player->max_bombs,
                      player->ordnance);
  } glPopMatrix();
}

/*===========================================================================*/

#define BOSS_BAR_WIDTH 400

static void draw_hud_boss_health(az_space_state_t *state) {
  az_baddie_t *baddie;
  if (az_lookup_baddie(state, state->boss_uid, &baddie)) {
    glPushMatrix(); {
      glTranslatef(HUD_MARGIN, AZ_SCREEN_HEIGHT - HUD_MARGIN -
                   2 * HUD_PADDING - 10, 0);

      const int height = 10 + 2 * HUD_PADDING;
      const int width = 2 * HUD_PADDING + 35 + BOSS_BAR_WIDTH;
      glColor4f(0, 0, 0, 0.75); // tinted-black
      glBegin(GL_QUADS);
      glVertex2i(0, 0);
      glVertex2i(0, height);
      glVertex2i(width, height);
      glVertex2i(width, 0);
      glEnd();

      glTranslatef(HUD_PADDING, HUD_PADDING, 0);

      glColor3f(1, 1, 1); // white
      az_draw_string(8, AZ_ALIGN_LEFT, 0, 1, "BOSS");
      assert(baddie->health > 0.0);
      assert(baddie->data->max_health > 0.0);
      glColor3f(1, 0.25, 0); // red-orange
      draw_hud_bar(35, 0, (baddie->health / baddie->data->max_health) *
                   BOSS_BAR_WIDTH, BOSS_BAR_WIDTH);
    } glPopMatrix();
  }
}

static void draw_hud_message(const az_preferences_t *prefs,
                             const az_message_t *message) {
  if (message->time_remaining <= 0.0) {
    assert(message->paragraph == NULL);
    assert(message->num_lines == 0);
    return;
  }
  assert(message->paragraph != NULL);
  assert(message->num_lines > 0);
  glPushMatrix(); {
    const int width = AZ_SCREEN_WIDTH - 20;
    const int height = 6 + 20 * message->num_lines;
    const double slide_time = 0.5; // seconds
    const int top = AZ_SCREEN_HEIGHT -
      (message->time_remaining >= slide_time ? height :
       (int)((double)height * (message->time_remaining / slide_time)));
    glTranslatef((AZ_SCREEN_WIDTH - width) / 2, top, 0);

    glColor4f(0.1, 0.1, 0.1, 0.9); // dark gray tint
    glBegin(GL_QUADS); {
      glVertex2i(0, 0); glVertex2i(0, height);
      glVertex2i(width, height); glVertex2i(width, 0);
    } glEnd();

    az_draw_paragraph(16, AZ_ALIGN_CENTER, width/2, 6, 20, -1,
                      prefs, message->paragraph);
  } glPopMatrix();
}

static void draw_hud_countdown(const az_countdown_t *countdown,
                               az_clock_t clock) {
  if (!countdown->is_active) return;
  glPushMatrix(); {
    const int width = 2 * HUD_PADDING + 7 * 24 - 3;
    const int height = 2 * HUD_PADDING + 20;

    const int xstart = AZ_SCREEN_WIDTH/2 - width/2;
    const int ystart = AZ_SCREEN_HEIGHT/2 + 75 - height/2;
    const int xend = AZ_SCREEN_WIDTH - HUD_MARGIN - width;
    const int yend = AZ_SCREEN_HEIGHT - HUD_MARGIN - height;
    const double speed = 150.0; // pixels per second
    const double offset = fmax(0.0, countdown->active_for - 2.5) * speed;
    glTranslatef(az_imin(xend, xstart + offset),
                 az_imin(yend, ystart + offset), 0);

    glColor4f(0, 0, 0, 0.75); // tinted-black
    glBegin(GL_QUADS); {
      glVertex2i(0, 0);
      glVertex2i(0, height);
      glVertex2i(width, height);
      glVertex2i(width, 0);
    } glEnd();

    assert(countdown->time_remaining >= 0.0);
    if (countdown->time_remaining >= 10.0) {
      glColor3f(1, 1, 1); // white
    } else {
      if (az_clock_mod(2, 3, clock) == 0) glColor3f(1, 1, 0); // yellow
      else glColor3f(1, 0, 0); // red
    }
    const int minutes = az_imin(9, ((int)countdown->time_remaining) / 60);
    const int seconds = ((int)countdown->time_remaining) % 60;
    const int jiffies = ((int)(countdown->time_remaining * 100)) % 100;
    az_draw_printf(24, AZ_ALIGN_LEFT, HUD_PADDING, HUD_PADDING,
                   "%d:%02d:%02d", minutes, seconds, jiffies);
  } glPopMatrix();
}

/*===========================================================================*/

static void draw_box(double left, double top, double width, double height) {
  glColor4f(0, 0, 0, 0.875); // black tint
  glBegin(GL_QUADS); {
    glVertex2d(left, top);
    glVertex2d(left + width, top);
    glVertex2d(left + width, top + height);
    glVertex2d(left, top + height);
  } glEnd();
  glColor3f(0, 1, 0); // green
  glBegin(GL_LINE_LOOP); {
    glVertex2d(left, top);
    glVertex2d(left + width, top);
    glVertex2d(left + width, top + height);
    glVertex2d(left, top + height);
  } glEnd();
}

/*===========================================================================*/

#define TEXT_LINE_SPACING 12
#define DIALOG_BOX_WIDTH 404
#define DIALOG_BOX_HEIGHT 116
#define DIALOG_BOX_MARGIN 10
#define PORTRAIT_BOX_WIDTH 150
#define PORTRAIT_BOX_HEIGHT 150
#define PORTRAIT_BOX_MARGIN 5
#define DIALOG_HORZ_SPACING 20
#define DIALOG_VERT_SPACING 50

static void draw_dialog_frames(double openness) {
  assert(openness >= 0.0);
  assert(openness <= 1.0);
  { // Portraits:
    const double sw = 0.5 * PORTRAIT_BOX_WIDTH * openness;
    const double sh = 0.5 * PORTRAIT_BOX_HEIGHT * openness;
    // Top portrait:
    const int tcx =
      (AZ_SCREEN_WIDTH - DIALOG_HORZ_SPACING - DIALOG_BOX_WIDTH) / 2;
    const int tcy =
      (AZ_SCREEN_HEIGHT - DIALOG_VERT_SPACING - PORTRAIT_BOX_HEIGHT) / 2;
    draw_box(tcx - sw, tcy - sh, sw * 2, sh * 2);
    // Bottom portrait:
    const int bcx =
      (AZ_SCREEN_WIDTH + DIALOG_HORZ_SPACING + DIALOG_BOX_WIDTH) / 2;
    const int bcy =
      (AZ_SCREEN_HEIGHT + DIALOG_VERT_SPACING + PORTRAIT_BOX_HEIGHT) / 2;
    draw_box(bcx - sw, bcy - sh, sw * 2, sh * 2);
  }
  { // Dialog boxes:
    const double sw = 0.5 * DIALOG_BOX_WIDTH * openness;
    const double sh = 0.5 * DIALOG_BOX_HEIGHT * openness;
    // Top dialog box:
    const int tcx =
      (AZ_SCREEN_WIDTH + DIALOG_HORZ_SPACING + PORTRAIT_BOX_WIDTH) / 2;
    const int tcy =
      (AZ_SCREEN_HEIGHT - DIALOG_VERT_SPACING - DIALOG_BOX_HEIGHT) / 2;
    draw_box(tcx - sw, tcy - sh, sw * 2, sh * 2);
    // Bottom dialog box:
    const int bcx =
      (AZ_SCREEN_WIDTH - DIALOG_HORZ_SPACING - PORTRAIT_BOX_WIDTH) / 2;
    const int bcy =
      (AZ_SCREEN_HEIGHT + DIALOG_VERT_SPACING + DIALOG_BOX_HEIGHT) / 2;
    draw_box(bcx - sw, bcy - sh, sw * 2, sh * 2);
  }
}

static void draw_dialog_text(const az_space_state_t *state) {
  assert(state->mode == AZ_MODE_DIALOG);
  const az_dialog_mode_data_t *mode_data = &state->dialog_mode;
  glPushMatrix(); {
    if (!mode_data->bottom_next) {
      glTranslatef((AZ_SCREEN_WIDTH + DIALOG_HORZ_SPACING - DIALOG_BOX_WIDTH +
                    PORTRAIT_BOX_WIDTH) / 2 + DIALOG_BOX_MARGIN,
                   (AZ_SCREEN_HEIGHT - DIALOG_VERT_SPACING) / 2 -
                   DIALOG_BOX_HEIGHT + DIALOG_BOX_MARGIN, 0);
    } else {
      glTranslatef((AZ_SCREEN_WIDTH - DIALOG_HORZ_SPACING - DIALOG_BOX_WIDTH -
                    PORTRAIT_BOX_WIDTH) / 2 + DIALOG_BOX_MARGIN,
                   (AZ_SCREEN_HEIGHT + DIALOG_VERT_SPACING) / 2 +
                   DIALOG_BOX_MARGIN, 0);
    }

    if (mode_data->step == AZ_DLS_WAIT) {
      if (az_clock_mod(2, 15, state->clock)) glColor4f(0.5, 0.5, 0.5, 0.5);
      else glColor4f(0.25, 0.75, 0.75, 0.5);
      az_draw_string(8, AZ_ALIGN_RIGHT,
                     DIALOG_BOX_WIDTH - 2 * DIALOG_BOX_MARGIN,
                     DIALOG_BOX_HEIGHT - 2 * DIALOG_BOX_MARGIN - 8, "[ENTER]");
    }

    az_draw_paragraph(8, AZ_ALIGN_LEFT, 0, 0, TEXT_LINE_SPACING,
                      mode_data->chars_to_print, state->prefs,
                      mode_data->paragraph);
  } glPopMatrix();
}

static void draw_dialog_portrait(az_portrait_t portrait, bool is_bottom,
                                 bool talking, az_clock_t clock) {
  if (portrait == AZ_POR_NOTHING) return;
  glPushMatrix(); {
    const GLfloat x_scale =
      (PORTRAIT_BOX_WIDTH - 2 * PORTRAIT_BOX_MARGIN) / 100.0;
    const GLfloat y_scale =
      (PORTRAIT_BOX_HEIGHT - 2 * PORTRAIT_BOX_MARGIN) / 100.0;
    if (!is_bottom) {
      glTranslatef((AZ_SCREEN_WIDTH - DIALOG_HORZ_SPACING - DIALOG_BOX_WIDTH -
                    PORTRAIT_BOX_WIDTH) / 2 + PORTRAIT_BOX_MARGIN,
                   (AZ_SCREEN_HEIGHT - DIALOG_VERT_SPACING) / 2 -
                   PORTRAIT_BOX_MARGIN, 0);
      glScalef(x_scale, -y_scale, 1);
    } else {
      glTranslatef((AZ_SCREEN_WIDTH + DIALOG_HORZ_SPACING + DIALOG_BOX_WIDTH -
                    PORTRAIT_BOX_WIDTH) / 2 + PORTRAIT_BOX_WIDTH -
                   PORTRAIT_BOX_MARGIN,
                   (AZ_SCREEN_HEIGHT + DIALOG_VERT_SPACING) / 2 +
                   PORTRAIT_BOX_HEIGHT - PORTRAIT_BOX_MARGIN, 0);
      glScalef(-x_scale, -y_scale, 1);
    }
    az_draw_portrait(portrait, talking, clock);
  } glPopMatrix();
}

void az_draw_dialog(const az_space_state_t *state) {
  assert(state->mode == AZ_MODE_DIALOG);
  const az_dialog_mode_data_t *mode_data = &state->dialog_mode;
  bool talking = false;
  const bool bottom_next = mode_data->bottom_next;
  switch (mode_data->step) {
    case AZ_DLS_BEGIN:
      draw_dialog_frames(mode_data->progress);
      break;
    case AZ_DLS_TALK:
      talking = true;
      // fallthrough
    case AZ_DLS_WAIT:
      draw_dialog_frames(1.0);
      draw_dialog_text(state);
      draw_dialog_portrait(mode_data->top, false, talking && !bottom_next,
                           state->clock);
      draw_dialog_portrait(mode_data->bottom, true, talking && bottom_next,
                           state->clock);
      break;
    case AZ_DLS_END:
      draw_dialog_frames(1.0 - mode_data->progress);
      break;
  }
}

/*===========================================================================*/

#define UPGRADE_BOX_WIDTH 500
#define UPGRADE_BOX_HEIGHT 94

static void draw_upgrade_box_frame(double openness) {
  assert(openness >= 0.0);
  assert(openness <= 1.0);
  const double width = sqrt(openness) * UPGRADE_BOX_WIDTH;
  const double height = openness * openness * UPGRADE_BOX_HEIGHT;
  const double left = 0.5 * (AZ_SCREEN_WIDTH - width);
  const double top = 0.5 * (AZ_SCREEN_HEIGHT - height);
  draw_box(left, top, width, height);
}

static void draw_upgrade_box_message(const az_preferences_t *prefs,
                                     az_upgrade_t upgrade) {
  const char *name = az_upgrade_name(upgrade);
  const char *description = az_upgrade_description(upgrade);
  const double top = 0.5 * (AZ_SCREEN_HEIGHT - UPGRADE_BOX_HEIGHT);
  glColor3f(1, 1, 1); // white
  az_draw_string(16, AZ_ALIGN_CENTER, AZ_SCREEN_WIDTH/2, top + 18, name);
  az_draw_paragraph(8, AZ_ALIGN_CENTER, AZ_SCREEN_WIDTH/2, top + 48, 20, -1,
                    prefs, description);
}

static void draw_upgrade_box(const az_space_state_t *state) {
  assert(state->mode == AZ_MODE_UPGRADE);
  const az_upgrade_mode_data_t *mode_data = &state->upgrade_mode;
  switch (mode_data->step) {
    case AZ_UGS_OPEN:
      draw_upgrade_box_frame(mode_data->progress);
      break;
    case AZ_UGS_MESSAGE:
      draw_upgrade_box_frame(1.0);
      draw_upgrade_box_message(state->prefs, mode_data->upgrade);
      break;
    case AZ_UGS_CLOSE:
      draw_upgrade_box_frame(1.0 - mode_data->progress);
      break;
  }
}

/*===========================================================================*/

void az_draw_hud(az_space_state_t *state) {
  const az_ship_t *ship = &state->ship;
  if (!az_ship_is_present(ship)) return;
  const az_player_t *player = &ship->player;
  draw_hud_shields_energy(player, state->clock);
  draw_hud_weapons_selection(player);
  draw_hud_boss_health(state);
  draw_hud_message(state->prefs, &state->message);
  draw_hud_countdown(&state->countdown, state->clock);
  if (state->mode == AZ_MODE_DIALOG) {
    az_draw_dialog(state);
  } else if (state->mode == AZ_MODE_UPGRADE) {
    draw_upgrade_box(state);
  }
}

/*===========================================================================*/
