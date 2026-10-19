# AefUtility – Shared Utilities for Unreal Engine

| | |
|---|---|
| Version | 3.2.0 |
| Engine | Unreal Engine 5.7 |
| Platform | Windows 64-bit |
| Module | `AefUtility` (Runtime) |
| Developer | Ars Electronica Futurelab |
| License | MIT – see [LICENSE](../../../../LICENSE) |

AefUtility is the shared base of the Ars Electronica Futurelab plugins. It resolves which `AefConfig.ini` is active,
so every plugin reads its configuration from the same file, and it copies that file into packaged builds.

---

## What it does

- Returns the path of the active configuration file. The default is `<Project>/Config/AefConfig.ini`.
- Lets you choose a different file with the command-line argument `-aefConfig=<path>`, for example per nDisplay node or per installation.
- Resolves relative `-aefConfig=` paths against the project directory and normalizes the result.
- Falls back to the default file when the override does not exist, logs this as an error and reports it through `GetConfigSource()`.
  `IsConfigWritable()` returns `false` in that state, so tools that write the config do not overwrite the default file by mistake.
- Resolves the path once at module startup and logs which file is used (`LogAefUtility`), in the editor and in packaged builds,
  with or without `-aefConfig=`. The result is cached; it is resolved again only when the process command line changes.
