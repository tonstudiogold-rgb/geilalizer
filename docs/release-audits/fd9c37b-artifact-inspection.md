# Geilalizer macOS Artifact Inspection — fd9c37b / Run 27519014310

Status: PASS

Scope approved by owner: download and inspect GitHub Actions artifact from run 27519014310 only. No merge, no release, no macos-latest generation, no private NAM bundling.

## Source run

- Repository: `tonstudiogold-rgb/geilalizer`
- Branch: `milestone/realtime-rescue`
- Commit: `fd9c37b39469e16bba15a54d92cf0af560671852`
- Run ID: `27519014310`
- Run URL: <https://github.com/tonstudiogold-rgb/geilalizer/actions/runs/27519014310>
- Run conclusion: `success`
- Artifact: `Geilalizer-macOS`
- Artifact API URL: <https://api.github.com/repos/tonstudiogold-rgb/geilalizer/actions/artifacts/7627384848/zip>
- Artifact expired: `false`
- Artifact size: `10422741` bytes

## Local inspection path

- Local artifact directory: `/tmp/geilalizer-artifact-27519014310`
- Full local inspection report: `/tmp/geilalizer-artifact-27519014310/artifact-inspection-report.txt`
- Inspection report SHA256: `fbbe550b9b1feb9dc85e18b982966123685106d562d0eb398ed09d6ccac11b95`

## Checksums

| File | SHA256 | Size |
| --- | --- | ---: |
| `24-Track Standalone Geilalizer-macOS.dmg` | `87bbabbc95e31efc9b87bfa327a7e6667ac28af292569fb0e6dd4b2e09700728` | 2,882,077 |
| `24-Track Standalone Geilalizer-macOS.zip` | `3e2f2c11e4f986f1ea0388e401fb060b1378ebb149e7356489808e3a24deddbe` | 2,354,821 |
| `geilalizer-macos.dmg` | `87bbabbc95e31efc9b87bfa327a7e6667ac28af292569fb0e6dd4b2e09700728` | 2,882,077 |
| `geilalizer-macos.zip` | `3e2f2c11e4f986f1ea0388e401fb060b1378ebb149e7356489808e3a24deddbe` | 2,354,821 |

All bundled `.sha256` manifests matched their corresponding local files by basename. The manifest text references `dist/...`, while GitHub artifact extraction places files directly in the artifact directory; hash values matched.

## Private asset scan

| Area | Result |
| --- | --- |
| Artifact top-level files | PASS: no `*.nam`, no `private_nam_models` |
| ZIP central directory | PASS: 8 entries, 0 forbidden entries |
| Unpacked ZIP | PASS: no `*.nam`, no `private_nam_models` |
| DMG/APFS extracted with 7z | PASS: no `*.nam`, no `private_nam_models` |

## ZIP app inspection

- App: `24-Track Standalone Geilalizer.app`
- `CFBundleName`: `24-Track Standalone Geilalizer`
- `CFBundleDisplayName`: `24-Track Standalone Geilalizer`
- `CFBundleIdentifier`: `com.geilalizer.standalone`
- `CFBundleVersion`: `0.1.0`
- `CFBundleShortVersionString`: `0.1.0`
- `CFBundleExecutable`: `24-Track Standalone Geilalizer`
- `NSMicrophoneUsageDescription`: present
- Executable permission: executable
- Executable SHA256: `905fd55fd8936981c85ae30893da1bb5725a060679d72a921f80161a4f4ec283`
- `file`: `Mach-O 64-bit arm64 executable`

## DMG app inspection

- DMG type: UDZO/zlib with APFS volume `24-Track Standalone Geilalizer`
- App found in DMG: `24-Track Standalone Geilalizer.app`
- Info.plist values matched the ZIP app.
- Executable permission: executable
- Executable SHA256: `905fd55fd8936981c85ae30893da1bb5725a060679d72a921f80161a4f4ec283`
- `file`: `Mach-O 64-bit arm64 executable`

## Tooling note

This audit ran on Linux. macOS-only `hdiutil`, `plutil`, and `otool` were unavailable. Equivalent checks used `unzip`, Python `plistlib`, `file`, a Python Mach-O load-command parser, `dmg2img`, and `7z` APFS extraction. DMG contents were successfully listed and extracted for inspection.

## Verdict

PASS for the approved artifact inspection scope. No private NAM model assets were found in the artifact, ZIP, unpacked ZIP app, or extracted DMG app.
