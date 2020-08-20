/*
 * Copyright 2013-2016 Emmanuel Engelhart <kelson@kiwix.org>
 * Copyright 2016 Matthieu Gautier <mgautier@kymeria.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU  General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#ifndef OPENZIM_ZIMWRITERFS_MIMETYPECOUNTER_H
#define OPENZIM_ZIMWRITERFS_MIMETYPECOUNTER_H

#include "zimcreatorfs.h"
#include <map>

class MimetypeCounter : public IHandler
{
 public:
  void handleItem(std::shared_ptr<zim::writer::Item> item);
  std::string getName() const { return "Counter"; }
  std::string getData() const;

 private:
  std::map<std::string, unsigned int> counters;
};

#endif  // OPENZIM_ZIMWRITERFS_MIMETYPECOUNTER_H