- Copies `Config/AefConfig.ini` automatically into every packaged build as a loose, editable file (see [Packaging](#packaging)).
- Used by AefPharus.

AefUtility only resolves the path. It does not check sections or values inside the file. Configuration changes take effect after a restart.

---

## Setup

1. The plugin lives in `Project/Plugins/AefUtility/` and is enabled by default. Plugins that depend on it (AefPharus) enable it automatically.
2. Keep the configuration at `Project/Config/AefConfig.ini`, or start the application with an override:

   ```
   DeepSpaceStarter.exe -aefConfig="D:/Shows/Installation/AefConfig.ini"
   DeepSpaceStarter.exe -aefConfig="Config/AefConfig_Test.ini"   (relative to the project directory)
   ```

3. Check the log at startup: `LogAefUtility: Using default config: ...` or `LogAefUtility: Display: Using config override: ...`.
   In Blueprint, **Get Config File Path** (category `AEF|Utility`) returns the same path.
4. Package the project as usual. `AefConfig.ini` is copied into the build automatically; no Project Settings entry and no manual copy are needed.

---

## Configuration

AefUtility has no configuration section of its own.

| Command-line argument | Effect |
|---|---|
| `-aefConfig=<path>` | Uses this file if it exists. Relative paths are resolved against the project directory. If the file does not exist, the default is used and an error is logged. |
| none | Uses `<Project>/Config/AefConfig.ini`. |

Location of the active file:

| Situation | Path |
|---|---|
| Editor | `<Project>/Config/AefConfig.ini` |
| Packaged build | `<Build>/DeepSpaceStarter/Config/AefConfig.ini`, for example `Deploy/Build/Windows/DeepSpaceStarter/Config/AefConfig.ini` |
| With `-aefConfig=` | The given file; relative paths start at `<Project>/` in the editor and at `<Build>/DeepSpaceStarter/` in a packaged build |

---

## Packaging

`AefConfig.ini` is part of every packaged build without any manual step:

- **Automatic:** `AefUtility.Build.cs` registers `<Project>/Config/AefConfig.ini` as a runtime dependency of type `NonUFS`
  for all non-editor targets. Packaging copies the file as a loose file to `<Build>/<Project>/Config/AefConfig.ini`,
  which is exactly the default path in the packaged build.
- **Editable on site:** the file is not inside the `.pak`. It can be opened with a text editor on the show machine; restart the application afterwards.
- **Blueprint-only project:** works. Because the plugins contain C++ modules, Unreal compiles them while packaging and creates
  temporary target files for it. The packaging log then shows `... is being treated as a code-based project ... Creating temporary .Target.cs files.`
  This is expected and is what includes `AefConfig.ini`.
- **No Project Settings entry:** the file does not appear under **Project Settings > Packaging > Additional Non-Asset Directories to Copy**.
  That list only accepts folders inside `Content/` and cannot reference `Config/AefConfig.ini`. Do not add `../Config` there;
  it would copy all engine `.ini` files a second time. `Project/Config/DefaultGame.ini` contains a comment pointing here.
- **Only this file:** other files in `Config/` are not copied. Additional configuration files for `-aefConfig=`
  (for example per nDisplay node) have to be placed on the show machine separately.
- **No file, no copy:** if the project has no `Config/AefConfig.ini`, nothing is copied and the plugins use their built-in defaults.

Checking a packaged build:

1. Package the project (**Platforms > Windows > Package Project**, Development).
2. Check that `<Build>/DeepSpaceStarter/Config/AefConfig.ini` exists.
3. Start the build and look for `LogAefUtility: Using default config: .../Config/AefConfig.ini` in the log (`<Build>/DeepSpaceStarter/Saved/Logs/`).

---

## Blueprint / C++ API

`UAefUtilityBPLibrary` (include `AefUtilityBPLibrary.h`):

| Function | Access | Description |
|---|---|---|
| `GetConfigFilePath()` | Blueprint (Pure), C++ | Normalized path of the active configuration file. Cached; resolved again only when the command line changes. |
| `GetConfigSource()` | Blueprint (Pure), C++ | `EAefConfigSource`: `Default`, `Override`, `OverrideMissingFallback` (override given but missing, default used) or `DefaultMissing`. |
| `GetRequestedOverridePath()` | Blueprint (Pure), C++ | Full path requested with `-aefConfig=`; empty without override. |
| `IsConfigWritable(FString& OutReason)` | C++ only | `false` while a requested override is missing. Code that writes the config should check this first. |
| `ResolveConfigFilePath(const TCHAR* CommandLineOverride)` | C++ only | Same resolution, but reads `aefConfig=` from the given command line. Pass `nullptr` to use the real command line. Not cached, does not log. |
| `ResolveConfig(const TCHAR* CommandLineOverride)` | C++ only | Like `ResolveConfigFilePath`, but returns `FAefConfigResolution` with path, source and requested override. |

C++ example for a consuming plugin:

```cpp
#include "AefUtilityBPLibrary.h"

const FString ConfigPath = UAefUtilityBPLibrary::GetConfigFilePath();
bool bAutoStart = true;
GConfig->GetBool(TEXT("PharusSubsystem"), TEXT("AutoStartSystem"), bAutoStart, ConfigPath);
```

A consuming plugin lists `AefUtility` under `"Plugins"` in its `.uplugin` and adds `"AefUtility"` to the dependencies in its `Build.cs`.

---

## Troubleshooting

| Symptom | Cause and solution |
|---|---|
| The default file is used although `-aefConfig=` was passed | The override file does not exist. The log shows `aefConfig override '...' not found, falling back to default config: ...`. Check the path and the working directory. |
| `AefConfig.ini` is missing in the packaged build | The project had no `Config/AefConfig.ini` when it was packaged. Add the file and package again. Also check that the packaging log shows the temporary target files being created and the plugins being compiled. |
| The log shows `No AefConfig.ini found at ...` in a packaged build | The file was deleted or renamed on the show machine. Copy it back to `<Build>/DeepSpaceStarter/Config/AefConfig.ini` or start with `-aefConfig=`. |
| Changes in `AefConfig.ini` have no effect | The configuration is only read at startup. Restart the editor or the application. |
| The override is not detected at all | The argument was not passed to the process that actually runs. Check the nDisplay or Switchboard launch command line. |

Log messages are written to the category `LogAefUtility`.

---

See [CHANGELOG.md](CHANGELOG.md) for version history.

© 2026 Ars Electronica Futurelab
