param(
    [switch]$IncludeDx9Backend
)

$ErrorActionPreference = 'Stop'

$ProjectDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectFile = Join-Path $ProjectDir 'NightSharp.vcxproj'
$FiltersFile = Join-Path $ProjectDir 'NightSharp.vcxproj.filters'
$MsbuildNs = 'http://schemas.microsoft.com/developer/msbuild/2003'

$HeaderExtensions = @('.h', '.hh', '.hpp', '.hxx', '.inl')
$SourceExtensions = @('.c', '.cc', '.cpp', '.cxx')
$MasmExtensions = @('.asm')
$ExcludedDirectories = @('.vs', '.codemap', 'bin', 'obj', 'Debug', 'Release', 'x64')
$ExcludedFiles = @()

if (-not $IncludeDx9Backend) {
    $ExcludedFiles += 'imgui\imgui_impl_dx9.cpp'
}

function Get-RelativeProjectPath {
    param([Parameter(Mandatory)][string]$FullPath)

    $baseUri = [Uri]((Join-Path $ProjectDir '') -replace '\\', '/')
    $fileUri = [Uri]($FullPath -replace '\\', '/')
    $relativePath = [Uri]::UnescapeDataString($baseUri.MakeRelativeUri($fileUri).ToString()) -replace '/', '\'
    $parts = $relativePath -split '\\'

    switch ($parts[0].ToLowerInvariant()) {
        'core' { $parts[0] = 'Core' }
        'plugins' { $parts[0] = 'Plugins' }
        'sdk' { $parts[0] = 'SDK' }
        'third_party' { $parts[0] = 'Third_Party' }
        'menu' { $parts[0] = 'menu' }
        'libs' { $parts[0] = 'libs' }
        'imgui' { $parts[0] = 'imgui' }
        'overlay' { $parts[0] = 'overlay' }
    }

    $parts -join '\'
}

function Test-IsExcluded {
    param(
        [Parameter(Mandatory)][IO.FileInfo]$File,
        [Parameter(Mandatory)][string]$RelativePath
    )

    if ($ExcludedFiles -contains $RelativePath) {
        return $true
    }

    $relativeParts = $RelativePath -split '\\'
    foreach ($part in $relativeParts) {
        if ($ExcludedDirectories -contains $part) {
            return $true
        }
    }

    return $false
}

function Get-ProjectItems {
    $items = @{
        ClCompile = [System.Collections.Generic.List[string]]::new()
        ClInclude = [System.Collections.Generic.List[string]]::new()
        MASM = [System.Collections.Generic.List[string]]::new()
    }

    Get-ChildItem -LiteralPath $ProjectDir -File -Recurse | ForEach-Object {
        $relativePath = Get-RelativeProjectPath $_.FullName
        if (Test-IsExcluded -File $_ -RelativePath $relativePath) {
            return
        }

        $extension = $_.Extension.ToLowerInvariant()
        if ($HeaderExtensions -contains $extension) {
            $items.ClInclude.Add($relativePath)
        } elseif ($SourceExtensions -contains $extension) {
            $items.ClCompile.Add($relativePath)
        } elseif ($MasmExtensions -contains $extension) {
            $items.MASM.Add($relativePath)
        }
    }

    foreach ($key in @('ClCompile', 'ClInclude', 'MASM')) {
        $items[$key] = @($items[$key] | Sort-Object -Unique)
    }

    $items
}

function New-MsbuildElement {
    param(
        [Parameter(Mandatory)][xml]$Document,
        [Parameter(Mandatory)][string]$Name
    )

    $Document.CreateElement($Name, $MsbuildNs)
}

function New-ProjectItemGroup {
    param(
        [Parameter(Mandatory)][xml]$Document,
        [Parameter(Mandatory)][string]$ItemName,
        [Parameter(Mandatory)][string[]]$Includes
    )

    $group = New-MsbuildElement -Document $Document -Name 'ItemGroup'
    foreach ($include in $Includes) {
        $item = New-MsbuildElement -Document $Document -Name $ItemName
        $item.SetAttribute('Include', $include)
        [void]$group.AppendChild($item)
    }

    $group
}

function Remove-ManagedProjectItemGroups {
    param([Parameter(Mandatory)][xml]$Document)

    $managedItemNames = @('ClCompile', 'ClInclude', 'MASM')
    $groups = @($Document.Project.ItemGroup)

    foreach ($group in $groups) {
        if ($group.Label) {
            continue
        }

        $children = @($group.ChildNodes | Where-Object { $_.NodeType -eq [System.Xml.XmlNodeType]::Element })
        if ($children.Count -eq 0) {
            continue
        }

        $managedChildren = @($children | Where-Object { $managedItemNames -contains $_.LocalName })
        if ($managedChildren.Count -eq $children.Count) {
            [void]$group.ParentNode.RemoveChild($group)
        }
    }
}

function Save-XmlDocument {
    param(
        [Parameter(Mandatory)][xml]$Document,
        [Parameter(Mandatory)][string]$Path
    )

    $settings = [System.Xml.XmlWriterSettings]::new()
    $settings.Encoding = [System.Text.UTF8Encoding]::new($false)
    $settings.Indent = $true
    $settings.NewLineChars = "`r`n"

    $writer = [System.Xml.XmlWriter]::Create($Path, $settings)
    try {
        $Document.Save($writer)
    } finally {
        $writer.Dispose()
    }
}

