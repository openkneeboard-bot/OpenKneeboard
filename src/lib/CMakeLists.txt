ok_add_library(OpenKneeboard-Lib-Headers INTERFACE)
target_include_directories(OpenKneeboard-Lib-Headers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/OpenKneeboard/config.in.hpp"
  "${CMAKE_CURRENT_BINARY_DIR}/include/OpenKneeboard/config.hpp"
  @ONLY
)
file(
  GENERATE
  OUTPUT
  "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/include/OpenKneeboard/detail/config.hpp"
  CONTENT "\
namespace OpenKneeboard::detail::Config {
  constexpr auto BuildType = \"$<CONFIG>\";
}"
)
ok_add_library(OpenKneeboard-config INTERFACE)
target_include_directories(
  OpenKneeboard-config
  INTERFACE
  "${CMAKE_CURRENT_BINARY_DIR}/include"
  "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/include"
)
target_link_libraries(OpenKneeboard-config INTERFACE OpenKneeboard-Lib-Headers)

ok_add_library(OpenKneeboard-ThreadGuard STATIC ThreadGuard.cpp)
target_link_libraries(OpenKneeboard-ThreadGuard PUBLIC OpenKneeboard-Lib-Headers)
target_link_libraries(OpenKneeboard-ThreadGuard PRIVATE OpenKneeboard-dprint)

ok_add_library(OpenKneeboard-GetMainHWND STATIC GetMainHWND.cpp)
target_link_libraries(
  OpenKneeboard-GetMainHWND
  PUBLIC
  OpenKneeboard-Lib-Headers
)
target_link_libraries(
  OpenKneeboard-GetMainHWND
  PRIVATE
  OpenKneeboard-config
  OpenKneeboard-shims
)

ok_add_library(OpenKneeboard-RunSubprocessAsync STATIC RunSubprocessAsync.cpp)
target_link_libraries(
  OpenKneeboard-RunSubprocessAsync
  PUBLIC
  OpenKneeboard-shims
)
target_link_libraries(
  OpenKneeboard-RunSubprocessAsync
  PRIVATE
  OpenKneeboard-dprint
)

ok_add_library(OpenKneeboard-GetSystemColor STATIC GetSystemColor.cpp)
target_link_libraries(
  OpenKneeboard-GetSystemColor
  PUBLIC
  OpenKneeboard-Lib-Headers)

ok_add_library(OpenKneeboard-FilesDiffer STATIC FilesDiffer.cpp)
target_link_libraries(
  OpenKneeboard-FilesDiffer
  PUBLIC
  OpenKneeboard-Lib-Headers)

ok_add_library(OpenKneeboard-handles INTERFACE)
target_link_libraries(
  OpenKneeboard-handles
  INTERFACE
  OpenKneeboard-Lib-Headers
)

ok_add_library(OpenKneeboard-Elevation STATIC Elevation.cpp)
target_link_libraries(
  OpenKneeboard-Elevation
  PUBLIC
  OpenKneeboard-Lib-Headers
)
target_link_libraries(
  OpenKneeboard-Elevation
  PRIVATE
  OpenKneeboard-config
  OpenKneeboard-dprint
  OpenKneeboard-scope_exit
  ThirdParty::CppWinRT
  System::Shell32
)

ok_add_library(OpenKneeboard-UTF8 STATIC utf8.cpp)
target_link_libraries(
  OpenKneeboard-UTF8
  PUBLIC
  ThirdParty::JSON
  OpenKneeboard-Lib-Headers
)

ok_add_library(OpenKneeboard-PDFNavigation STATIC PDFNavigation.cpp)
target_link_libraries(
  OpenKneeboard-PDFNavigation
  PUBLIC
  OpenKneeboard-Lib-Headers
  OpenKneeboard-shims
)
target_link_libraries(
  OpenKneeboard-PDFNavigation
  PRIVATE
  OpenKneeboard-DebugTimer
  OpenKneeboard-UTF8
  OpenKneeboard-dprint
  ThirdParty::QPDF
)

