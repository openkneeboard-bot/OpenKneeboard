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

#include <OpenKneeboard/DoodleRenderer.hpp>
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/WebView2PageSource.hpp>

#include <shims/winrt/base.h>

#include <Windows.h>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/hresult.hpp>
#include <OpenKneeboard/json_format.hpp>
#include <OpenKneeboard/version.hpp>

#include <fstream>
#include <sstream>
#include <system_error>

#include <WebView2.h>
#include <shlwapi.h>
#include <wininet.h>
#include <wrl.h>

namespace OpenKneeboard {

std::shared_ptr<WebView2PageSource> WebView2PageSource::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const Settings& settings) {
  auto ret = std::shared_ptr<WebView2PageSource>(
    new WebView2PageSource(dxr, kbs, settings));
  ret->Init();
  return ret;
}

std::shared_ptr<WebView2PageSource> WebView2PageSource::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const std::filesystem::path& path) {
  char buffer[INTERNET_MAX_URL_LENGTH];
  DWORD charCount {std::size(buffer)};

  winrt::check_hresult(
    UrlCreateFromPathA(path.string().c_str(), buffer, &charCount, NULL));

  const Settings settings {
    .mIntegrateWithSimHub = false,
    .mURI = {buffer, charCount},
  };

  return Create(dxr, kbs, settings);
}

winrt::fire_and_forget WebView2PageSource::Init() {
  if (!IsAvailable()) {
    co_return;
  }
  auto uiThread = mUIThread;
  auto weak = weak_from_this();

  mRenderersState
    .Transition<RenderersState::Constructed, RenderersState::Initializing>();
  co_await winrt::resume_foreground(mWorkerDQ);
  auto self = weak.lock();
  if (!self) {
    co_return;
  }

  SetThreadDescription(GetCurrentThread(), L"OKB WebView2 Worker");

  using namespace winrt::Microsoft::Web::WebView2::Core;

  const auto userData = Filesystem::GetLocalAppDataDirectory() / "WebView2";
  std::filesystem::create_directories(userData);

  const auto edgeArgs
    = std::format(L"--disable-gpu-vsync --max-gum-fps={}", FramesPerSecond);
  CoreWebView2EnvironmentOptions options;
  options.AdditionalBrowserArguments(edgeArgs);

  mEnvironment = co_await CoreWebView2Environment::CreateWithOptionsAsync(
    {}, userData.wstring(), options);

  co_await uiThread;

  auto doodles = std::make_shared<DoodleRenderer>(mDXResources, mKneeboard);

  APIPage apiPage {
    .mGuid = random_guid(),
    .mPixelSize = mSettings.mInitialSize,
    .mPageID = mScrollableContentPageID,
  };
  mDocumentResources = {
    .mPages = {apiPage},
    .mDoodles = doodles,
  };

  auto renderer = WebView2Renderer::Create(
    mDXResources,
    mKneeboard,
    mSettings,
    mDocumentResources.mDoodles,
    mWorkerDQC,
    mEnvironment,
    nullptr,
    {apiPage});
  ConnectRenderer(renderer.get());
  mDocumentResources.mRenderers.emplace(
    mScrollableContentRendererKey, std::move(renderer));

  mRenderersState
    .Transition<RenderersState::Initializing, RenderersState::Ready>();
}

WebView2PageSource::WebView2PageSource(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const Settings& settings)
  : mDXResources(dxr), mKneeboard(kbs), mSettings(settings) {
  OPENKNEEBOARD_TraceLoggingScope("WebView2PageSource::WebView2PageSource()");
  if (!IsAvailable()) {
    return;
  }
  mWorkerDQC = winrt::Windows::System::DispatcherQueueController::
    CreateOnDedicatedThread();
  mWorkerDQ = mWorkerDQC.DispatcherQueue();
}

