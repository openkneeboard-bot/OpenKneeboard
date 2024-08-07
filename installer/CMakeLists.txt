install(
  FILES
  "WiX-LICENSE.txt"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-WiX.txt"
  COMPONENT Installer
)

# #### CPack (MSI) Config #####
set(CPACK_GENERATOR "WIX")
set(CPACK_PACKAGE_NAME "OpenKneeboard")
set(CPACK_PACKAGE_VENDOR "Fred Emmott")
set(CPACK_PACKAGE_VERSION "${CMAKE_PROJECT_VERSION}")
set(CPACK_WIX_UPGRADE_GUID "843c9331-0610-4ab1-9cf9-5305c896fb5b")
set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/assets/icon.ico")
set(CPACK_WIX_COMPONENT_INSTALL ON)
set(CPACK_WIX_UI_REF "WixUI_Minimal_NoEULA")
set(CPACK_WIX_PROPERTY_REINSTALLMODE "amus") # Allow DLL downgrades, always install what's in the MSI
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CPACK_PACKAGE_NAME}")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-v${CMAKE_PROJECT_VERSION}")
set(CPACK_PACKAGE_EXECUTABLES "OpenKneeboardApp;OpenKneeboard")
set(CPACK_CREATE_DESKTOP_LINKS "OpenKneeboardApp")
set(CPACK_WIX_UI_DIALOG "${CMAKE_SOURCE_DIR}/assets/WiXUIDialog.png")
set(CPACK_WIX_UI_BANNER "${CMAKE_SOURCE_DIR}/assets/WixUIBanner.png")

# For CI
file(WRITE "${CMAKE_BINARY_DIR}/installer-name.txt" "${CPACK_PACKAGE_FILE_NAME}.msi")

get_cmake_property(CPACK_COMPONENTS_ALL COMPONENTS)
list(REMOVE_ITEM CPACK_COMPONENTS_ALL DebugSymbols)
list(REMOVE_ITEM CPACK_COMPONENTS_ALL DualArch)
list(REMOVE_ITEM CPACK_COMPONENTS_ALL DualArchDebugSymbols)

list(
  APPEND
  CPACK_WIX_EXTRA_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/LaunchAfterInstall.wxs"
  "${CMAKE_CURRENT_SOURCE_DIR}/OpenXRAPILayerRegistration.wxs"
  "${CMAKE_CURRENT_SOURCE_DIR}/WixUI_Minimal_NoEULA.wxs"
  "${CMAKE_CURRENT_SOURCE_DIR}/OpenKneeboardPluginFileType.wxs"
)
list(
  APPEND
  CPACK_WIX_PATCH_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/patch.xml"
)
list(
  APPEND
  CPACK_WIX_EXTENSIONS
  "WixUtilExtension"
)

include(CPack)
cpack_add_component(
  Default
  HIDDEN
)
cpack_add_component(
  Installer
  HIDDEN
)