ok_add_library(OpenKneeboard-DebugTimer STATIC DebugTimer.cpp)
target_link_libraries(
  OpenKneeboard-DebugTimer
  PRIVATE
  OpenKneeboard-dprint
)
target_link_libraries(
  OpenKneeboard-DebugTimer
  PUBLIC
  OpenKneeboard-Lib-Headers
)

ok_add_library(OpenKneeboard-tracing STATIC tracing.cpp)
target_link_libraries(
  OpenKneeboard-tracing
  PRIVATE
  OpenKneeboard-dprint
  ThirdParty::CppWinRT
)
target_link_libraries(
  OpenKneeboard-tracing
  PUBLIC
  OpenKneeboard-Lib-Headers
)

ok_add_library(
  OpenKneeboard-D3D11
  STATIC
  D3D11.cpp
  D3D11/Renderer.cpp
)
target_link_libraries(
  OpenKneeboard-D3D11
  PUBLIC
  System::D3d11
  OpenKneeboard-shims
  ThirdParty::DirectXTK
  ThirdParty::CppWinRT
  OpenKneeboard-Lib-Headers
  PRIVATE
  OpenKneeboard-dprint
  System::Dxguid
)

ok_add_library(
  OpenKneeboard-D3D12
  STATIC
  D3D12.cpp
  D3D12/Renderer.cpp
)
target_link_libraries(
  OpenKneeboard-D3D12
  PUBLIC
  System::D3d12
  OpenKneeboard-shims
  ThirdParty::DirectXTK12
  ThirdParty::CppWinRT
  OpenKneeboard-Lib-Headers
  PRIVATE
  OpenKneeboard-dprint
  OpenKneeboard-RenderDoc
  System::Dxguid
)

ok_add_library(
  OpenKneeboard-Vulkan
  STATIC
  Vulkan.cpp
  Vulkan/SpriteBatch.cpp)
target_link_libraries(
  OpenKneeboard-Vulkan
  PUBLIC
  OpenKneeboard-dprint
  ThirdParty::VulkanHeaders
  OpenKneeboard-Lib-Headers
  OpenKneeboard-SpriteBatch-SPIRV
)

ok_add_library(OpenKneeboard-DXResources STATIC DXResources.cpp)
target_link_libraries(
  OpenKneeboard-DXResources
  PUBLIC
  OpenKneeboard-Lib-Headers
  OpenKneeboard-D3D11
  PRIVATE
  OpenKneeboard-dprint
  System::Gdi32
)

set(RUNTIME_FILES_CPP "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/RuntimeFiles.cpp")
file(GENERATE OUTPUT "${RUNTIME_FILES_CPP}" INPUT RuntimeFiles.cpp.in)
ok_add_library(
  OpenKneeboard-RuntimeFiles
  STATIC
  "${RUNTIME_FILES_CPP}"
  RuntimeFilesGetInstallationDirectory.cpp
)
target_link_libraries(
  OpenKneeboard-RuntimeFiles
  PUBLIC
  OpenKneeboard-Lib-Headers
  PRIVATE
  OpenKneeboard-dprint
  OpenKneeboard-Filesystem
)

ok_add_library(
  OpenKneeboard-SHM
  STATIC
  SHM.cpp
  SHM/ActiveConsumers.cpp
  FlatConfig.cpp
)
target_link_libraries(
  OpenKneeboard-SHM
  PUBLIC
  OpenKneeboard-dprint
  OpenKneeboard-Lib-Headers
  PRIVATE
  OpenKneeboard-Elevation
  OpenKneeboard-config
  OpenKneeboard-shims
  OpenKneeboard-version
)

ok_add_library(
  OpenKneeboard-SHM-Client-D3D11
  STATIC
  SHM/D3D11.cpp
)
target_link_libraries(
  OpenKneeboard-SHM-Client-D3D11
  PUBLIC
  OpenKneeboard-shims
  OpenKneeboard-SHM
  System::D3d11
  ThirdParty::CppWinRT
  ThirdParty::DirectXTK
  PRIVATE
  OpenKneeboard-dprint
)

