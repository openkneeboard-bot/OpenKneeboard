---
parent: Internals
---

# Release Checklist

## Before Release

* Update `Quick Start.pdf`
* Update the `APIEvent` path if the format of existing messages has changed in a backwards-incompatible way
* Check version number in CMakeLists.txt

## Testing

Useful test apps:
- `hello_xr` from the OpenXR SDK source
- `OculusRoomTiny` from the legacy Oculus SDK
- the various imgui examples for non-VR

Test all of these with the `RelWithDebInfo` msi package, as it's the most restricted form of installation.

* Install in clean virtual machine
* Gaze detection, zoomed and unzoomed, before and after centering
  * SteamVR
  * Oculus: D3D11
  * OpenXR: D3D11, D3D12, and Vulkan
* Wacom tablet: drawing, erasing, PDF navigation, expresskeys
* Huion tablet: drawing, erasing, PDF navigation, presskeys + remote control executables
* OpenTabletDriver: drawing, erasing, PDF navigation, keys
* Variable aspect ratios
* Performance
  * Drawing perf/feel in-game:
    * VR
      * SteamVR
      * Oculus: D3D11
      * 64-bit OpenXR: D3D11, D3D12, and Vulkan
      * 32-bit OpenXR: D3D11, D3D12, and Vulkan
    * Non-VR: D3D11
  * Check for issues in Visual Studio performance profiler
  * Check for issues in OpenXR-Tracing in WPA
  * Check a trace via ETL while in VR: maintains even 90hz, no stalls
* Bindings
  * Physical joystick/throttle
  * XBox controller
  * VJoy controller
  * Mouse buttons
  * Keyboard
  * Multiple buttons (combos)
  * Plugging and unplugging devices
* adding, removing, reordering tabs, including DCS tabs
* expected default tabs with quick start guide on fresh install
* adding, removing games
* finding DCS from empty games list
* DCS integration test utilities: `fake-dcs.exe` and `test-apievent-feeder.exe`
* DCS: play full single-player mission in Caucasus or NTTR
  * Briefing tab
    * Text
    * Images
    * A-10C CDU wind
  * Mission kneeboard
  * Radio log
  * Theater kneeboard (only in Caucasus and NTTR)
* DCS: restart mission
* DCS: switch to a different campaign
* DCS: restart DCS. Start multiplayer mission. Check DCS tabs.
* Modifying files:
  * single file tabs:
    * editing a text file leads to an update
    * replacing an image file leads to an update, and all notes being purged
    * replacing pdf file leads to an update, and all notes being purged
  * folder tabs:
    * removing the current page moves to the new first page
    * removing the current page leads to 'no pages' if there are no pages
    * adding a new image in the middle leads to it being inserted in the correct place
      * existing notes are preserved on the corresponding pages

## Releasing

* Push tag
* Wait for GitHub Actions to create a draft release with files and template release notes
* Edit release notes
* Publish
* Update DCS User Files, Discord, Reddit etc
