# Changelog

All notable changes to the AefUtility plugin are documented in this file.
This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [3.2.3] - 2026-10-27

### Changed
- Version aligned with the project release 3.2.3; no functional changes.

## [3.2.2] - 2026-10-27

### Changed
- The command-line override is now `-dsConfig=<path>` (was `-aefConfig=<path>`). Behavior is unchanged; the old
  argument is no longer recognized, so launch commands (e.g. nDisplay / Switchboard, `project.json` → `args`) must be updated.
  Log and error messages now say `dsConfig override ...`.

## [3.2.1] - 2026-10-20

Version aligned with the DeepSpace Starter 3.2.1 release. No functional changes.

## [3.2.0] - 2026-10-19

Version aligned with the DeepSpace Starter project version (shared version for project, plugins and `AefConfig.ini`).
No functional changes.

## [1.0.0] - 2026-10-15

Initial release.

### Added
- `UAefUtilityBPLibrary::GetConfigFilePath()` resolves the active `AefConfig.ini`
  (default `<Project>/Config/AefConfig.ini`). Resolved and logged once at module startup (editor and packaged builds);
  cached and resolved again only when the command line changes.
- Command-line override `-aefConfig=<path>`. Relative paths are resolved against the project directory and normalized.
  A missing or empty override is logged as an error and falls back to the default file.
- `GetConfigSource()`, `GetRequestedOverridePath()`, `IsConfigWritable()`, `ResolveConfigFilePath()` and `ResolveConfig()`
  with `EAefConfigSource` / `FAefConfigResolution`.
- Log category `LogAefUtility`.
- `Config/AefConfig.ini` is staged automatically as a loose, editable file in packaged builds
  (`AefUtility.Build.cs`, `RuntimeDependencies` / `NonUFS`). Works in C++ and Blueprint-only projects.