ok_add_library(
  OpenKneeboard-SHM-Client-D3D12
  STATIC
  SHM/D3D12.cpp
)
target_link_libraries(
  OpenKneeboard-SHM-Client-D3D12
  PUBLIC
  OpenKneeboard-shims
  System::D3d12
  ThirdParty::CppWinRT
  ThirdParty::DirectXTK12
  PRIVATE
  OpenKneeboard-dprint
)

ok_add_library(
  OpenKneeboard-SHM-Client-Vulkan
  STATIC
  SHM/Vulkan.cpp
)
target_link_libraries(
  OpenKneeboard-SHM-Client-Vulkan
  PUBLIC
  OpenKneeboard-SHM-Client-D3D11
  OpenKneeboard-Vulkan
  OpenKneeboard-shims
  ThirdParty::CppWinRT
  ThirdParty::VulkanHeaders
)

ok_add_library(OpenKneeboard-dprint STATIC dprint.cpp)
target_link_libraries(
  OpenKneeboard-dprint
  PUBLIC
  OpenKneeboard-Lib-Headers
  OpenKneeboard-config
)
target_link_libraries(OpenKneeboard-dprint
  PRIVATE
  OpenKneeboard-scope_exit
)

ok_add_library(OpenKneeboard-consolelib STATIC ConsoleLoopCondition.cpp)
target_link_libraries(OpenKneeboard-consolelib PUBLIC OpenKneeboard-Lib-Headers)

ok_add_library(OpenKneeboard-APIEvent STATIC APIEvent.cpp)
target_link_libraries(OpenKneeboard-APIEvent PRIVATE OpenKneeboard-config OpenKneeboard-dprint)
target_link_libraries(OpenKneeboard-APIEvent PUBLIC OpenKneeboard-Lib-Headers OpenKneeboard-UTF8 OpenKneeboard-json)

ok_add_library(OpenKneeboard-D2DErrorRenderer STATIC D2DErrorRenderer.cpp)
target_link_libraries(
  OpenKneeboard-D2DErrorRenderer
  PUBLIC
  OpenKneeboard-DXResources
  OpenKneeboard-UTF8
  OpenKneeboard-config
  System::D2d1
  System::Dwrite
  OpenKneeboard-Lib-Headers)

ok_add_library(OpenKneeboard-RayIntersectsRect STATIC RayIntersectsRect.cpp)
target_link_libraries(
  OpenKneeboard-RayIntersectsRect
  PUBLIC
  OpenKneeboard-Lib-Headers
  ThirdParty::DirectXTK)
target_link_libraries(
  OpenKneeboard-RayIntersectsRect
  PRIVATE
  OpenKneeboard-dprint)

ok_add_library(OpenKneeboard-scope_exit STATIC scope_exit.cpp)
target_link_libraries(
  OpenKneeboard-scope_exit
  PUBLIC
  OpenKneeboard-Lib-Headers)

ok_add_library(OpenKneeboard-Wintab
  STATIC
  WintabTablet.cpp
)
target_link_libraries(
  OpenKneeboard-Wintab
  PRIVATE
  OpenKneeboard-Lib-Headers
  ThirdParty::Wintab
  OpenKneeboard-dprint
  OpenKneeboard-UTF8
)

ok_add_library(OpenKneeboard-VRKneeboard STATIC VRKneeboard.cpp)
target_link_libraries(
  OpenKneeboard-VRKneeboard
  PUBLIC
  OpenKneeboard-Lib-Headers
  ThirdParty::DirectXTK
  OpenKneeboard-RayIntersectsRect
  OpenKneeboard-SHM
)

