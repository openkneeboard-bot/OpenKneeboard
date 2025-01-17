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

#include <OpenKneeboard/CursorEvent.hpp>
#include <OpenKneeboard/DoodleRenderer.hpp>
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/KneeboardView.hpp>
#include <OpenKneeboard/WebView2Renderer.hpp>

#include <OpenKneeboard/json/Geometry2D.hpp>

#include <OpenKneeboard/format/json.hpp>
#include <OpenKneeboard/version.hpp>

#include <shims/winrt/base.h>

#include <winrt/Microsoft.UI.Dispatching.h>

#include <format>
#include <fstream>

namespace OpenKneeboard {

template <class T>
using worker_task = WebView2Renderer::worker_task<T>;
using jsapi_task = WebView2Renderer::jsapi_task;

void to_json(nlohmann::json& j, const WebView2Renderer::APIPage& v) {
  j.update({
    {"guid", std::format("{:nobraces}", v.mGuid)},
    {"pixelSize",
     {
       {"width", v.mPixelSize.mWidth},
       {"height", v.mPixelSize.mHeight},
     }},
    {"extraData", v.mExtraData},
  });
}

void from_json(const nlohmann::json& j, WebView2Renderer::APIPage& v) {
  v.mGuid = j.at("guid");
  if (j.contains("extraData")) {
    v.mExtraData = j.at("extraData");
  }
  if (j.contains("pixelSize")) {
    const auto ps = j.at("pixelSize");
    v.mPixelSize = PixelSize {
      ps.at("width"),
      ps.at("height"),
    };
  }
}

#define IT(x) {CursorTouchState::x, #x},
NLOHMANN_JSON_SERIALIZE_ENUM(
  CursorTouchState,
  {OPENKNEEBOARD_CURSORTOUCHSTATE_VALUES});
#undef IT

namespace {
const ExperimentalFeature RawCursorEventsFeature {
  "RawCursorEvents",
  2024071802};
const ExperimentalFeature DoodlesOnlyFeature {"DoodlesOnly", 2024071802};

const ExperimentalFeature SetCursorEventsModeFeature {
  "SetCursorEventsMode",
  2024071801};

const ExperimentalFeature PageBasedContentFeature {
  "PageBasedContent",
  2024072001};
const ExperimentalFeature PageBasedContentWithRequestPageChangeFeature {
  "PageBasedContent",
  2024073001};

std::array SupportedExperimentalFeatures {
  RawCursorEventsFeature,
  DoodlesOnlyFeature,
  SetCursorEventsModeFeature,
  PageBasedContentFeature,
  PageBasedContentWithRequestPageChangeFeature,
};

template <class... Args>
auto jsapi_error(std::format_string<Args...> fmt, Args&&... args) {
  return std::unexpected {std::format(fmt, std::forward<Args>(args)...)};
}

};// namespace

OPENKNEEBOARD_DEFINE_JSON(ExperimentalFeature, mName, mVersion);

WebView2Renderer::~WebView2Renderer() {
  OPENKNEEBOARD_TraceLoggingWrite("WebView2::~WebView2Renderer()");
}

static wchar_t WindowClassName[] {L"OpenKneeboard/WebView2Host"};

void WebView2Renderer::RegisterWindowClass() {
  WNDCLASSW windowClass {
    .style = CS_HREDRAW | CS_VREDRAW,
    .lpfnWndProc = &WebView2Renderer::WindowProc,
    .hInstance = GetModuleHandle(nullptr),
    .lpszClassName = WindowClassName,
  };
  ::RegisterClassW(&windowClass);
}

void WebView2Renderer::CreateBrowserWindow() {
  OPENKNEEBOARD_TraceLoggingScope("WebView2Renderer::CreateBrowserWindow()");

  {
    static std::once_flag sFlag;
    std::call_once(sFlag, &WebView2Renderer::RegisterWindowClass);
  }

  mBrowserWindow = unique_hwnd {CreateWindowExW(
    WS_EX_NOACTIVATE,
    WindowClassName,
    L"OpenKneeboard WebView2 Host",
    0,
    CW_DEFAULT,
    CW_DEFAULT,
    0,
    0,
    HWND_MESSAGE,
    NULL,
    GetModuleHandle(nullptr),
    nullptr)};
  if (!mBrowserWindow) {
    const auto code = GetLastError();
    auto e = std::system_category().default_error_condition(code);
    dprint(
      "Failed to create browser window: {:#x} - {}",
      std::bit_cast<uint32_t>(code),
      e.message());
    OPENKNEEBOARD_BREAK;
  }
}

task<void> WebView2Renderer::InitializeContentToCapture() {
  OPENKNEEBOARD_TraceLoggingCoro(
    "WebView2Renderer::InitializeContentToCapture");
  auto weak = weak_from_this();

  mState.Assert(State::Constructed);
  auto dqc = mDQC;
  auto dq = mDQC.DispatcherQueue();
  co_await wil::resume_foreground(dq);
  auto self = weak.lock();
  if (!self) {
    co_return;
  }

  this->CreateBrowserWindow();

  this->InitializeComposition();

  using namespace winrt::Microsoft::Web::WebView2::Core;

  const auto windowRef
    = CoreWebView2ControllerWindowReference::CreateFromWindowHandle(
      reinterpret_cast<uint64_t>(mBrowserWindow.get()));

  mController
    = co_await mEnvironment.CreateCoreWebView2CompositionControllerAsync(
      windowRef);

  if (mSettings.mTransparentBackground) {
    mController.DefaultBackgroundColor(
      winrt::Windows::UI::Colors::Transparent());
  }

  mWebView = mController.CoreWebView2();

  auto settings = mWebView.Settings();
  const auto userAgent = std::format(
    L"{} OpenKneeboard/{}.{}.{}.{}",
    std::wstring_view {settings.UserAgent()},
    Version::Major,
    Version::Minor,
    Version::Patch,
    Version::Build);
  settings.UserAgent(userAgent);

  settings.IsWebMessageEnabled(true);
  mWebView.WebMessageReceived(
    bind_refs_front(&WebView2Renderer::OnWebMessageReceived, this));
  mWebView.NavigationStarting([](auto self, auto, auto args) {
    self->mDocumentResources = {};
    const auto originalURI = self->mSettings.mURI;
    const auto newURI = winrt::to_string(args.Uri());
    if (originalURI == newURI) {
      self->mDocumentResources.mPages = self->mInitialPages;
    }
  } | bind_refs_front(this));
  mWebView.DocumentTitleChanged([](auto self) {
    self->evDocumentTitleChangedEvent.Emit(
      winrt::to_string(self->mWebView.DocumentTitle()));
  } | bind_refs_front(this) | drop_winrt_event_args());

  co_await this->ImportJavascriptFile(
    Filesystem::GetImmutableDataDirectory() / "WebView2.js");

  nlohmann::json initData {
    {
      "Version",
      {
        {
          "Components",
          {
            {"Major", Version::Major},
            {"Minor", Version::Minor},
            {"Patch", Version::Patch},
            {"Build", Version::Build},
          },
        },
        {"HumanReadable", Version::ReleaseName},
        {"IsGitHubActionsBuild", Version::IsGithubActionsBuild},
        {"IsTaggedVersion", Version::IsTaggedVersion},
        {"IsStableRelease", Version::IsStableRelease},
      },
    },
    {"AvailableExperimentalFeatures", SupportedExperimentalFeatures},
    {"VirtualHosts", nlohmann::json::object({})},
  };

  if (!mSettings.mVirtualHosts.empty()) {
    auto& jsonVHosts = initData["VirtualHosts"];
    for (auto&& [vhost, path]: mSettings.mVirtualHosts) {
      mWebView.SetVirtualHostNameToFolderMapping(
        winrt::to_hstring(vhost),
        winrt::to_hstring(path.string()),
        CoreWebView2HostResourceAccessKind::Allow);
      jsonVHosts[vhost] = to_utf8(path);
    }
  }

  if (mViewInfo) {
    initData.emplace(
      "PeerInfo",
      nlohmann::json {
        {"ThisInstance",
         {
           {"ViewGUID", std::format("{:nobraces}", mViewInfo->mGuid)},
           {"ViewName", mViewInfo->mName},
         }},
      });
  }

  co_await mWebView.AddScriptToExecuteOnDocumentCreatedAsync(
    std::format(L"window.OpenKneeboard = new OpenKneeboardAPI({});", initData));

  if (mSettings.mIntegrateWithSimHub) {
    co_await this->ImportJavascriptFile(
      Filesystem::GetImmutableDataDirectory() / "WebView2-SimHub.js");
  }

  mController.BoundsMode(CoreWebView2BoundsMode::UseRawPixels);
  mController.RasterizationScale(1.0);
  mController.ShouldDetectMonitorScaleChanges(false);
  mController.Bounds({
    0,
    0,
    mSize.Width<float>(),
    mSize.Height<float>(),
  });

  mController.RootVisualTarget(mWebViewVisual);
  mController.IsVisible(true);

  if (mSettings.mOpenDeveloperToolsWindow) {
    mWebView.OpenDevToolsWindow();
  }

  try {
    mWebView.Navigate(winrt::to_hstring(mSettings.mURI));
  } catch (const winrt::hresult_invalid_argument& e) {
    dprint.Warning("Can't navigate to URI: {}", mSettings.mURI);
  }

  mState.Transition<State::Constructed, State::InitializedComposition>();
}

std::optional<float> WebView2Renderer::GetHDRWhiteLevelInNits() const {
  return {};
}

winrt::Windows::Graphics::DirectX::DirectXPixelFormat
WebView2Renderer::GetPixelFormat() const {
  return winrt::Windows::Graphics::DirectX::DirectXPixelFormat::
    B8G8R8A8UIntNormalized;
}

winrt::Windows::Graphics::Capture::GraphicsCaptureItem
WebView2Renderer::CreateWGCaptureItem() {
  return winrt::Windows::Graphics::Capture::GraphicsCaptureItem::
    CreateFromVisual(mRootVisual);
}

PixelRect WebView2Renderer::GetContentRect(const PixelSize& captureSize) const {
  return {{0, 0}, mSize};
}

PixelSize WebView2Renderer::GetSwapchainDimensions(
  const PixelSize& captureSize) const {
  return captureSize;
}

void WebView2Renderer::PostFrame() {
  this->FlushCursorEvents();
}

void WebView2Renderer::OnJSAPI_Peer_SendMessageToPeers(
  const WebView2Renderer::InstanceID& sender,
  const nlohmann::json& message) {
  if (!mViewInfo) {
    return;
  }

  if (mViewInfo->mGuid == sender) {
    return;
  }

  if (mDocumentResources.mContentMode != ContentMode::PageBased) {
    // No 'peers'
    return;
  }

  this->SendJSEvent(
    "peerMessage",
    {{
      "detail",
      {
        {"message", message},
      },
    }});
}

void WebView2Renderer::PostCursorEvent(
  KneeboardViewID ctx,
  const CursorEvent& event) {
  if (!mController) {
    return;
  }

  switch (mDocumentResources.mCursorEventsMode) {
    case CursorEventsMode::MouseEmulation: {
      std::unique_lock lock(mCursorEventsMutex);
      mCursorEvents.push(event);
      return;
    }
    case CursorEventsMode::Raw:
      this->SendJSEvent(
        "cursor",
        {
          {"detail",
           {
             {"touchState", event.mTouchState},
             {"buttons", event.mButtons},
             {"position",
              {
                {"x", event.mX},
                {"y", event.mY},
              }},
           }},
        });
      return;
    case CursorEventsMode::DoodlesOnly: {
      mDoodles->PostCursorEvent(
        ctx, event, mDocumentResources.mCurrentPage, mSize);
      return;
    }
  }
}

OpenKneeboard::fire_and_forget WebView2Renderer::SendJSEvent(
  std::string eventType,
  nlohmann::json eventOptions) {
  auto weak = weak_from_this();
  co_await wil::resume_foreground(mDQC.DispatcherQueue());
  auto self = weak.lock();
  if (!self) {
    co_return;
  }

  const nlohmann::json message {
    {"OpenKneeboard_WebView2_MessageType", "Event"},
    {"eventType", eventType},
    {"eventOptions", eventOptions},
  };

  mWebView.PostWebMessageAsJson(winrt::to_hstring(message.dump()));
}

OpenKneeboard::fire_and_forget WebView2Renderer::ActivateJSAPI(
  std::string api) {
  auto weak = weak_from_this();
  co_await wil::resume_foreground(mDQC.DispatcherQueue());
  auto self = weak.lock();
  if (!self) {
    co_return;
  }

  const nlohmann::json message {
    {"OpenKneeboard_WebView2_MessageType", "ActivateAPI"},
    {"api", api},
  };

  mWebView.PostWebMessageAsJson(winrt::to_hstring(message.dump()));
}

void WebView2Renderer::PostCustomAction(
  std::string_view id,
  const nlohmann::json& arg) {
  this->SendJSEvent(
    "plugin/tab/customAction",
    {{"detail",
      {
        {"id", id},
        {"extraData", arg},
      }}});
}

worker_task<void> WebView2Renderer::ImportJavascriptFile(
  std::filesystem::path path) {
  std::ifstream f(path);
  std::stringstream ss;
  ss << f.rdbuf();
  const auto js = ss.str();

  co_await mWebView.AddScriptToExecuteOnDocumentCreatedAsync(
    winrt::to_hstring(js));
}

task<void> WebView2Renderer::RenderPage(
  const RenderContext& rc,
  PageID page,
  const PixelRect& rect) {
  auto& dr = mDocumentResources;
  if (dr.mContentMode == ContentMode::PageBased && dr.mCurrentPage != page) {
    auto it = std::ranges::find(dr.mPages, page, &APIPage::mPageID);
    if (it != dr.mPages.end()) {
      dr.mCurrentPage = page;
      co_await this->Resize(it->mPixelSize);
      this->SendJSEvent("pageChanged", {{"detail", {{"page", *it}}}});
    }
  }

  auto rt = rc.GetRenderTarget();

  WGCRenderer::Render(rt, rect);

  if (
    dr.mContentMode == ContentMode::PageBased
    || dr.mCursorEventsMode == CursorEventsMode::DoodlesOnly) {
    mDoodles->Render(rt, page, rect);
  }
}

OpenKneeboard::fire_and_forget WebView2Renderer::FlushCursorEvents() {
  if (mCursorEvents.empty()) {
    co_return;
  }
  auto weakThis = weak_from_this();
  co_await wil::resume_foreground(mDQC.DispatcherQueue());
  auto self = weakThis.lock();
  if (!self) {
    co_return;
  }

  std::queue<CursorEvent> events;
  {
    std::unique_lock lock(mCursorEventsMutex);
    events = std::move(mCursorEvents);
    mCursorEvents = {};
  }

  while (!events.empty()) {
    const auto event = events.front();
    events.pop();

    // TODO: button tracking
    using namespace winrt::Microsoft::Web::WebView2::Core;

    using EVKind = CoreWebView2MouseEventKind;
    using VKey = CoreWebView2MouseEventVirtualKeys;

    VKey keys {};
    if ((event.mButtons & 1)) {
      keys |= VKey::LeftButton;
    }
    if ((event.mButtons & (1 << 1))) {
      keys |= VKey::RightButton;
    }

    if (event.mTouchState == CursorTouchState::NotNearSurface) {
      if (mMouseButtons & 1) {
        mController.SendMouseInput(EVKind::LeftButtonUp, keys, 0, {});
      }
      if (mMouseButtons & (1 << 1)) {
        mController.SendMouseInput(EVKind::RightButtonUp, keys, 0, {});
      }
      mMouseButtons = {};
      mController.SendMouseInput(EVKind::Leave, keys, 0, {});
      continue;
    }

    // We only pay attention to left/right buttons
    const auto buttons = event.mButtons & 3;
    if (buttons == mMouseButtons) {
      mController.SendMouseInput(EVKind::Move, keys, 0, {event.mX, event.mY});
      continue;
    }

    const auto down = event.mButtons & ~mMouseButtons;
    const auto up = mMouseButtons & ~event.mButtons;
    mMouseButtons = buttons;

    if (down & 1) {
      mController.SendMouseInput(
        EVKind::LeftButtonDown, keys, 0, {event.mX, event.mY});
    }
    if (up & 1) {
      mController.SendMouseInput(
        EVKind::LeftButtonUp, keys, 0, {event.mX, event.mY});
    }
    if (down & (1 << 1)) {
      mController.SendMouseInput(
        EVKind::RightButtonDown, keys, 0, {event.mX, event.mY});
    }
    if (up & (1 << 1)) {
      mController.SendMouseInput(
        EVKind::RightButtonUp, keys, 0, {event.mX, event.mY});
    }
  }

  mLastCursorEventAt = std::chrono::system_clock::now();
}

OpenKneeboard::fire_and_forget WebView2Renderer::OnWebMessageReceived(
  winrt::Microsoft::Web::WebView2::Core::CoreWebView2,
  winrt::Microsoft::Web::WebView2::Core::CoreWebView2WebMessageReceivedEventArgs
    args) {
  const auto weak = weak_from_this();

  const auto json = to_string(args.WebMessageAsJson());
  const auto parsed = nlohmann::json::parse(json);

  if (!parsed.contains("messageName")) {
    co_return;
  }

  const std::string message = parsed.at("messageName");
  const uint64_t callID = parsed.at("callID");

  const auto respond = &WebView2Renderer::SendJSResponse | bind_refs_front(this)
    | bind_front(callID);

  try {
#define OKB_INVOKE_JSAPI(APIFUNC) \
  if (message == "OpenKneeboard." #APIFUNC) { \
    respond(co_await this->JSAPI_##APIFUNC(parsed.at("messageData"))); \
    co_return; \
  }
    OPENKNEEBOARD_JSAPI_METHODS(OKB_INVOKE_JSAPI)
#undef OKB_CALL_JSAPI
  } catch (const nlohmann::json::exception& e) {
    dprint("JSAPI ERROR: JSON error: {} ({})", e.what(), e.id);
    respond(jsapi_error("JSON error: {} ({})", e.what(), e.id));
    co_return;
  } catch (const std::exception& e) {
    dprint("JSAPI ERROR - std::exception: {}", e.what());
    respond(jsapi_error("C++ exception: {}", e.what()));
    co_return;
  }

  OPENKNEEBOARD_BREAK;
  respond(jsapi_error("Invalid JS API request: {}", message));
}

jsapi_task WebView2Renderer::JSAPI_SetPreferredPixelSize(nlohmann::json args) {
  PixelSize size = {
    args.at("width"),
    args.at("height"),
  };
  if (size.mWidth < 1 || size.mHeight < 1) {
    co_return jsapi_error("WebView2 requested 0px area, ignoring");
  }
  if (
    size.mWidth > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION
    || size.mHeight > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION) {
    dprint(
      "WebView2 requested size {}x{} is outside of D3D11 limits",
      size.mWidth,
      size.mHeight);
    size = size.ScaledToFit(
      {
        D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION,
        D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION,
      },
      Geometry2D::ScaleToFitMode::ShrinkOnly);
    if (size.mWidth < 1 || size.mHeight < 1) {
      co_return jsapi_error(
        "Requested size scales down to < 1px in at least 1 dimension");
    }
    dprint("Shrunk to fit: {}x{}", size.mWidth, size.mHeight);
  }

  const auto success = [&size](auto result) {
    return nlohmann::json {
      {"result", result},
      {
        "details",
        {
          {"width", size.mWidth},
          {"height", size.mHeight},
        },
      },
    };
  };

  if (mSize == size) {
    co_return success("no change");
  }

  co_await this->Resize(size);
  co_return success("resized");
}

task<void> WebView2Renderer::Resize(PixelSize size) {
  if (mSize == size) {
    co_return;
  }
  auto weak = weak_from_this();
  const auto uiThread = mUIThread;

  const auto dq = mDQC.DispatcherQueue();
  co_await wil::resume_foreground(dq);

  {
    auto self = weak.lock();
    if (!self) {
      co_return;
    }

    mSize = size;

    const auto wfSize
      = mSize.StaticCast<float, winrt::Windows::Foundation::Numerics::float2>();
    mRootVisual.Size(wfSize);
    mWebViewVisual.Size(wfSize);
    mController.Bounds({0, 0, mSize.Width<float>(), mSize.Height<float>()});
    mController.RasterizationScale(1.0);
  }

  co_await uiThread;

  auto self = weak.lock();
  if (!self) {
    co_return;
  }
  WGCRenderer::ForceResize(size);

  evNeedsRepaintEvent.Emit();
}

WebView2Renderer::JSAPIResult
WebView2Renderer::GetJSAPIMissingRequiredFeatureResponse(
  const ExperimentalFeature& feature,
  const std::source_location& caller) {
  constexpr std::string_view prefix {"JSAPI_"};
  std::string_view func {caller.function_name()};
  {
    const auto idx = func.find_first_of("(");
    if (idx != func.npos) {
      func = func.substr(0, idx);
    }
  }
  {
    const auto idx = func.find_last_of("::");
    if (idx != func.npos) {
      func.remove_prefix(idx + 1);
    }
  }

  assert(func.starts_with(prefix));
  func.remove_prefix(prefix.size());
  const auto message = std::format(
    "⚠️ JS ERROR: OpenKneeboard.{}() failed because it requires the "
    "experimental feature {}, "
    "which has not been enabled.",
    func,
    feature);
  dprint(message);
  return jsapi_error("{}", message);
}

bool WebView2Renderer::IsJSAPIFeatureEnabled(
  const ExperimentalFeature& feature) const {
  return std::ranges::contains(
    mDocumentResources.mEnabledExperimentalFeatures, feature);
}

jsapi_task WebView2Renderer::JSAPI_SetCursorEventsMode(nlohmann::json args) {
  auto& pageApi = mDocumentResources;

  if (!this->IsJSAPIFeatureEnabled(SetCursorEventsModeFeature)) {
    co_return this->GetJSAPIMissingRequiredFeatureResponse(
      SetCursorEventsModeFeature);
  }
  const std::string mode = args.at("mode");

  auto success = []() { return nlohmann::json {{"result", "success"}}; };

  if (mode == "MouseEmulation") {
    pageApi.mCursorEventsMode = CursorEventsMode::MouseEmulation;
    co_return success();
  }

  if (mode == "DoodlesOnly") {
    if (!this->IsJSAPIFeatureEnabled(DoodlesOnlyFeature)) {
      co_return this->GetJSAPIMissingRequiredFeatureResponse(
        DoodlesOnlyFeature);
    }
    pageApi.mCursorEventsMode = CursorEventsMode::DoodlesOnly;
    co_return success();
  }

  if (mode == "Raw") {
    if (!this->IsJSAPIFeatureEnabled(RawCursorEventsFeature)) {
      co_return this->GetJSAPIMissingRequiredFeatureResponse(
        RawCursorEventsFeature);
    }
    pageApi.mCursorEventsMode = CursorEventsMode::Raw;
    co_return success();
  }

  co_return jsapi_error("Unrecognized mode '{}'", mode);
}

jsapi_task WebView2Renderer::JSAPI_GetPages(nlohmann::json args) {
  auto& dr = mDocumentResources;
  if (!this->IsJSAPIFeatureEnabled(PageBasedContentFeature)) {
    co_return this->GetJSAPIMissingRequiredFeatureResponse(
      PageBasedContentFeature);
  }

  if (dr.mContentMode == ContentMode::Scrollable) {
    dr.mContentMode = ContentMode::PageBased;
    if (!mViewInfo) {
      // If we don't have a view, that means:
      // 1. we have not yet switched to renderer-per-view
      // 2. so this instance will be the first to call `SetPages()`
      // 3. So our current pages are the 'placeholder' single page
      assert(dr.mPages.size() == 1);
      dr.mPages.clear();
    }
  }

  co_return nlohmann::json {
    {"havePages", !dr.mPages.empty()},
    {"pages", dr.mPages},
  };
}

jsapi_task WebView2Renderer::JSAPI_SendMessageToPeers(nlohmann::json args) {
  if (!this->IsJSAPIFeatureEnabled(PageBasedContentFeature)) {
    co_return this->GetJSAPIMissingRequiredFeatureResponse(
      PageBasedContentFeature);
  }

  if (!mViewInfo) {
    co_return jsapi_error("Pages have not been set; no peers exist");
  }

  evJSAPI_SendMessageToPeers.Emit(mViewInfo->mGuid, args.at("message"));

  co_return nlohmann::json {};
}

jsapi_task WebView2Renderer::JSAPI_OpenDeveloperToolsWindow(
  [[maybe_unused]] nlohmann::json args) {
  mWebView.OpenDevToolsWindow();
  co_return nlohmann::json {};
}

jsapi_task WebView2Renderer::JSAPI_SetPages(nlohmann::json args) {
  auto weak = weak_from_this();
  auto thread = mUIThread;

  auto& pageApi = mDocumentResources;
  if (!this->IsJSAPIFeatureEnabled(PageBasedContentFeature)) {
    co_return this->GetJSAPIMissingRequiredFeatureResponse(
      PageBasedContentFeature);
  }

  std::vector<APIPage> pages;
  for (const auto& it: args.at("pages")) {
    auto page = it.get<APIPage>();
    const auto old
      = std::ranges::find(pageApi.mPages, page.mGuid, &APIPage::mGuid);
    if (old != pageApi.mPages.end()) {
      // Use the new pixelsize and data, but keep existing internal ID
      page.mPageID = old->mPageID;
    }

    if (page.mPixelSize.IsEmpty()) {
      page.mPixelSize = mSize;
    }
    pages.push_back(page);
  }

  if (pages.empty()) {
    co_return jsapi_error("Must provide at least one page definition");
  }

  pageApi.mPages = std::move(pages);
  pageApi.mContentMode = ContentMode::PageBased;

  co_await thread;
  auto self = weak.lock();
  if (!self) {
    co_return jsapi_error("WebView2 lifetime exceeded.");
  }
  evJSAPI_SetPages.EnqueueForContext(mUIThread, pageApi.mPages);

  co_return nlohmann::json {};
}

jsapi_task WebView2Renderer::JSAPI_RequestPageChange(nlohmann::json args) {
  if (!this->IsJSAPIFeatureEnabled(
        PageBasedContentWithRequestPageChangeFeature)) {
    co_return this->GetJSAPIMissingRequiredFeatureResponse(
      PageBasedContentWithRequestPageChangeFeature);
  }

  if (mDocumentResources.mContentMode != ContentMode::PageBased) {
    co_return jsapi_error(
      "Content mode is not page-based; call GetPages() and SetPages() first");
  }

  if (
    mKind != Kind::Plugin
    && (std::chrono::system_clock::now() - mLastCursorEventAt)
      > std::chrono::milliseconds(100)) {
    co_return jsapi_error(
      "Web Dashboards can only call `RequestPageChange()` shortly after a "
      "CursorEvent; to remove this limit, create an OpenKneeboard plugin.");
  }

  const auto guid = args.get<winrt::guid>();
  const auto& pages = mDocumentResources.mPages;
  const auto it = std::ranges::find(pages, guid, &APIPage::mGuid);
  if (it == pages.end()) {
    co_return jsapi_error("Couldn't find page with GUID {}", guid);
  }
  if (!mViewInfo) {
    OPENKNEEBOARD_BREAK;
    co_return jsapi_error("OKB internal error: don't have an active view");
  }

  this->evJSAPI_PageChangeRequested.EnqueueForContext(
    mUIThread, mViewInfo->mRuntimeID, it->mPageID);
  co_return nlohmann::json {};
}

jsapi_task WebView2Renderer::JSAPI_EnableExperimentalFeatures(
  nlohmann::json args) {
  std::vector<ExperimentalFeature> enabledFeatures;

  auto& pageApi = mDocumentResources;

  const auto EnableFeature = [&](const ExperimentalFeature& feature) {
    dprint("WARNING: JS enabled experimental feature {}", feature);
    pageApi.mEnabledExperimentalFeatures.push_back(feature);
    enabledFeatures.push_back(feature);
  };

  for (const auto& featureSpec: args.at("features")) {
    const std::string name = featureSpec.at("name");
    const uint64_t version = featureSpec.at("version");

    if (std::ranges::contains(
          pageApi.mEnabledExperimentalFeatures,
          name,
          &ExperimentalFeature::mName)) {
      co_return jsapi_error(
        "Experimental feature `{}` is already enabled", name);
    }

    const ExperimentalFeature feature {name, version};

    if (!std::ranges::contains(SupportedExperimentalFeatures, feature)) {
      if (!std::ranges::contains(
            SupportedExperimentalFeatures, name, &ExperimentalFeature::mName)) {
        co_return jsapi_error(
          "`{}` is not a recognized experimental feature", name);
      }

      co_return jsapi_error(
        "`{}` is a recognized experimental feature, but `{}` is not a "
        "supported version",
        name,
        version);
    }

    EnableFeature(feature);

    if (feature == RawCursorEventsFeature || feature == DoodlesOnlyFeature) {
      // Nothing to do except enable these
      continue;
    }

    if (feature == SetCursorEventsModeFeature) {
      this->ActivateJSAPI("SetCursorEventsMode");
      continue;
    }

    if (feature == PageBasedContentFeature) {
      this->ActivateJSAPI("PageBasedContent");
      const auto message = std::format(
        "⚠️ WARNING: enabling obsolete experimental feature {}", feature);
      dprint(message);
      this->SendJSLog(message);
      continue;
    }

    if (feature == PageBasedContentWithRequestPageChangeFeature) {
      EnableFeature(PageBasedContentFeature);
      this->ActivateJSAPI("PageBasedContentWithRequestPageChange");
      // Enable the original too, as this is a strict superset
      continue;
    }

    const auto message = std::format(
      "OpenKneeboard internal error: `{}` v{} is a recognized but "
      "unhandled experimental feature",
      name,
      version);
    dprint(message);
    OPENKNEEBOARD_BREAK;
    co_return jsapi_error("{}", message);
  }

  co_return nlohmann::json {
    {"result", std::format("enabled {} features", enabledFeatures.size())},
    {"details",
     {
       {"features", enabledFeatures},
     }},
  };
}

task<void> WebView2Renderer::DisposeAsync() noexcept {
  OPENKNEEBOARD_TraceLoggingCoro("WebView2Renderer::DisposeAsync");
  if (mState.Get() == State::Disposed) {
    co_return;
  }
  auto self = shared_from_this();

  mState.Transition<State::InitializedComposition, State::Disposing>();
  co_await wil::resume_foreground(mDQC.DispatcherQueue());

  if (!self) {
    OPENKNEEBOARD_BREAK;
    co_return;
  }

  mEnvironment = nullptr;
  mController = nullptr;
  mDQC = nullptr;
  mWebView = nullptr;
  mWebViewVisual = nullptr;
  mRootVisual = nullptr;
  mCompositor = nullptr;
  mBrowserWindow.reset();

  co_await mUIThread;

  co_await WGCRenderer::DisposeAsync();

  mState.Transition<State::Disposing, State::Disposed>();
}

void WebView2Renderer::InitializeComposition() noexcept {
  OPENKNEEBOARD_TraceLoggingScope("WebView2Renderer::InitializeComposition");
  if (mCompositor) {
    OPENKNEEBOARD_BREAK;
    return;
  }
  mCompositor = {};
  mRootVisual = mCompositor.CreateContainerVisual();
  mRootVisual.Size(
    mSize.StaticCast<float, winrt::Windows::Foundation::Numerics::float2>());
  mRootVisual.IsVisible(true);

  mWebViewVisual = mCompositor.CreateContainerVisual();
  mRootVisual.Children().InsertAtTop(mWebViewVisual);
  mWebViewVisual.RelativeSizeAdjustment({1, 1});
}

task<std::shared_ptr<WebView2Renderer>> WebView2Renderer::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const Kind kind,
  const Settings& settings,
  const std::shared_ptr<DoodleRenderer>& doodles,
  const WorkerDQC& workerDQC,
  const winrt::Microsoft::Web::WebView2::Core::CoreWebView2Environment&
    environment,
  KneeboardView* view,
  const std::vector<APIPage>& apiPages) try {
  std::shared_ptr<WebView2Renderer> ret {new WebView2Renderer(
    dxr, kbs, kind, settings, doodles, workerDQC, environment, view, apiPages)};
  co_await ret->Init();
  co_return ret;
} catch (const std::exception& e) {
  dprint.Error("Failed to initialize WebView2Renderer: {}", e.what());
  throw;
} catch (const winrt::hresult_error& e) {
  dprint.Error(
    "Failed to initalize WebView2Renderer with hresult error: {}", e.message());
  throw;
}

WebView2Renderer::WebView2Renderer(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const Kind kind,
  const Settings& settings,
  const std::shared_ptr<DoodleRenderer>& doodles,
  const WorkerDQC& workerDQC,
  const winrt::Microsoft::Web::WebView2::Core::CoreWebView2Environment&
    environment,
  KneeboardView* view,
  const std::vector<APIPage>& apiPages)
  : WGCRenderer(dxr, kbs, {}),
    mDXResources(dxr),
    mKneeboard(kbs),
    mKind(kind),
    mSettings(settings),
    mSize(settings.mInitialSize),
    mDoodles(doodles),
    mDQC(workerDQC),
    mEnvironment(environment),
    mInitialPages(apiPages) {
  if (view) {
    mViewInfo = ViewInfo {
      .mGuid = view->GetPersistentGUID(),
      .mName = std::string {view->GetName()},
      .mRuntimeID = view->GetRuntimeID(),
    };
  }
}

void WebView2Renderer::OnJSAPI_Peer_SetPages(
  const std::vector<APIPage>& pages) {
  if (pages == mDocumentResources.mPages) {
    return;
  }

  EventDelay delay;

  mDocumentResources.mPages = pages;

  this->SendJSEvent("pagesChanged", {{"detail", {{"pages", pages}}}});
}

OpenKneeboard::fire_and_forget WebView2Renderer::SendJSResponse(
  uint64_t callID,
  JSAPIResult result) {
  if (!this->IsLiveForContent()) {
    co_return;
  }
  auto weak = weak_from_this();
  auto dq = mDQC.DispatcherQueue();
  co_await wil::resume_foreground(dq);

  auto self = weak.lock();
  if (!(self && self->IsLiveForContent())) {
    co_return;
  }
  nlohmann::json response {
    {"OpenKneeboard_WebView2_MessageType", "AsyncResponse"},
    {"callID", callID},
  };
  if (result.has_value()) {
    if (result->is_null()) {
      response.emplace("result", "ok");
    } else {
      response.emplace("result", result.value());
    }
  } else {
    response.emplace("error", result.error());
    dprint("WARNING: WebView2 API error: {}", result.error());
    if constexpr (!Version::IsStableRelease) {
      mWebView.OpenDevToolsWindow();
    }
  }

  if (mWebView) {
    mWebView.PostWebMessageAsJson(winrt::to_hstring(response.dump()));
  }
}

LRESULT CALLBACK WebView2Renderer::WindowProc(
  HWND const window,
  UINT const message,
  WPARAM const wparam,
  LPARAM const lparam) noexcept {
  return DefWindowProc(window, message, wparam, lparam);
}

}// namespace OpenKneeboard