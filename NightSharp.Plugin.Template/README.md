# NightSharp Plugin Template

Standalone external plugin DLL template.

Build:

```text
MSBuild NightSharp.Plugin.Template.vcxproj /p:Configuration=Release /p:Platform=x64
```

The project imports `..\NightSharp.SDK\build\NightSharp.SDK.props` and exports
`NightSharpGetPluginExports`. Phase 3 loader work will consume this export and
bind plugin menu/runtime services through the host bridge.