set(VERSION_CPP_FILE "${CMAKE_CURRENT_BINARY_DIR}/include/OpenKneeboard/version.hpp")
add_custom_target(
  OpenKneeboard-version-sources
  COMMAND
  ${CMAKE_COMMAND}
  -DVERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
  -DVERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
  -DVERSION_PATCH=${CMAKE_PROJECT_VERSION_PATCH}
  -DVERSION_BUILD=${CMAKE_PROJECT_VERSION_TWEAK}
  -DRELEASE_NAME=${RELEASE_NAME}
  -DOPENXR_API_LAYER_IMPLEMENTATION_VERSION=${OPENXR_API_LAYER_IMPLEMENTATION_VERSION}
  "-DINPUT_CPP_FILE=${CMAKE_CURRENT_SOURCE_DIR}/include/OpenKneeboard/version.in.hpp"
  "-DOUTPUT_CPP_FILE=${VERSION_CPP_FILE}"
  "-P${CMAKE_CURRENT_SOURCE_DIR}/../version.cmake"
  DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/../version.cmake"
  "version.hpp.in"
  BYPRODUCTS
  "${VERSION_CPP_FILE}"
  WORKING_DIRECTORY
  "${CMAKE_SOURCE_DIR}"
)
ok_add_library(OpenKneeboard-version INTERFACE)
target_include_directories(
  OpenKneeboard-version
  INTERFACE
  "${CMAKE_CURRENT_BINARY_DIR}/include"
)
add_dependencies(OpenKneeboard-version INTERFACE OpenKneeboard-version-sources)

ok_add_library(OpenKneeboard-shims INTERFACE)
target_include_directories(
  OpenKneeboard-shims
  INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
  OpenKneeboard-shims
  INTERFACE
  OpenKneeboard-dprint
)

ok_add_library(OpenKneeboard-OpenXRMode STATIC OpenXRMode.cpp)
target_link_libraries(
  OpenKneeboard-OpenXRMode
  PUBLIC
  ThirdParty::JSON
  OpenKneeboard-Lib-Headers
)
target_link_libraries(
  OpenKneeboard-OpenXRMode
  PRIVATE
  OpenKneeboard-RunSubprocessAsync
  OpenKneeboard-RuntimeFiles
  OpenKneeboard-dprint
  ThirdParty::CppWinRT
)

ok_add_library(OpenKneeboard-json STATIC json.cpp)
target_link_libraries(
  OpenKneeboard-json
  PUBLIC
  OpenKneeboard-dprint
  ThirdParty::JSON
  OpenKneeboard-Lib-Headers
)

ok_add_library(
  OpenKneeboard-SteamVRKneeboard
  STATIC
  SteamVRKneeboard.cpp
)
target_link_libraries(
  OpenKneeboard-SteamVRKneeboard
  PUBLIC
  OpenKneeboard-DXResources
  OpenKneeboard-D3D11
  OpenKneeboard-SHM-Client-D3D11
  OpenKneeboard-VRKneeboard
  OpenKneeboard-config
  ThirdParty::DirectXTK
  ThirdParty::OpenVR
  OpenKneeboard-Lib-Headers
)
target_link_libraries(
  OpenKneeboard-SteamVRKneeboard
  PRIVATE
  OpenKneeboard-dprint
)

ok_add_library(OpenKneeboard-Filesystem STATIC Filesystem.cpp)
target_link_libraries(
  OpenKneeboard-Filesystem
  PUBLIC
  OpenKneeboard-shims
  OpenKneeboard-config
  OpenKneeboard-Lib-Headers
  PRIVATE
  OpenKneeboard-dprint
  OpenKneeboard-UTF8
  System::Shlwapi
  ThirdParty::CppWinRT
  ThirdParty::WIL
)

ok_add_library(OpenKneeboard-WindowCaptureControl STATIC WindowCaptureControl.cpp)
target_link_libraries(
  OpenKneeboard-WindowCaptureControl
  PRIVATE
  OpenKneeboard-RuntimeFiles
  OpenKneeboard-dprint
  OpenKneeboard-shims
)
target_link_libraries(
  OpenKneeboard-WindowCaptureControl
  PUBLIC
  OpenKneeboard-handles
  OpenKneeboard-Lib-Headers
)

ok_add_library(
  OpenKneeboard-RenderDoc
  STATIC
  RenderDoc.cpp
)
target_link_libraries(
  OpenKneeboard-RenderDoc
  PUBLIC
  OpenKneeboard-Lib-Headers
  PRIVATE
  ThirdParty::RenderDoc
)