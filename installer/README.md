# Windows installer

The installer includes the complete Windows package: the launcher, Qt libraries and plugins, Microsoft Visual C++ runtime files, documentation, and dependency licenses. Minecraft and Java must already be installed.

It creates a Start menu entry and offers an optional desktop shortcut. A per-user installation does not require administrator rights; an all-users installation can be selected. The installer is unsigned.

## Build

After building and deploying the Windows package, compile `Obsidian.iss` with Inno Setup 6.7.3 or later:

```powershell
ISCC.exe /DPackageDir="C:\path\to\dist" /DInstallerOutput="C:\path\to\release" .\installer\Obsidian.iss
```

Without overrides, the script expects a `Windows` folder two levels above `installer` and places the setup file alongside it. The existing script produces `Obsidian-v2.3-setup.exe`. The first GitHub beta uses an identical copy named `Obsidian-Beta-v2.3-setup.exe`.

## Updates

The application ID remains `{6D964CAB-6450-45F6-A095-31CC146B7D17}` so the installer can recognize existing installations. It reuses the registered installation folder, shortcuts, and installation mode. When an installation is found, the folder and shortcut pages are skipped; an alternate `/DIR` does not move it.

Close Obsidian and finish any running game before installing an update. There is no automatic update checker in version 2.3. Publish a setup executable with each GitHub release so users can install updates manually until that feature is added.
