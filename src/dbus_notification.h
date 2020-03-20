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

class DBusNotification {
 public:
  static void Configure(const char *uuid) {
    // Replace '-' in UUID string for D-Bus compat
    std::string safe_uuid(uuid);
    std::replace(safe_uuid.begin(), safe_uuid.end(), '-', '_');

    // Construct an object path for this instance
    std::string object = "/com/hzeller/gmedia_resurrect/" + safe_uuid;

    // Connect to the system bus
    GDBusConnection *connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    if (connection == NULL) {
      Log_error(TAG_, "Failed to connect to system D-Bus.");
      return;
    }

    Log_info(TAG_, "Notifying D-Bus at %s", object.c_str());

    // Signal D-Bus when transport state changes
    upnp_transport_register_variable_listener(
        [connection, object](int, const std::string &var_name,
                             const std::string &,
                             const std::string &new_value) {
          if (var_name != "TransportState") return;

          g_dbus_connection_emit_signal(
              connection, NULL, object.c_str(), interface_name_,
              "PlaybackState",
              g_variant_new("(s)", transport_state_map_[new_value].c_str()),
              NULL);
        });

    upnp_control_register_variable_listener(
        [connection, object](int, const std::string &var_name,
                             const std::string &,
                             const std::string &new_value) {
          if (var_name != "Volume") return;

          g_dbus_connection_emit_signal(
              connection, NULL, object.c_str(), interface_name_, "Volume",
              g_variant_new("(s)", new_value.c_str()), NULL);
        });
  }

 private:
  static constexpr const char *TAG_ = "dbus";

  static constexpr const char *interface_name_ =
      "com.hzeller.gmedia_resurrect.v1.Monitor";
  static std::map<const std::string, const std::string> transport_state_map_;
};

#endif