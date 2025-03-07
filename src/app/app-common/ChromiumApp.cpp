/*
 * OpenKneeboard
 *
 * Copyright (C) 2025 Fred Emmott <fred@fredemmott.com>
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
#include <OpenKneeboard/ChromiumApp.hpp>
#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/RuntimeFiles.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/version.hpp>

#include <include/cef_app.h>
#include <include/cef_base.h>
#include <include/cef_sandbox_win.h>
#include <include/cef_version.h>

namespace OpenKneeboard {

class ChromiumApp::Impl : public CefApp, public CefBrowserProcessHandler {
 public:
  Impl() {
  }

  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }

  void OnBeforeCommandLineProcessing(
    const CefString& process_type,
    CefRefPtr<CefCommandLine> command_line) override {
    command_line->AppendSwitch("angle");
    command_line->AppendSwitchWithValue("use-angle", "d3d11");
    command_line->AppendSwitchWithValue("use-adapter-luid", this->GetGpuLuid());
  }

 private:
  IMPLEMENT_REFCOUNTING(Impl);
  DISALLOW_COPY_AND_ASSIGN(Impl);

  CefString mGpuLuid;

  CefString GetGpuLuid() {
    if (!mGpuLuid.empty()) {
      return mGpuLuid;
    }
    D3D11Resources dxr;
    const auto luid = std::bit_cast<uint64_t>(dxr.mAdapterLUID);
    dprint("Setting CEF GPU LUID to {:#018x}", luid);
    mGpuLuid = std::format(
      "{},{}", luid >> 32, luid & std::numeric_limits<uint32_t>::max());
    return mGpuLuid;
  }
};

struct ChromiumApp::Wrapper {
  struct CefApp;

  CefRefPtr<Impl> mCefApp;
#ifdef CEF_USE_SANDBOX
  CefScopedSandboxInfo mCefSandbox;
#endif
};

ChromiumApp::ChromiumApp() {
  const auto subprocessPath
    = (Filesystem::GetRuntimeDirectory().parent_path() / RuntimeFiles::CHROMIUM);
  const auto libcefPath = subprocessPath.parent_path();

  struct scope_exit_t {
    scope_exit_t() {
      GetDllDirectoryW(MAX_PATH, mOldDllDirectory);
    }
    ~scope_exit_t() {
      SetDllDirectoryW(mOldDllDirectory);
    }

   private:
    wchar_t mOldDllDirectory[MAX_PATH] {};
  } at_scope_exit;
  SetDllDirectoryW(libcefPath.c_str());

  p.reset(new Wrapper());
  p->mCefApp = new Impl();

  CefMainArgs mainArgs {};
  CefSettings settings {};
  CefString(&settings.log_file)
    .FromWString(
      (Filesystem::GetLogsDirectory() / "chromium-debug.log").wstring());
  settings.log_severity = cef_log_severity_t::LOGSEVERITY_ERROR;
  settings.multi_threaded_message_loop = true;
  settings.windowless_rendering_enabled = true;
  CefString(&settings.user_agent_product)
    .FromString(std::format(
      "OpenKneeboard/{}.{}.{}.{} Chromium/{}.0.0.0",
      Version::Major,
      Version::Minor,
      Version::Patch,
      Version::Build,
      CEF_VERSION_MAJOR));
  CefString(&settings.root_cache_path)
    .FromWString(
      (Filesystem::GetLocalAppDataDirectory() / "Chromium").wstring());

  CefString(&settings.browser_subprocess_path)
    .FromWString(subprocessPath.wstring());

  void* sandboxInfo {nullptr};
#ifdef CEF_USE_SANDBOX
  sandboxInfo = p->mCefSandbox.sandbox_info();
#endif

  CefInitialize(mainArgs, settings, p->mCefApp, sandboxInfo);
}

ChromiumApp::~ChromiumApp() {
  p = nullptr;
  CefShutdown();
}

}// namespace OpenKneeboard