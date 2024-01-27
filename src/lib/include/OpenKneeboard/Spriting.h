/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#pragma once

#include <OpenKneeboard/Pixels.h>

#include <OpenKneeboard/config.h>

namespace OpenKneeboard::Spriting {

constexpr PixelPoint GetOffset(
  uint8_t sprite,
  [[maybe_unused]] uint8_t maxSprites) {
  return {TextureWidth * sprite, 0};
}

constexpr PixelSize GetBufferSize(uint8_t maxSprites) noexcept {
  return {
    TextureWidth * maxSprites,
    TextureHeight,
  };
}

constexpr PixelRect GetRect(uint8_t sprite, uint8_t maxSprites) noexcept {
  return {
    GetOffset(sprite, maxSprites),
    {TextureWidth, TextureHeight},
  };
}

}// namespace OpenKneeboard::Spriting