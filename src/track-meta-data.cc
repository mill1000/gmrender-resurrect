// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
/* track-meta-data - Object holding meta data for a song.
 *
 * Copyright (C) 2012 Henner Zeller
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

// TODO: we're assuming that the namespaces are abbreviated with 'dc' and 'upnp'
// ... but if I understand that correctly, that doesn't need to be the case.

#include "track-meta-data.h"

/**
  @brief  Create and append root element and requried attributes for metadata
  XML

  @param  xml_document pugi::xml_document to add root item to
  @retval none
*/
void TrackMetadata::CreateXmlRoot(tinyxml2::XMLDocument* xml_document) const {
  tinyxml2::XMLElement* root = xml_document->NewElement("DIDL-Lite");
  xml_document->InsertFirstChild(root);

  root->SetAttribute("xmlns", "urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/");
  root->SetAttribute("xmlns:dc", "http://purl.org/dc/elements/1.1/");
  root->SetAttribute("xmlns:upnp", "urn:schemas-upnp-org:metadata-1-0/upnp/");
  
  // An "item" element must have
  // dc:title element first
  // upnp:class element
  // id attribute
  // parentId attribute
  // restricted attribute
  tinyxml2::XMLElement* item = xml_document->NewElement("item");
  root->InsertEndChild(item);

  item->SetAttribute("id", "");
  item->SetAttribute("parentID", "0");
  item->SetAttribute("restricted", "false");
}

/**
  @brief  Format metadata to XML (DIDL-Lite) format. Modifies existing XML
          if passed as argument

  @param  xml Original XML string to modify
  @retval std::string Metadata as XML (DIDL-Lite)
*/
std::string TrackMetadata::ToXml(const std::string& xml) const {
  tinyxml2::XMLDocument xml_document;

  // Parse existing document
  xml_document.Parse(xml.c_str());

  tinyxml2::XMLElement* root = xml_document.FirstChildElement("DIDL-Lite");
  tinyxml2::XMLElement* item = NULL;

  if (root != NULL)
    item = root->FirstChildElement("item");

  // Existing format sucks, just make our own
  if (root == NULL || item == NULL) {
    xml_document.Clear();
    CreateXmlRoot(&xml_document);

    // Update locals with new document objects
    root = xml_document.FirstChildElement("DIDL-Lite");
    item = root->FirstChildElement("item");
  }

  bool modified = false;
  for (const auto& kv : tags_) {
    const std::string& tag = kv.second.key_;
    const std::string& value = kv.second.value_;

    // Skip if no value
    if (value.empty()) continue;

    tinyxml2::XMLElement* element = item->FirstChildElement(tag.c_str());
    if (element) {
      // Check if already equal to avoid ID update
      if (value.compare(element->GetText()) == 0) continue;

      // Update existing XML element
      element->SetText(value.c_str());

      modified = true;
    } else {
      // Insert new XML element
      element = xml_document.NewElement(tag.c_str());
      item->InsertEndChild(element);

      element->SetText(value.c_str());

      modified = true;
    }
  }

  if (modified) {
    char idString[20] = {0};
    snprintf(idString, sizeof(idString), "gmr-%08x", id_);

    item->SetAttribute("id", idString);
  }

  tinyxml2::XMLPrinter printer;
  xml_document.Print(&printer);

  return printer.CStr();
}