# Building Obsidian


You need Visual Studio 2022 with Desktop development with C++, CMake 3.24 or later, and a matching Qt 6 MSVC x64 kit. Qt 6.8.3 is recommended; include Qt Test to run the tests.

In Developer PowerShell for VS 2022:

```powershell
.\build.ps1 -QtPath 'C:\Qt\6.8.3\msvc2022_64'
.\dist\ObsidianLauncher.exe
```

Or run the steps separately:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64" -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
$env:PATH = 'C:\Qt\6.8.3\msvc2022_64\bin;' + $env:PATH
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix dist
```

CMake downloads the pinned zlib 1.3.1 source archive on the first configuration. For an offline build, provide an extracted copy with `-DFETCHCONTENT_SOURCE_DIR_ZLIB=C:/deps/zlib-1.3.1`.

Distribute the whole `dist` folder, including Qt libraries, plugins, licenses, and the required Microsoft Visual C++ runtime. See [installer instructions](../installer/README.md) and [release instructions](RELEASING.md).

