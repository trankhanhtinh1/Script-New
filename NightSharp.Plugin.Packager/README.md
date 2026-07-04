# NightSharp Plugin Packager

Build:

```bat
MSBuild NightSharp.Plugin.Packager.sln /p:Configuration=Release /p:Platform=x64
```

Pack a release plugin:

Double-click `NightSharp.Plugin.Packager.exe`, select a plugin DLL, and the tool
will create a `.NS` file next to that DLL.

Command-line usage is still available:

```bat
NightSharp.Plugin.Packager.exe ..\NightSharp.Plugin.Template\bin\Release\NightSharp.Plugin.Template.dll
```

With explicit output and metadata:

```bat
NightSharp.Plugin.Packager.exe MyPlugin.dll "%AppData%\NightSharp\Plugins\MyPlugin.NS" --version 1.0.0 --deps "SomeDependency.dll"
```

Release plugins go in:

```text
%AppData%\NightSharp\Plugins
```

Raw DLL development builds go in:

```text
%AppData%\NightSharp\Plugins\Dev
```

Debug builds enable raw DLL developer loading by default. Release builds require
`NIGHTSHARP_ENABLE_DEV_PLUGINS=1`; otherwise only `.NS` packages are loaded.

The `.NS` format stores plugin metadata, SDK ABI, integrity hashes, a keyed package signature, and an encrypted DLL payload. This is the first release container layer; heavier protection can be added later without changing the plugin ABI.