function Update-ProjectFile {
    param([Parameter(Mandatory)]$Items)

    [xml]$document = Get-Content -LiteralPath $ProjectFile -Raw
    Remove-ManagedProjectItemGroups -Document $document

    $anchor = $document.SelectSingleNode("//*[local-name()='Import' and @Project='`$(VCTargetsPath)\Microsoft.Cpp.targets']")
    if (-not $anchor) {
        throw 'Could not find Microsoft.Cpp.targets import in NightSharp.vcxproj.'
    }

    $groups = @(
        New-ProjectItemGroup -Document $document -ItemName 'ClCompile' -Includes $Items.ClCompile
        New-ProjectItemGroup -Document $document -ItemName 'MASM' -Includes $Items.MASM
        New-ProjectItemGroup -Document $document -ItemName 'ClInclude' -Includes $Items.ClInclude
    )

    foreach ($group in $groups) {
        if ($group.ChildNodes.Count -gt 0) {
            [void]$document.Project.InsertBefore($group, $anchor)
        }
    }

    Save-XmlDocument -Document $document -Path $ProjectFile
}

function Get-FilterPath {
    param([Parameter(Mandatory)][string]$IncludePath)

    $directory = Split-Path $IncludePath -Parent
    if ([string]::IsNullOrWhiteSpace($directory) -or $directory -eq '.') {
        return $null
    }

    $directory -replace '/', '\'
}

function Get-FilterGuid {
    param([Parameter(Mandatory)][string]$FilterPath)

    $bytes = [Text.Encoding]::UTF8.GetBytes("NightSharp:$FilterPath")
    $hash = [System.Security.Cryptography.MD5]::Create().ComputeHash($bytes)
    $guid = New-Object Guid (,$hash)
    '{' + $guid.ToString().ToUpperInvariant() + '}'
}

function Get-AllFilterPaths {
    param([Parameter(Mandatory)]$Items)

    $filters = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($include in @($Items.ClCompile + $Items.MASM + $Items.ClInclude)) {
        $filter = Get-FilterPath $include
        while (-not [string]::IsNullOrWhiteSpace($filter)) {
            [void]$filters.Add($filter)
            $filter = Split-Path $filter -Parent
            if ($filter -eq '.') {
                $filter = $null
            }
        }
    }

    @($filters | Sort-Object)
}

function New-FiltersDefinitionGroup {
    param(
        [Parameter(Mandatory)][xml]$Document,
        [Parameter(Mandatory)][string[]]$Filters
    )

    $group = New-MsbuildElement -Document $Document -Name 'ItemGroup'
    foreach ($filterPath in $Filters) {
        $filter = New-MsbuildElement -Document $Document -Name 'Filter'
        $filter.SetAttribute('Include', $filterPath)

        $id = New-MsbuildElement -Document $Document -Name 'UniqueIdentifier'
        $id.InnerText = Get-FilterGuid $filterPath
        [void]$filter.AppendChild($id)
        [void]$group.AppendChild($filter)
    }

    $group
}

function New-FiltersItemGroup {
    param(
        [Parameter(Mandatory)][xml]$Document,
        [Parameter(Mandatory)][string]$ItemName,
        [Parameter(Mandatory)][string[]]$Includes
    )

    $group = New-MsbuildElement -Document $Document -Name 'ItemGroup'
    foreach ($include in $Includes) {
        $item = New-MsbuildElement -Document $Document -Name $ItemName
        $item.SetAttribute('Include', $include)

        $filterPath = Get-FilterPath $include
        if ($filterPath) {
            $filter = New-MsbuildElement -Document $Document -Name 'Filter'
            $filter.InnerText = $filterPath
            [void]$item.AppendChild($filter)
        }

        [void]$group.AppendChild($item)
    }

    $group
}

function Update-FiltersFile {
    param([Parameter(Mandatory)]$Items)

    [xml]$document = Get-Content -LiteralPath $FiltersFile -Raw
    @($document.Project.ItemGroup) | ForEach-Object {
        [void]$_.ParentNode.RemoveChild($_)
    }

    $groups = @(
        New-FiltersDefinitionGroup -Document $document -Filters (Get-AllFilterPaths -Items $Items)
        New-FiltersItemGroup -Document $document -ItemName 'ClCompile' -Includes $Items.ClCompile
        New-FiltersItemGroup -Document $document -ItemName 'MASM' -Includes $Items.MASM
        New-FiltersItemGroup -Document $document -ItemName 'ClInclude' -Includes $Items.ClInclude
    )

    foreach ($group in $groups) {
        if ($group.ChildNodes.Count -gt 0) {
            [void]$document.Project.AppendChild($group)
        }
    }

    Save-XmlDocument -Document $document -Path $FiltersFile
}

$items = Get-ProjectItems
Update-ProjectFile -Items $items
Update-FiltersFile -Items $items

Write-Host "Synced NightSharp.vcxproj"
Write-Host "  Sources: $($items.ClCompile.Count)"
Write-Host "  Headers: $($items.ClInclude.Count)"
Write-Host "  MASM:    $($items.MASM.Count)"
Write-Host "  Filters: $((Get-AllFilterPaths -Items $items).Count)"
