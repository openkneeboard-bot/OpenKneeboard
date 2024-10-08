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

#include <OpenKneeboard/Bookmark.hpp>
#include <OpenKneeboard/CursorClickableRegions.hpp>
#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/UILayerBase.hpp>

#include <shims/winrt/base.h>

#include <OpenKneeboard/audited_ptr.hpp>

#include <memory>

namespace OpenKneeboard {

class KneeboardState;
class KneeboardView;

class BookmarksUILayer final
  : public UILayerBase,
    private EventReceiver,
    public std::enable_shared_from_this<BookmarksUILayer> {
 public:
  static std::shared_ptr<BookmarksUILayer>
  Create(const audited_ptr<DXResources>& dxr, KneeboardState*, KneeboardView*);
  virtual ~BookmarksUILayer();

  virtual void PostCursorEvent(
    const NextList&,
    const Context&,
    KneeboardViewID,
    const CursorEvent&) override;
  virtual Metrics GetMetrics(const NextList&, const Context&) const override;
  [[nodiscard]] task<void> Render(
    const RenderContext&,
    const NextList&,
    const Context&,
    const PixelRect&) override;

  BookmarksUILayer() = delete;

 private:
  BookmarksUILayer(
    const audited_ptr<DXResources>& dxr,
    KneeboardState*,
    KneeboardView*);
  void Init();

  audited_ptr<DXResources> mDXResources;
  KneeboardState* mKneeboardState {nullptr};
  KneeboardView* mKneeboardView;

  winrt::com_ptr<ID2D1SolidColorBrush> mBackgroundBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mTextBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mHoverBrush;

  winrt::com_ptr<ID2D1SolidColorBrush> mCurrentPageStrokeBrush;
  winrt::com_ptr<ID2D1StrokeStyle> mCurrentPageStrokeStyle;

  struct Button {
    D2D1_RECT_F mRect {};
    Bookmark mBookmark {};

    bool operator==(const Button&) const noexcept;
  };

  using Buttons = std::shared_ptr<CursorClickableRegions<Button>>;

  Buttons mButtons;
  Buttons LayoutButtons();

  bool IsEnabled() const;
  void OnClick(const Button&);
};

}// namespace OpenKneeboard
