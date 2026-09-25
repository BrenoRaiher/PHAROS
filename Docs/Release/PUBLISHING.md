# Publishing PHAROS Source

Publish the maintained source tree, excluding local build products and optional
installed evidence. The [licensing overview](../Legal/README.md) identifies
the applicable notices, attribution records, and corresponding source.

## Included

- First-party source, Controller SDK, build scripts, current Unreal assets,
  native tests, contributor documentation, and project configuration.
- Vendored CSPICE/toml++ dependencies, NAIF kernel data, the JPL SSD
  `ura111.bsp` kernel, and their original notices.
- Attribution, EULA, license texts, and exact Eigen source used by the build.
- Documentation links and issue-tracker URLs matching the actual publication.

The root MIT License does not override identified third-party terms.

## Excluded

- Local saves, ad-hoc scenarios/results, newly generated controller DLLs,
  logs, crash dumps, browser caches, user identifiers, and local IDE state.
  Frozen binaries, full histories, and deidentified execution logs are supplied
  in the separate reproducibility bundle. Its manifest is indexed under
  `Validation/Catalog`; restored files remain excluded from source commits.
- Build output, cooked/staged packages, object/debug files, toolchain binaries,
  engine source, and regenerable Visual Studio solution/project files.
- Unused assets, internal working notes, local backups, and private archives.
- Private permission correspondence, credentials, and signing keys/certificates.

`.gitignore` excludes generated and private files. `.gitattributes` tracks large Unreal
assets, images, videos, libraries and kernels with Git LFS. Install Git LFS before
adding these files; confirm storage/bandwidth availability with the host.
Git and Git LFS are separate installations. Confirm `git lfs version` works
in the same terminal used to publish; configuring a filter alone is not
enough. The official installer is available at https://git-lfs.com/.

```powershell
# Only if this folder has not already been initialized as a Git repository:
git init -b main
git lfs version
git lfs install
& .\Tools\Release\Test-Publication.ps1
git add .
git diff --cached --name-only
git lfs status
git status --short
git diff --cached --stat
```

Initialize Git locally if this folder is not already a repository. Review
the staged file list before the first commit and upload. Ignore rules do not
remove a file from existing history or override `git add -f`: if private or
restricted files were previously committed, remove them from history before
publication. Do not publish the private backup archive.

The publication check scans candidate and already tracked files for local-user
paths, common credentials, and forbidden runtime artifacts.
It also checks required notices, the matching MIT license copies, a working
Git LFS executable, LFS rules, and missing LFS payloads. Run it again after
staging. No tool in this procedure commits, creates a remote
repository, or uploads on your behalf.
Review dependency licenses, asset attribution, and the final Git tree
as part of each release; automated file checks do not replace that review.

## Licensing and Citation

Preserve the MIT License and its copyright and permission notices. Request
software citation through `CITATION.cff`. Third-party assets and dependencies,
including Unreal Engine, retain their own terms in source and binary distributions.

The package EULA is not a click-through requirement for reading, building,
or modifying the MIT source. It addresses the independently distributed
application and its bundled third-party components.

## Community Releases

Community maintainers may publish their work without the original author's
approval. Each release must identify its publisher, source commit, changes,
and checks performed. Update installer and application metadata, support
details, release notes, and distribution terms to match that publisher.
Preserve attribution without implying endorsement by other contributors.

Distribute engine and third-party components under their applicable terms.
Keep previous releases and their assessment records identifiable, and record
new results or corrections with the software and inputs that produced them.
Follow the [collaboration guidelines](../../.github/GOVERNANCE.md) and
[release checklist](RELEASE_CHECKLIST.md).

## Installer Downloads and Demo Recordings

Publish the Windows installer and its matching `.sha256` file as GitHub
Release assets. The root README's download button opens the latest published
release. Keep the setup filename and checksum link in the README synchronized
with the release being described. For version 1.0, attach
`PHAROS-1.0-Setup.exe` and `PHAROS-1.0-Setup.exe.sha256`; the checksum is also
stored under `Docs/Release` for verification. Installers remain outside the
source tree.

The README video gallery links to the MP4 files under `Docs/Media/Videos`
through GitHub's LFS media endpoint. Preview frames are under
`Docs/Media/Previews`. Push the LFS payloads along with the source commit and
confirm the demo recordings and installer download are accessible before
announcing the release. Update the gallery URLs if the repository or branch
changes. Original recordings remain at their full resolution and quality.

## Check a Fresh Clone

Fetch Git LFS objects and follow [the build guide](../GettingStarted/Build.md)
to build and open the project. The included Uranus kernel retains the pinned
bytes; `InstallExternalKernels.ps1` can verify it or restore a missing copy.
The script does not replace a different existing local file.

Source publication and application releases are separate. Do not attach an
old packaged application to a new source release without the package review
in the [release checklist](RELEASE_CHECKLIST.md).

Check source structure and scenario paths with
`Tools/Repository/Test-PublicRepository.ps1`. After installing the optional
reproducibility bundle, add `-VerifyEvidence` to check its recorded hashes.