std::string WebView2PageSource::GetVersion() {
  static std::string sVersion;
  static std::once_flag sFlag;
  std::call_once(sFlag, [p = &sVersion]() {
    WCHAR* version {nullptr};
    if (FAILED(
          GetAvailableCoreWebView2BrowserVersionString(nullptr, &version))) {
      return;
    }

    *p = winrt::to_string(version);

    CoTaskMemFree(version);
  });
  return sVersion;
}

bool WebView2PageSource::IsAvailable() {
  return !GetVersion().empty();
}

WebView2PageSource::~WebView2PageSource() = default;

winrt::Windows::Foundation::IAsyncAction
WebView2PageSource::DisposeAsync() noexcept {
  const auto disposing = mDisposal.Start();
  if (!disposing) {
    co_return;
  }

  auto self = shared_from_this();
  if (mWorkerDQ) {
    co_await wil::resume_foreground(self->mWorkerDQ);

    auto children = self->mDocumentResources.mRenderers | std::views::values
      | std::views::transform([](auto it) { return it->DisposeAsync(); })
      | std::ranges::to<std::vector>();
    for (auto&& child: children) {
      co_await child;
    }

    mEnvironment = nullptr;
    co_await mUIThread;
    mDocumentResources = {};
    mWorkerDQ = {nullptr};
    co_await mWorkerDQC.ShutdownQueueAsync();
  }

  co_await mUIThread;
  this->RemoveAllEventListeners();
  co_return;
}

void WebView2PageSource::PostCursorEvent(
  KneeboardViewID view,
  const CursorEvent& event,
  PageID) {
  const auto key = (mDocumentResources.mContentMode == ContentMode::PageBased)
    ? view
    : mScrollableContentRendererKey;
  auto it = mDocumentResources.mRenderers.find(key);
  if (it == mDocumentResources.mRenderers.end()) {
    return;
  }
  it->second->PostCursorEvent(view, event);
}

void WebView2PageSource::PostCustomAction(
  KneeboardViewID viewID,
  std::string_view actionID,
  const nlohmann::json& arg) {
  static_assert(std::same_as<KneeboardViewID, RendererKey>);

  const auto key = (mDocumentResources.mContentMode == ContentMode::Scrollable)
    ? mScrollableContentRendererKey
    : viewID;

  if (!mDocumentResources.mRenderers.contains(key)) {
    return;
  }

  mDocumentResources.mRenderers.at(key)->PostCustomAction(actionID, arg);
}

bool WebView2PageSource::CanClearUserInput(PageID pageID) const {
  const auto& doodles = mDocumentResources.mDoodles;
  return doodles && doodles->HaveDoodles(pageID);
}

bool WebView2PageSource::CanClearUserInput() const {
  const auto& doodles = mDocumentResources.mDoodles;
  return doodles && doodles->HaveDoodles();
}

void WebView2PageSource::ClearUserInput(PageID pageID) {
  auto& doodles = mDocumentResources.mDoodles;
  if (!doodles) {
    return;
  }
  doodles->ClearPage(pageID);
}

void WebView2PageSource::ClearUserInput() {
  auto& doodles = mDocumentResources.mDoodles;
  if (!doodles) {
    return;
  }
  doodles->Clear();
}

PageIndex WebView2PageSource::GetPageCount() const {
  return this->GetPageIDs().size();
}

std::vector<PageID> WebView2PageSource::GetPageIDs() const {
  switch (mDocumentResources.mContentMode) {
    case ContentMode::PageBased:
      return mDocumentResources.mPages
        | std::views::transform(&APIPage::mPageID)
        | std::ranges::to<std::vector>();
    case ContentMode::Scrollable:
      return {mScrollableContentPageID};
  }
  OPENKNEEBOARD_LOG_AND_FATAL(
    "Invalid content mode in WebView2PageSource::GetPageIDs()");
}

