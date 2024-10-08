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

#include <OpenKneeboard/FilesystemWatcher.hpp>
#include <OpenKneeboard/PageSourceWithDelegates.hpp>

#include <shims/winrt/base.h>

#include <winrt/Windows.Storage.Search.h>

#include <OpenKneeboard/audited_ptr.hpp>

#include <filesystem>
#include <memory>

namespace OpenKneeboard {

class PlainTextPageSource;

class PlainTextFilePageSource final : public PageSourceWithDelegates {
 private:
  PlainTextFilePageSource(const audited_ptr<DXResources>&, KneeboardState*);

 public:
  PlainTextFilePageSource() = delete;
  virtual ~PlainTextFilePageSource();
  static task<std::shared_ptr<PlainTextFilePageSource>> Create(
    audited_ptr<DXResources>,
    KneeboardState*,
    std::filesystem::path path = {});

  std::filesystem::path GetPath() const;
  void SetPath(const std::filesystem::path& path);

  PageIndex GetPageCount() const override;

  void Reload();

 private:
  winrt::apartment_context mUIThread;
  std::filesystem::path mPath;
  std::shared_ptr<PlainTextPageSource> mPageSource;

  std::string GetFileContent() const;

  std::shared_ptr<FilesystemWatcher> mWatcher;

  void SubscribeToChanges() noexcept;
  void OnFileModified(const std::filesystem::path&) noexcept;
};

}// namespace OpenKneeboard
