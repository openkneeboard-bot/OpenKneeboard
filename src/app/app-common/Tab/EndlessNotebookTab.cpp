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
#include <OpenKneeboard/DoodleRenderer.h>
#include <OpenKneeboard/EndlessNotebookTab.h>
#include <OpenKneeboard/FilePageSource.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/scope_guard.h>

#include <shims/nlohmann/json.hpp>

namespace OpenKneeboard {

EndlessNotebookTab::EndlessNotebookTab(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title)
  : TabBase(persistentID, title), mDXR(dxr), mKneeboard(kbs) {
  mDoodles = std::make_unique<DoodleRenderer>(dxr, kbs);

  AddEventListener(
    mDoodles->evAddedPageEvent, this->evAvailableFeaturesChangedEvent);
  AddEventListener(mDoodles->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
}

std::shared_ptr<EndlessNotebookTab> EndlessNotebookTab::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const std::filesystem::path& path) {
  std::shared_ptr<EndlessNotebookTab> ret(
    new EndlessNotebookTab(dxr, kbs, winrt::guid {}, to_utf8(path.stem())));
  ret->SetPath(path);
  return ret;
}

std::shared_ptr<EndlessNotebookTab> EndlessNotebookTab::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title,
  const nlohmann::json& settings) {
  std::shared_ptr<EndlessNotebookTab> ret(
    new EndlessNotebookTab(dxr, kbs, persistentID, title));
  ret->SetPath(settings.at("Path").get<std::filesystem::path>());
  return ret;
}

EndlessNotebookTab::~EndlessNotebookTab() {
  this->RemoveAllEventListeners();
}

nlohmann::json EndlessNotebookTab::GetSettings() const {
  return {{"Path", GetPath()}};
}

std::string EndlessNotebookTab::GetGlyph() const {
  return GetStaticGlyph();
}

std::string EndlessNotebookTab::GetStaticGlyph() {
  return "\ue8ed";// RepeatOne
}

std::filesystem::path EndlessNotebookTab::GetPath() const {
  return mPath;
}

void EndlessNotebookTab::SetPath(const std::filesystem::path& rawPath) {
  auto path = rawPath;
  if (std::filesystem::exists(path)) {
    path = std::filesystem::canonical(path);
  }
  if (path == mPath) {
    return;
  }
  mPath = path;

  this->Reload();
}

void EndlessNotebookTab::Reload() {
  const scope_guard contentChanged([this]() { evContentChangedEvent.Emit(); });

  mPageIDs.clear();
  mSourcePageID = {nullptr};
  mSource = {};
  mDoodles->Clear();

  auto delegate = FilePageSource::Create(mDXR, mKneeboard, mPath);
  if (!delegate) {
    return;
  }

  mSource = delegate;

  AddEventListener(mSource->evContentChangedEvent, [weak = weak_from_this()]() {
    if (auto self = weak.lock()) {
      self->OnSourceContentChanged();
    }
  });

  if (delegate->GetPageCount() == 0) {
    return;
  }
  mPageIDs = {PageID {}};
  mSourcePageID = mSource->GetPageIDs().front();
}

void EndlessNotebookTab::OnSourceContentChanged() {
  const auto ids = mSource->GetPageIDs();
  if (ids.empty()) {
    mDoodles->Clear();
    mPageIDs.clear();
    mSourcePageID = {nullptr};
    evContentChangedEvent.Emit();
    return;
  }

  if (mSourcePageID == ids.front()) {
    return;
  }

  mDoodles->Clear();
  mSourcePageID = ids.front();
  mPageIDs = {PageID {}};
  evContentChangedEvent.Emit();
}

PageIndex EndlessNotebookTab::GetPageCount() const {
  return mPageIDs.size();
}

std::vector<PageID> EndlessNotebookTab::GetPageIDs() const {
  return mPageIDs;
}

PreferredSize EndlessNotebookTab::GetPreferredSize(PageID) {
  if (mSource) {
    return mSource->GetPreferredSize(mSourcePageID);
  }

  return {
    ErrorRenderSize,
    ScalingKind::Vector,
  };
}

void EndlessNotebookTab::RenderPage(
  RenderTarget* rt,
  PageID pageID,
  const PixelRect& rect) {
  if (!mSource) {
    return;
  }

  mSource->RenderPage(rt, mSourcePageID, rect);
  {
    auto d2d = rt->d2d();
    mDoodles->Render(d2d, pageID, rect);
  }
}

void EndlessNotebookTab::PostCursorEvent(
  EventContext ec,
  const CursorEvent& ce,
  PageID pageID) {
  if (!mSource) {
    return;
  }

  mDoodles->PostCursorEvent(
    ec, ce, pageID, mSource->GetPreferredSize(mSourcePageID).mPixelSize);
  if (mDoodles->HaveDoodles(pageID) && pageID == mPageIDs.back()) {
    mPageIDs.push_back({});
    evPageAppendedEvent.Emit(SuggestedPageAppendAction::KeepOnCurrentPage);
  }
}

bool EndlessNotebookTab::CanClearUserInput(PageID id) const {
  return mDoodles->HaveDoodles(id);
}

bool EndlessNotebookTab::CanClearUserInput() const {
  return mDoodles->HaveDoodles();
}

void EndlessNotebookTab::ClearUserInput(PageID id) {
  mDoodles->ClearPage(id);
  evAvailableFeaturesChangedEvent.Emit();
}

void EndlessNotebookTab::ClearUserInput() {
  mDoodles->Clear();
  evAvailableFeaturesChangedEvent.Emit();
}

}// namespace OpenKneeboard
