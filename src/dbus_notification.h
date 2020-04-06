// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
/* dbus_notification.h - D-Bus Status Notification
 *
 * Copyright (C) 2020 Tucker Kern
 *
 * This file is part of GMediaRender.
 *
 * GMediaRender is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GMediaRender is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GMediaRender; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#ifndef _DBUS_NOTIFICATION_H
#define _DBUS_NOTIFICATION_H

#include <gio/gio.h>
#include <glib.h>

#include <algorithm>
#include <map>
#include <string>

#include "logging.h"
#include "upnp_control.h"
#include "upnp_transport.h"
#include "mpris_interface.h"

class DBusNotification {
 public:
  static void Configure(const char *uuid) {
    // Replace '-' in UUID string for D-Bus compat
    std::string safe_uuid(uuid);
    std::replace(safe_uuid.begin(), safe_uuid.end(), '-', '_');

    // Construct a unique name for this instance
    std::string name = "org.mpris.MediaPlayer2.gmediarender.uuid" + safe_uuid;

    media_player_ = nullptr;
    player_ = nullptr;

    // TODO free later
    g_bus_own_name(G_BUS_TYPE_SYSTEM, name.c_str(), G_BUS_NAME_OWNER_FLAGS_REPLACE, BusAcquired, NameAcquired, NameLost, NULL, NULL);

    // Update player object when state changes
    upnp_transport_register_variable_listener(
        [](int, const std::string &var_name,
                             const std::string &,
                             const std::string &new_value) {
          if (var_name != "TransportState") return;

          if (player_ == nullptr) return;

          media_player2_player_set_playback_status(player_, transport_state_map_[new_value].c_str());
        });

    upnp_control_register_variable_listener(
        [](int, const std::string &var_name,
                             const std::string &,
                             const std::string &new_value) {
          if (var_name != "Volume") return;

          if (player_ == nullptr) return;

          media_player2_player_set_volume(player_, std::stod(new_value)/100);
        });
  }

 private:
  static constexpr const char *TAG_ = "dbus";

  static constexpr const char *mpris_path_ = "/org/mpris/MediaPlayer2";
  static std::map<const std::string, const std::string> transport_state_map_;

  static MediaPlayer2* media_player_;
  static MediaPlayer2Player* player_;

  static void BusAcquired(GDBusConnection* connection, const gchar* name, gpointer user_data)
  {
    Log_info(TAG_, "Acquired bus. Exporting MPRIS objects.");

    // Construct the MediaPlayer2 interface
    media_player_ = media_player2_skeleton_new();

    // We won't accept any quit, raise or fullscreen commands
    media_player2_set_can_quit(media_player_, false);
    media_player2_set_can_raise(media_player_, false);
    media_player2_set_can_set_fullscreen(media_player_, false);
    
    // Set the initial state
    media_player2_set_has_track_list(media_player_, false);
    media_player2_set_fullscreen(media_player_, false);
    
    // TODO Technically we know enough to fill these, but we don't care really
    media_player2_set_supported_uri_schemes(media_player_, NULL);
    media_player2_set_supported_mime_types(media_player_, NULL);

    // Set a friendly name
    // TODO Fetch assigned name from cmd
    media_player2_set_identity(media_player_, "GmediaRender");

    // Export this interface on the bus
    g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(media_player_), connection, mpris_path_, NULL);

    // Construct the MediaPlayer2.Player interface
    player_ = media_player2_player_skeleton_new();

    // We won't accept any control inputs
    media_player2_player_set_can_control(player_, false);
    media_player2_player_set_can_go_next(player_, false);
    media_player2_player_set_can_go_previous(player_, false);
    media_player2_player_set_can_play(player_, false);
    media_player2_player_set_can_pause(player_, false);
    media_player2_player_set_can_seek(player_, false);
    
    // Set initial state
    media_player2_player_set_playback_status(player_, "Stopped");
    media_player2_player_set_position(player_, 0);
    //media_player2_player_set_loop_status(player_, "None"); // Optional
    media_player2_player_set_rate(player_, 1.0);
    media_player2_player_set_minimum_rate(player_, 1.0);
    media_player2_player_set_maximum_rate(player_, 1.0);
    //media_player2_player_set_shuffle(player_, false); // Optional
    media_player2_player_set_volume(player_, 1.0); // TODO Get inital volume

    // Export interface on the bus
    g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(player_), connection, mpris_path_, NULL);
  }

  static void NameAcquired(GDBusConnection* connection, const gchar* name, gpointer user_data)
  {
    Log_info(TAG_, "Acquired '%s' on D-Bus", name);
  }

  static void NameLost(GDBusConnection* connection, const gchar* name, gpointer user_data)
  {
    Log_warn(TAG_, "Lost '%s' on D-Bus", name);
  }

};

#endif