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

#include <functional>

namespace OpenKneeboard {

// Roughly equivalent to `std::experimental::scope_exit`, but that isn't wideley
// available yet
class scope_exit final {
 private:
  std::function<void()> mCallback;

 public:
  scope_exit(std::function<void()> f);
  ~scope_exit() noexcept;

  void abandon();

  scope_exit(scope_exit&&) noexcept = default;
  scope_exit& operator=(scope_exit&&) noexcept = default;

  scope_exit() = delete;
  scope_exit(const scope_exit&) = delete;
  scope_exit& operator=(const scope_exit&) = delete;
};

}// namespace OpenKneeboard
