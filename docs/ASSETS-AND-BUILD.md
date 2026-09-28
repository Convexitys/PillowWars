# Source and asset scope

The authoritative editable project remains on its owner's Windows laptop. This publication is a clean gameplay-source snapshot, not a replacement project and not a fully self-contained editor checkout.

Included: custom runtime and editor-test C++ module, target/build definitions, sanitized game/input/render configuration, documentation, and selected actual-game screenshots.

Excluded: personal paths/settings, Android file-server credentials, local backups, cache/build/crash folders, authentication data, Unreal Engine source/binaries, the development MCP plugin, and uncooked binary content. Template assets and any third-party content retain their own terms; this repository does not relicense them.

To compile the original full project, use Unreal Engine **5.5.3**, Visual Studio 2022 C++ Build Tools (MSVC v143 14.38 verified), and the Windows SDK. Close Unreal before a normal module rebuild. The public C++ source references project-specific Blueprints, skeletal meshes, maps, sounds, and materials; without that content it is for code review, not a runnable editor reconstruction.

No blanket open-source license is assigned to third-party dependencies or artwork. Public visibility permits inspection under GitHub's service terms; it does not assert unrestricted redistribution rights to everything shown. Contact the project owner for asset/editable-project access.

For playing, use the cooked Windows release. Always distribute the complete extracted package, not an isolated EXE. The packaged release does not require the MCP plugin or Unreal Editor to be installed.
