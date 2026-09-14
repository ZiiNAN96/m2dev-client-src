[CmdletBinding()]
param(
    [ValidateNotNullOrEmpty()]
    [string]$Revision = 'HEAD',

    [string]$OutputCsv,

    [string]$DetailCsv
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = (& git rev-parse --show-toplevel).Trim()
if (-not $repositoryRoot) {
    throw 'Run this script from inside the m2dev-client-src Git worktree.'
}

$isWorktree = $Revision -eq 'WORKTREE'
if ($isWorktree) {
    $resolvedRevision = 'WORKTREE'
}
else {
    $resolvedRevision = (& git rev-parse --verify "$Revision^{commit}").Trim()
    if ($LASTEXITCODE -ne 0 -or -not $resolvedRevision) {
        throw "Cannot resolve revision '$Revision'."
    }
}

$sourcePathspecs = @(
    ':(glob)src/**/*.h',
    ':(glob)src/**/*.hpp',
    ':(glob)src/**/*.inl',
    ':(glob)src/**/*.c',
    ':(glob)src/**/*.cc',
    ':(glob)src/**/*.cpp',
    ':(glob)src/**/*.cxx',
    ':(exclude,glob)src/PythonModules/**',
    ':(exclude,glob)src/DumpProto/**',
    ':(exclude,glob)src/PackMaker/**'
)

$headerPathspecs = @(
    ':(glob)src/**/*.h',
    ':(glob)src/**/*.hpp',
    ':(glob)src/**/*.inl',
    ':(exclude,glob)src/PythonModules/**',
    ':(exclude,glob)src/DumpProto/**',
    ':(exclude,glob)src/PackMaker/**'
)

$coreSourcePathspecs = @(
    $sourcePathspecs
    ':(exclude,glob)src/Platform/Windows/**'
    ':(exclude,glob)src/UserInterface/Windows/**'
)

$coreHeaderPathspecs = @(
    $headerPathspecs
    ':(exclude,glob)src/Platform/Windows/**'
    ':(exclude,glob)src/UserInterface/Windows/**'
)

$cmakePathspecs = @(
    'CMakeLists.txt',
    ':(glob)src/**/CMakeLists.txt',
    ':(glob)tests/**/CMakeLists.txt',
    ':(glob)tests/**/*.cmake',
    ':(glob)buildtool/**/*.cmake'
)

$rgSourceArgs = @(
    '-g', '*.{h,hpp,inl,c,cc,cpp,cxx}',
    '-g', '!src/PythonModules/**',
    '-g', '!src/DumpProto/**',
    '-g', '!src/PackMaker/**',
    'src'
)

$rgHeaderArgs = @(
    '-g', '*.{h,hpp,inl}',
    '-g', '!src/PythonModules/**',
    '-g', '!src/DumpProto/**',
    '-g', '!src/PackMaker/**',
    'src'
)

$rgCoreSourceArgs = @(
    '-g', '*.{h,hpp,inl,c,cc,cpp,cxx}',
    '-g', '!src/PythonModules/**',
    '-g', '!src/DumpProto/**',
    '-g', '!src/PackMaker/**',
    '-g', '!src/Platform/Windows/**',
    '-g', '!src/UserInterface/Windows/**',
    'src'
)

$rgCoreHeaderArgs = @(
    '-g', '*.{h,hpp,inl}',
    '-g', '!src/PythonModules/**',
    '-g', '!src/DumpProto/**',
    '-g', '!src/PackMaker/**',
    '-g', '!src/Platform/Windows/**',
    '-g', '!src/UserInterface/Windows/**',
    'src'
)

$rgCMakeArgs = @(
    'CMakeLists.txt',
    'src',
    'tests',
    'buildtool',
    '-g', 'CMakeLists.txt',
    '-g', '*.cmake',
    '-g', '!build/**',
    '-g', '!extern/**',
    '-g', '!vendor/**'
)

$metrics = @(
    # System-header form is intentional. Quoted project shims belong in the curated inventory.
    [pscustomobject]@{ Name = 'windows_header_includes'; Scope = 'source'; Pattern = '^\s*#\s*include\s*<(?:[^>]*[\\/])?(?i:windows\.h)>' },
    [pscustomobject]@{ Name = 'windows_family_header_includes'; Scope = 'source'; Pattern = '^\s*#\s*include\s*<(?:[^>]*[\\/])?(?i:windows\.h|windowsx\.h|dinput(?:8)?\.h|winsock2?\.h|ws2tcpip\.h|shellapi\.h|shlobj\.h|commctrl\.h|dbghelp\.h|tlhelp32\.h|psapi\.h|wincrypt\.h|process\.h|direct\.h|io\.h|mmsystem\.h|vfw\.h|dshow\.h|strmif\.h|objbase\.h|ole2\.h|imm\.h|wininet\.h)>' },
    [pscustomobject]@{ Name = 'windows_family_header_includes_in_headers'; Scope = 'headers'; Pattern = '^\s*#\s*include\s*<(?:[^>]*[\\/])?(?i:windows\.h|windowsx\.h|dinput(?:8)?\.h|winsock2?\.h|ws2tcpip\.h|shellapi\.h|shlobj\.h|commctrl\.h|dbghelp\.h|tlhelp32\.h|psapi\.h|wincrypt\.h|process\.h|direct\.h|io\.h|mmsystem\.h|vfw\.h|dshow\.h|strmif\.h|objbase\.h|ole2\.h|imm\.h|wininet\.h)>' },
    [pscustomobject]@{ Name = 'directinput_header_includes'; Scope = 'source'; Pattern = '^\s*#\s*include\s*<(?:[^>]*[\\/])?(?i:dinput(?:8)?\.h)>' },
    [pscustomobject]@{ Name = 'winsock_header_includes'; Scope = 'source'; Pattern = '^\s*#\s*include\s*<(?:[^>]*[\\/])?(?i:winsock2?\.h|ws2tcpip\.h)>' },
    [pscustomobject]@{ Name = 'shell_header_includes'; Scope = 'source'; Pattern = '^\s*#\s*include\s*<(?:[^>]*[\\/])?(?i:shellapi\.h|shlobj\.h)>' },
    [pscustomobject]@{ Name = 'diagnostics_header_includes'; Scope = 'source'; Pattern = '^\s*#\s*include\s*<(?:[^>]*[\\/])?(?i:dbghelp\.h|tlhelp32\.h|psapi\.h)>' },
    [pscustomobject]@{ Name = 'media_header_includes'; Scope = 'source'; Pattern = '^\s*#\s*include\s*<(?:[^>]*[\\/])?(?i:mmsystem\.h|vfw\.h|dshow\.h|strmif\.h|objbase\.h|ole2\.h)>' },
    [pscustomobject]@{ Name = 'windows_native_handle_types'; Scope = 'source'; Pattern = '\b(?:HWND|HINSTANCE|HANDLE|HMODULE|WNDPROC|WPARAM|LPARAM|LRESULT)\b' },
    [pscustomobject]@{ Name = 'windows_native_handle_types_in_headers'; Scope = 'headers'; Pattern = '\b(?:HWND|HINSTANCE|HANDLE|HMODULE|WNDPROC|WPARAM|LPARAM|LRESULT)\b' },
    [pscustomobject]@{ Name = 'window_event_api_calls'; Scope = 'source'; Pattern = '\b(?:PeekMessage[AW]?|GetMessage[AW]?|TranslateMessage|DispatchMessage[AW]?|CreateWindow(?:Ex)?[AW]?|RegisterClass(?:Ex)?[AW]?|DestroyWindow|GetClientRect|SetWindowPos|ShowWindow|SetFocus|GetFocus|IsIconic|DefWindowProc[AW]?|PostQuitMessage)\s*\(' },
    [pscustomobject]@{ Name = 'time_sleep_api_calls'; Scope = 'source'; Pattern = '\b(?:QueryPerformanceCounter|QueryPerformanceFrequency|GetTickCount64?|timeGetTime|Sleep(?:Ex)?|WaitForSingleObject)\s*\(' },
    [pscustomobject]@{ Name = 'win32_thread_sync_api_calls'; Scope = 'source'; Pattern = '\b(?:CreateThread|_beginthreadex?|InitializeCriticalSection|DeleteCriticalSection|EnterCriticalSection|LeaveCriticalSection|CreateEvent[AW]?|SetEvent|ResetEvent|WaitForSingleObject)\s*\(' },
    [pscustomobject]@{ Name = 'winsock_api_calls'; Scope = 'source'; Pattern = '\b(?:WSAStartup|WSACleanup|WSAGetLastError|socket|closesocket|ioctlsocket|getaddrinfo|freeaddrinfo|connect|recv|send|select)\s*\(' },
    [pscustomobject]@{ Name = 'winsock_native_types_in_headers'; Scope = 'headers'; Pattern = '\b(?:SOCKET|WSADATA|sockaddr|sockaddr_in)\b' },
    [pscustomobject]@{ Name = 'dynamic_library_api_calls'; Scope = 'source'; Pattern = '\b(?:LoadLibrary(?:Ex)?[AW]?|GetProcAddress|FreeLibrary)\s*\(' },
    [pscustomobject]@{ Name = 'registry_api_calls'; Scope = 'source'; Pattern = '\b(?:RegOpenKeyEx[AW]?|RegCreateKeyEx[AW]?|RegQueryValueEx[AW]?|RegSetValueEx[AW]?|RegCloseKey)\s*\(' },
    [pscustomobject]@{ Name = 'cursor_clipboard_shell_api_calls'; Scope = 'source'; Pattern = '\b(?:ShellExecute[AW]?|SHGetFolderPath[AW]?|SHGetKnownFolderPath|OpenClipboard|CloseClipboard|GetClipboardData|SetClipboardData|EmptyClipboard|ClipCursor|ShowCursor|SetCursor|LoadCursor[AW]?|GetCursorPos|SetCursorPos|SetCapture|ReleaseCapture)\s*\(' },
    [pscustomobject]@{ Name = 'win32_file_api_calls'; Scope = 'source'; Pattern = '\b(?:CreateFile[AW]?|ReadFile|WriteFile|GetFileSize(?:Ex)?|FindFirstFile[AW]?|FindNextFile[AW]?|FindClose|GetFileAttributes[AW]?|SetCurrentDirectory[AW]?|GetCurrentDirectory[AW]?|GetModuleFileName[AW]?|CreateDirectory[AW]?|DeleteFile[AW]?|MoveFile[AW]?|CopyFile[AW]?|GetTempPath[AW]?)\s*\(' },
    [pscustomobject]@{ Name = 'current_directory_api_calls'; Scope = 'source'; Pattern = '\b(?:SetCurrentDirectory[AW]?|GetCurrentDirectory[AW]?|_chdir|_wchdir|getcwd|_getcwd|_wgetcwd)\s*\(' },
    [pscustomobject]@{ Name = 'c_stdio_file_calls'; Scope = 'source'; Pattern = '\b(?:fopen|_wfopen|freopen|_wfreopen)\s*\(' },
    [pscustomobject]@{ Name = 'std_file_stream_uses'; Scope = 'source'; Pattern = '\b(?:std::)?(?:ifstream|ofstream|fstream)\b' },
    [pscustomobject]@{ Name = 'std_filesystem_uses'; Scope = 'source'; Pattern = '\bstd::filesystem(?:::|\b)' },
    [pscustomobject]@{ Name = 'backslash_path_tokens'; Scope = 'source'; Pattern = '[A-Za-z0-9_%{}.-]+\\\\[A-Za-z0-9_%{}.-]+' },
    [pscustomobject]@{ Name = 'absolute_drive_path_literals'; Scope = 'source'; Pattern = '[A-Za-z]:\\\\' },
    [pscustomobject]@{ Name = 'directinput_symbols'; Scope = 'source'; Pattern = '\b(?:DirectInput8Create|IDirectInput8|IDirectInputDevice8|DIMOUSESTATE|DIK_[A-Z0-9_]+|DISCL_[A-Z0-9_]+)\b' },
    [pscustomobject]@{ Name = 'webview2_symbols'; Scope = 'source'; Pattern = '\b(?:ICoreWebView2[A-Za-z0-9_]*|CreateCoreWebView2EnvironmentWithOptions|CoreWebView2EnvironmentOptions)\b' },
    [pscustomobject]@{ Name = 'directshow_video_symbols'; Scope = 'source'; Pattern = '\b(?:IGraphBuilder|IMediaControl|IMediaEvent|IMultiMediaStream|IDirectDrawMediaStream|AMMultiMediaStream|CoCreateInstance)\b' },
    [pscustomobject]@{ Name = 'debug_diagnostics_api_calls'; Scope = 'source'; Pattern = '\b(?:OutputDebugString[AW]?|IsDebuggerPresent|DebugBreak|MiniDumpWriteDump|StackWalk64|SymInitialize|SymCleanup|CreateToolhelp32Snapshot)\s*\(' },
    [pscustomobject]@{ Name = 'platform_preprocessor_directives'; Scope = 'source'; Pattern = '^\s*#\s*(?:if|ifdef|ifndef|elif)\b[^\r\n]*(?:_WIN32|\bWIN32\b|_WINDOWS|_WIN64|_M_X64|_M_IX86)' },
    [pscustomobject]@{ Name = 'msvc_preprocessor_directives'; Scope = 'source'; Pattern = '^\s*#\s*(?:if|ifdef|ifndef|elif)\b[^\r\n]*(?:_MSC_VER|_MSVC_LANG)' },
    [pscustomobject]@{ Name = 'cmake_win32_conditions'; Scope = 'cmake'; Pattern = '(?i)\b(?:if|elseif)\s*\(\s*(?:NOT\s+)?WIN32\b' },
    [pscustomobject]@{ Name = 'cmake_msvc_conditions'; Scope = 'cmake'; Pattern = '(?i)\b(?:if|elseif)\s*\(\s*(?:NOT\s+)?MSVC\b' },
    [pscustomobject]@{ Name = 'cmake_msvc_settings'; Scope = 'cmake'; Pattern = '\b(?:CMAKE_MSVC_[A-Z0-9_]+|CMAKE_CXX_COMPILER_ARCHITECTURE_ID|CXX_COMPILER_ID:MSVC|COMPILE_LANG_AND_ID:CXX,MSVC|DILIGENT_MSVC_[A-Z0-9_]+)\b' },
    [pscustomobject]@{ Name = 'cmake_windows_link_items'; Scope = 'cmake'; Pattern = '(?i)\b(?:WindowsInput|WebView|ws2_32|strmiids|amstrmid|dmoguids|ddraw|winmm|Dbghelp|shell32|imm32|dinput8|dxguid)\b' },
    [pscustomobject]@{ Name = 'cmake_recursive_source_globs'; Scope = 'cmake'; Pattern = '(?i)\bfile\s*\(\s*GLOB_RECURSE\b' },
    [pscustomobject]@{ Name = 'cmake_win32_executable_targets'; Scope = 'cmake'; Pattern = '(?i)\badd_executable\s*\([^\r\n]*\bWIN32\b' }
)

function Get-Pathspecs([string]$Scope) {
    switch ($Scope) {
        'source' { return $sourcePathspecs }
        'headers' { return $headerPathspecs }
        'core' { return $coreSourcePathspecs }
        'core_headers' { return $coreHeaderPathspecs }
        'cmake' { return $cmakePathspecs }
        default { throw "Unknown audit scope '$Scope'." }
    }
}

function Get-RgArgs([string]$Scope) {
    switch ($Scope) {
        'source' { return $rgSourceArgs }
        'headers' { return $rgHeaderArgs }
        'core' { return $rgCoreSourceArgs }
        'core_headers' { return $rgCoreHeaderArgs }
        'cmake' { return $rgCMakeArgs }
        default { throw "Unknown audit scope '$Scope'." }
    }
}

function Invoke-AuditSearch([string]$Pattern, [string]$Scope, [ValidateSet('lines', 'matches', 'files')] [string]$Mode) {
    if ($isWorktree) {
        $arguments = @('--no-heading', '--color', 'never', '--no-messages', '-P')
        switch ($Mode) {
            'lines' { $arguments += '-n' }
            'matches' { $arguments += @('-n', '-o') }
            'files' { $arguments += '-l' }
        }
        $arguments += $Pattern
        $arguments += Get-RgArgs $Scope
        $result = @(& rg @arguments)
        if ($LASTEXITCODE -gt 1) {
            throw "ripgrep failed for pattern '$Pattern'."
        }
        return $result
    }

    $arguments = @('grep', '-I', '-P')
    switch ($Mode) {
        'lines' { $arguments += '-n' }
        'matches' { $arguments += @('-n', '-o') }
        'files' { $arguments += '-l' }
    }
    $arguments += $Pattern
    $arguments += $resolvedRevision
    $arguments += '--'
    $arguments += Get-Pathspecs $Scope
    $result = @(& git @arguments 2>$null)
    if ($LASTEXITCODE -gt 1) {
        throw "git grep failed for pattern '$Pattern'."
    }
    return $result
}

function Get-ScopeFiles([string]$Scope) {
    if ($isWorktree) {
        $trackedAndUntracked = @(& git ls-files --cached --others --exclude-standard)
    }
    else {
        $trackedAndUntracked = @(& git ls-tree -r --name-only $resolvedRevision)
    }

    switch ($Scope) {
        'source' {
            return @($trackedAndUntracked | Where-Object {
                $_ -match '^src/.+\.(?:h|hpp|inl|c|cc|cpp|cxx)$' -and
                $_ -notmatch '^src/(?:PythonModules|DumpProto|PackMaker)/'
            })
        }
        'headers' {
            return @($trackedAndUntracked | Where-Object {
                $_ -match '^src/.+\.(?:h|hpp|inl)$' -and
                $_ -notmatch '^src/(?:PythonModules|DumpProto|PackMaker)/'
            })
        }
        'core' {
            return @($trackedAndUntracked | Where-Object {
                $_ -match '^src/.+\.(?:h|hpp|inl|c|cc|cpp|cxx)$' -and
                $_ -notmatch '^src/(?:PythonModules|DumpProto|PackMaker|Platform/Windows|UserInterface/Windows)/'
            })
        }
        'core_headers' {
            return @($trackedAndUntracked | Where-Object {
                $_ -match '^src/.+\.(?:h|hpp|inl)$' -and
                $_ -notmatch '^src/(?:PythonModules|DumpProto|PackMaker|Platform/Windows|UserInterface/Windows)/'
            })
        }
        'cmake' {
            return @($trackedAndUntracked | Where-Object {
                $_ -eq 'CMakeLists.txt' -or
                $_ -match '^(?:src|tests)/.+/CMakeLists\.txt$' -or
                $_ -match '^tests/.+\.cmake$' -or
                $_ -match '^buildtool/.+\.cmake$'
            })
        }
        default { throw "Unknown audit scope '$Scope'." }
    }
}

function ConvertTo-DetailRow([string]$Metric, [string]$Scope, [string]$Line) {
    $expression = if ($isWorktree) {
        '^(?<file>[^:]+):(?<line>\d+):(?<text>.*)$'
    }
    else {
        '^[^:]+:(?<file>[^:]+):(?<line>\d+):(?<text>.*)$'
    }

    if ($Line -notmatch $expression) {
        throw "Cannot parse audit output: $Line"
    }

    return [pscustomobject]@{
        Revision = $resolvedRevision
        Scope = $Scope
        Metric = $Metric
        File = $Matches.file -replace '\\', '/'
        Line = [int]$Matches.line
        Text = $Matches.text.Trim()
    }
}

Push-Location $repositoryRoot
try {
    $rows = [System.Collections.Generic.List[object]]::new()
    $details = [System.Collections.Generic.List[object]]::new()

    foreach ($scope in @('source', 'headers', 'core', 'core_headers', 'cmake')) {
        $files = @(Get-ScopeFiles $scope | Sort-Object -Unique)
        $rows.Add([pscustomobject]@{
            Revision = $resolvedRevision
            Scope = $scope
            Metric = 'audited_files'
            Occurrences = $files.Count
            MatchingLines = $files.Count
            Files = $files.Count
        })
    }

    $platformFiles = @(Get-ScopeFiles 'source' | Where-Object { ($_ -replace '\\', '/') -like 'src/Platform/*' })
    $rows.Add([pscustomobject]@{
        Revision = $resolvedRevision
        Scope = 'source'
        Metric = 'platform_layer_files'
        Occurrences = $platformFiles.Count
        MatchingLines = $platformFiles.Count
        Files = $platformFiles.Count
    })

    foreach ($metric in $metrics) {
        $auditScopes = @($metric.Scope)
        if ($metric.Scope -eq 'source') {
            $auditScopes += 'core'
        }
        elseif ($metric.Scope -eq 'headers') {
            $auditScopes += 'core_headers'
        }

        foreach ($auditScope in $auditScopes) {
            $matchingLines = @(Invoke-AuditSearch $metric.Pattern $auditScope 'lines')
            $matchingOccurrences = @(Invoke-AuditSearch $metric.Pattern $auditScope 'matches')
            $matchingFiles = @(Invoke-AuditSearch $metric.Pattern $auditScope 'files')

            $rows.Add([pscustomobject]@{
                Revision = $resolvedRevision
                Scope = $auditScope
                Metric = $metric.Name
                Occurrences = $matchingOccurrences.Count
                MatchingLines = $matchingLines.Count
                Files = $matchingFiles.Count
            })

            if ($DetailCsv) {
                foreach ($line in $matchingLines) {
                    $details.Add((ConvertTo-DetailRow $metric.Name $auditScope $line))
                }
            }
        }
    }

    if ($OutputCsv) {
        $outputPath = if ([System.IO.Path]::IsPathRooted($OutputCsv)) { $OutputCsv } else { Join-Path $repositoryRoot $OutputCsv }
        $rows | Export-Csv -LiteralPath $outputPath -NoTypeInformation -Encoding utf8
    }

    if ($DetailCsv) {
        $detailPath = if ([System.IO.Path]::IsPathRooted($DetailCsv)) { $DetailCsv } else { Join-Path $repositoryRoot $DetailCsv }
        $details | Export-Csv -LiteralPath $detailPath -NoTypeInformation -Encoding utf8
    }

    $rows | Format-Table Scope, Metric, Occurrences, MatchingLines, Files -AutoSize
}
finally {
    Pop-Location
}