PreferredSize WebView2PageSource::GetPreferredSize(PageID pageID) {
  if (mDocumentResources.mContentMode == ContentMode::Scrollable) {
    auto it = mDocumentResources.mRenderers.find(mScrollableContentRendererKey);
    if (it == mDocumentResources.mRenderers.end()) {
      return {mSettings.mInitialSize, ScalingKind::Bitmap};
    }
    return it->second->GetPreferredSize();
  }

  assert(mDocumentResources.mContentMode == ContentMode::PageBased);

  auto it
    = std::ranges::find(mDocumentResources.mPages, pageID, &APIPage::mPageID);
  if (it != mDocumentResources.mPages.end()) {
    return {
      it->mPixelSize,
      ScalingKind::Bitmap,
    };
  }
  return {};
}

void WebView2PageSource::RenderPage(
  const RenderContext& rc,
  PageID pageID,
  const PixelRect& rect) {
  if (mDisposal.HasStarted()) [[unlikely]] {
    return;
  }
  if (mRenderersState.Get() != RenderersState::Ready) [[unlikely]] {
    return;
  }
  const auto isPageMode
    = (mDocumentResources.mContentMode == ContentMode::PageBased);

  const auto view = rc.GetKneeboardView();
  if (!view) {
    OPENKNEEBOARD_LOG_AND_FATAL(
      "WebView2PageSource::Render() should always have a view");
  }

  const auto key
    = isPageMode ? view->GetRuntimeID() : mScrollableContentRendererKey;

  if (!mDocumentResources.mRenderers.contains(key)) {
    assert(isPageMode);
    auto renderer = WebView2Renderer::Create(
      mDXResources,
      mKneeboard,
      mSettings,
      mDocumentResources.mDoodles,
      mWorkerDQC,
      mEnvironment,
      view,
      mDocumentResources.mPages);
    ConnectRenderer(renderer.get());
    mDocumentResources.mRenderers.emplace(key, std::move(renderer));
  }
  auto renderer = mDocumentResources.mRenderers.at(key);
  renderer->RenderPage(rc, pageID, rect);
}

void WebView2PageSource::ConnectRenderer(WebView2Renderer* renderer) {
  AddEventListener(
    renderer->evContentChangedEvent, this->evContentChangedEvent);
  AddEventListener(
    renderer->evJSAPI_SetPages,
    {weak_from_this(), &WebView2PageSource::OnJSAPI_SetPages});
  AddEventListener(
    renderer->evJSAPI_SendMessageToPeers,
    {weak_from_this(), &WebView2PageSource::OnJSAPI_SendMessageToPeers});
  AddEventListener(renderer->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
  AddEventListener(
    renderer->evJSAPI_PageChangeRequested, this->evPageChangeRequestedEvent);
}

winrt::fire_and_forget WebView2PageSource::OnJSAPI_SetPages(
  std::vector<APIPage> pages) {
  auto keepAlive = shared_from_this();
  mRenderersState
    .Transition<RenderersState::Ready, RenderersState::ChangingModes>();

  mDocumentResources.mPages = pages;

  for (const auto& [rtid, renderer]: mDocumentResources.mRenderers) {
    renderer->OnJSAPI_Peer_SetPages(pages);
  }

  if (mDocumentResources.mContentMode != ContentMode::PageBased) {
    mDocumentResources.mContentMode = ContentMode::PageBased;
    co_await mDocumentResources.mRenderers.at(mScrollableContentRendererKey)
      ->DisposeAsync();
    mDocumentResources.mRenderers.erase(mScrollableContentRendererKey);
  }
  mRenderersState
    .Transition<RenderersState::ChangingModes, RenderersState::Ready>();

  evContentChangedEvent.Emit();
  evAvailableFeaturesChangedEvent.Emit();
}

void WebView2PageSource::OnJSAPI_SendMessageToPeers(
  const InstanceID& sender,
  const nlohmann::json& message) {
  for (const auto& [rtid, renderer]: mDocumentResources.mRenderers) {
    renderer->OnJSAPI_Peer_SendMessageToPeers(sender, message);
  }
}

}// namespace OpenKneeboard
