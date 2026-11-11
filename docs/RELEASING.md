# Releases

Publish stable versions as normal releases and mark the current version as Latest.

Each release must include:

- A matching source tag, such as `v1.0.0`.
- `Obsidian-Launcher-v1.0.0-setup.exe` (replace the version for each release).
- `SHA256SUMS.txt` containing the installer's SHA-256 checksum.
- Short release notes in English.

The updater reads this repository's latest stable release, compares semantic versions, and accepts only the matching installer asset with a GitHub SHA-256 digest. It checks the downloaded bytes before allowing installation. Drafts and pre-releases are excluded.

Keep the installer application ID unchanged. Build, test, deploy, package, and verify assets before publishing. No automatic publishing workflow is configured.

Version 1.0.0 starts the stable release series after the historical 2.3 beta. The old beta remains available, but is not part of stable update version comparisons.
