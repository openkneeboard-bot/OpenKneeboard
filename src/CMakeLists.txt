add_compile_options(
  "/Zc:__cplusplus"
  "/utf-8"

  # All warnings as errors
  "/WX"
  # TODO: more warnings, /W4

  # Lots of legacy C functions are unsafe, and raise warnings in third-party code
  "/wd4996"

  # Informational 'warning' describing how diamond inheritance works
  "/wd4250"
)
if(CLANG_CL)
  add_compile_options(
    "/clang:-Wno-missing-braces"
  )
endif()

add_compile_definitions(
  "UNICODE=1"
  "_UNICODE=1"
  "NOMINMAX=1"
  "WIN32_LEAN_AND_MEAN=1"
  "$<IF:$<CONFIG:Debug>,DEBUG,NDEBUG>"
)

add_subdirectory(shaders)

# Always link this so we never accidentally use the Windows SDK version (as opposed to the Windows App SDK version)
link_libraries(ThirdParty::CppWinRT)

include(sourcelink)

include(ok_add_runtime_files)
include(ok_add_library)
include(ok_add_executable)
include(ok_add_license_file)

add_subdirectory(lib)
add_subdirectory(api)

add_subdirectory(dcs-hook)
add_subdirectory(injectables)

add_subdirectory(games)
add_subdirectory(utilities)

if(BUILD_IS_64BIT)
  add_subdirectory(app)
endif()