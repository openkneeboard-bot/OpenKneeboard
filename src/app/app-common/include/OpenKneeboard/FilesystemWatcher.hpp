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

#include <OpenKneeboard/Events.hpp>

#include <OpenKneeboard/final_release_deleter.hpp>
#include <OpenKneeboard/handles.hpp>

#include <shims/winrt/base.h>

#include <winrt/Windows.Storage.Search.h>

#include <filesystem>
#include <memory>

#include <fileapi.h>

namespace OpenKneeboard {

class FilesystemWatcher final
  : public std::enable_shared_from_this<FilesystemWatcher> {
 public:
  using unique_changenotification = std::
    unique_ptr<HANDLE, CHandleDeleter<HANDLE, &FindCloseChangeNotification>>;
  static std::shared_ptr<FilesystemWatcher> Create(
    const std::filesystem::path&);

  static OpenKneeboard::fire_and_forget final_release(
    std::unique_ptr<FilesystemWatcher>);

  Event<std::filesystem::path> evFilesystemModifiedEvent;

  FilesystemWatcher() = delete;
  ~FilesystemWatcher();

 private:
  FilesystemWatcher(const std::filesystem::path&);
  void Initialize();
  task<void> Run();
  OpenKneeboard::fire_and_forget OnContentsChanged();

  std::optional<task<void>> mImpl;

  winrt::apartment_context mOwnerThread;
  std::filesystem::path mPath;

  std::filesystem::file_time_type mLastWriteTime;
  bool mSettling = false;

  unique_changenotification mHandle;
  std::stop_source mStop;
};

}// namespace OpenKneeboard
