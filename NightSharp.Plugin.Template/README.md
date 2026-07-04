# NightSharp Plugin Template

Standalone external plugin DLL template.

Build:

```text
MSBuild NightSharp.Plugin.Template.vcxproj /p:Configuration=Release /p:Platform=x64
```

The project imports `..\NightSharp.SDK\build\NightSharp.SDK.props` and exports
`NightSharpGetPluginExports`.

Developer mode can load the raw DLL from:

```text
%AppData%\NightSharp\Plugins\Dev
```

Debug builds enable this by default. Release builds require:

```text
NIGHTSHARP_ENABLE_DEV_PLUGINS=1
```

Release mode should pack the DLL into `.NS`:

```text
..\NightSharp.Plugin.Packager\bin\Release\NightSharp.Plugin.Packager.exe bin\Release\NightSharp.Plugin.Template.dll
```

NightSharp loads release packages from:

```text
%AppData%\NightSharp\Plugins
```
