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

    // TODO free later
    g_bus_own_name(G_BUS_TYPE_SYSTEM, name.c_str(), G_BUS_NAME_OWNER_FLAGS_REPLACE, BusAcquired, NameAcquired, NameLost, NULL, NULL);

    // Update player object when state changes
    upnp_transport_register_variable_listener(
        [](int, const std::string &var_name,
                             const std::string &,
                             const std::string &new_value) {
          if (var_name != "TransportState") return;

          media_player_.SetPlaybackStatus(transport_state_map_[new_value]);
        });

    upnp_control_register_variable_listener(
        [](int, const std::string &var_name,
                             const std::string &,
                             const std::string &new_value) {
          if (var_name != "Volume") return;

          media_player_.SetVolume(std::stod(new_value)/100);
        });
  }

 private:
  class MediaPlayer2
  {
    public:
    void Export(GDBusConnection* connection, const char* path = DBusNotification::mpris_path_)
    {
      // Export this interface on the bus
      g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(mpris_media_player_), connection, path, NULL);
      g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON((MprisMediaPlayer2Player*)player_), connection, path, NULL);
    }

    void SetPlaybackStatus(const std::string& status)
    {
      mpris_media_player2_player_set_playback_status(player_, status.c_str());
    }

    void SetVolume(double volume)
    {
      mpris_media_player2_player_set_volume(player_, volume);
    }

    MediaPlayer2() : player_()
    {
      // Construct the MediaPlayer2 interface
      mpris_media_player_ = mpris_media_player2_skeleton_new();

      // We won't accept any quit, raise or fullscreen commands
      mpris_media_player2_set_can_quit(mpris_media_player_, false);
      mpris_media_player2_set_can_raise(mpris_media_player_, false);
      mpris_media_player2_set_can_set_fullscreen(mpris_media_player_, false);

      // Set the initial state
      mpris_media_player2_set_has_track_list(mpris_media_player_, false);
      mpris_media_player2_set_fullscreen(mpris_media_player_, false);

      // TODO Technically we know enough to fill these, but we don't care really
      mpris_media_player2_set_supported_uri_schemes(mpris_media_player_, NULL);
      mpris_media_player2_set_supported_mime_types(mpris_media_player_, NULL);

      // Set a friendly name
      // TODO Fetch assigned name from cmd
      mpris_media_player2_set_identity(mpris_media_player_, "GmediaRender");
    }

    MediaPlayer2(const MediaPlayer2&) = delete;  // Delete copy constructor

  private:
   class Player
    {
      public:
      Player()
      {
        // Construct the MediaPlayer2.Player interface
        mpris_player_ = mpris_media_player2_player_skeleton_new();

        // We won't accept any control inputs
        mpris_media_player2_player_set_can_control(mpris_player_, false);
        mpris_media_player2_player_set_can_go_next(mpris_player_, false);
        mpris_media_player2_player_set_can_go_previous(mpris_player_, false);
        mpris_media_player2_player_set_can_play(mpris_player_, false);
        mpris_media_player2_player_set_can_pause(mpris_player_, false);
        mpris_media_player2_player_set_can_seek(mpris_player_, false);

        // Set initial state
        mpris_media_player2_player_set_playback_status(mpris_player_, "Stopped");
        mpris_media_player2_player_set_position(mpris_player_, 0);
        //mpris_media_player2_player_set_loop_status(player_, "None"); // Optional
        mpris_media_player2_player_set_rate(mpris_player_, 1.0);
        mpris_media_player2_player_set_minimum_rate(mpris_player_, 1.0);
        mpris_media_player2_player_set_maximum_rate(mpris_player_, 1.0);
        //mpris_media_player2_player_set_shuffle(player_, false); // Optional
        mpris_media_player2_player_set_volume(mpris_player_, 1.0); // TODO Get inital volume
      }

      Player(const Player&) = delete;  // Delete copy constructor

      operator MprisMediaPlayer2Player*()
      {
        return mpris_player_;
      }

      private:
        MprisMediaPlayer2Player* mpris_player_;
    };

    Player player_;
    
    MprisMediaPlayer2* mpris_media_player_;
  };

  static constexpr const char *TAG_ = "dbus";

  static constexpr const char *mpris_path_ = "/org/mpris/MediaPlayer2";
  static std::map<const std::string, const std::string> transport_state_map_;

  static MediaPlayer2 media_player_;

  static void BusAcquired(GDBusConnection* connection, const gchar* name, gpointer user_data)
  {
    Log_info(TAG_, "Acquired bus. Exporting MPRIS objects.");

    media_player_.Export(connection);
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