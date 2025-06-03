@echo off
REM Format C++ files in multiple directories using clang-format

REM Get the directory of this script
set "script_dir=%~dp0"
set "script_dir=%script_dir:~0,-1%"

REM Define directories to format
set "dirs=sources"

REM Loop through each target directory
for %%D in (%dirs%) do (
    pushd "%script_dir%\..\%%D"
    set "current_dir=%cd%"
    echo Running clang-format in: %current_dir%

    REM Format all .cpp and .h files (non-recursive)
    for %%F in (*.cpp *.h) do (
        echo Formatting %%F
        clang-format -i "%%F"
    )

    popd
)

echo Clang-format completed.
