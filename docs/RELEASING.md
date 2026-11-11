# Publishing a release

Publish a Windows setup executable with every release. Keep installers in GitHub Release assets rather than committing them to the repository.

For the first public beta:

- Tag: `v2.3.0-beta.1`
- Title: `Obsidian Launcher 2.3 Beta`
- Installer: `Obsidian-Beta-v2.3-setup.exe`
- Checksum file: `SHA256SUMS.txt`
- GitHub release type: pre-release

This release contains the existing 2.3 installer. Renaming the file does not change the executable or its internal version.

## Future releases

1. Build and test the intended version, deploy the full Windows package, and compile its installer.
2. Commit the matching source and documentation. Keep the existing installer application ID so updates recognize previous installations.
3. Tag that commit with the version, for example `v3.0.0-beta.1` for a beta or `v3.0.0` for a stable release.
4. Attach the setup executable and its SHA-256 checksum file. Write release notes in plain English, including known limitations.
5. Check the uploaded file name, size, and checksum before publishing. Mark betas as pre-releases.

The first release is being published manually; no automatic build or release workflow has been set up.

## Future automatic updates

Version 2.3 does not check for updates, download releases, or run installers automatically. Attaching a setup file to a release provides the download, but the launcher still needs an update checker and a verified installation flow.

Stable updates can use GitHub's latest-release endpoint. That endpoint excludes drafts and pre-releases. A beta update channel must list releases, exclude drafts, and compare version tags explicitly. Select the intended setup asset by a documented naming rule, rather than relying on asset order.

Sources: [GitHub Releases API](https://docs.github.com/en/rest/releases/releases), [GitHub CLI release command](https://cli.github.com/manual/gh_release_create).
