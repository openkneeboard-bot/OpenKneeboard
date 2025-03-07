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
#include <OpenKneeboard/ITab.hpp>
#include <OpenKneeboard/KneeboardView.hpp>
#include <OpenKneeboard/SwitchTabAction.hpp>
#include <OpenKneeboard/TabView.hpp>
#include <OpenKneeboard/TabsList.hpp>

namespace OpenKneeboard {

SwitchTabAction::SwitchTabAction(
  const std::shared_ptr<KneeboardView>& kneeboardView,
  const std::shared_ptr<ITab>& tab)
  : ToolbarAction({}, tab->GetTitle()),
    mKneeboardView(kneeboardView),
    mTabID(tab->GetRuntimeID()) {
  AddEventListener(
    kneeboardView->evCurrentTabChangedEvent, this->evStateChangedEvent);
}

SwitchTabAction::~SwitchTabAction() {
  this->RemoveAllEventListeners();
}

bool SwitchTabAction::IsChecked() const {
  auto kbv = mKneeboardView.lock();
  if (!kbv) {
    return false;
  }
  const auto tab = kbv->GetCurrentTabView()->GetRootTab().lock();
  return tab && tab->GetRuntimeID() == mTabID;
}

bool SwitchTabAction::IsEnabled() const {
  return true;
}

task<void> SwitchTabAction::Execute() {
  if (auto kbv = mKneeboardView.lock()) {
    kbv->SetCurrentTabByRuntimeID(mTabID);
  }
  co_return;
}

}// namespace OpenKneeboard
