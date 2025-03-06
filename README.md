# NOVA
Neuromorphic Optics Visualization Application

# Table of Contents
1. [Dev Setup](#dev-setup)
2. [Running](#running)
3. [TODO](#todo)

# Dev Setup
## Dependencies
As a preface, you should
1. Clone somewhere in your `C:` drive (so not in `WSL`). Then if you use VSCode you can open a split terminal with powershell.exe on the left and WSL2 on the right.
2. Try to run your `git` commands in a `WSL` terminal, not `PowerShell`, to prevent line carriage return [issues](https://docs.github.com/en/get-started/git-basics/configuring-git-to-handle-line-endings#re-normalizing-a-repository). `.gitattributes` exists, so this should be mitigated.
3. Expect installs (i.e. `vcpkg`) to require a restart of the terminal / `VSCode` instance.

### VCPKG
You don't have to make/place it in `C:\Dev\`, that's just what I use because it is convenient.
```
git clone https://github.com/microsoft/vcpkg.git C:\Dev\vcpkg
cd C:\Dev\vcpkg
git checkout -b fmt-10.2.1 b8ec6abf5d
.\bootstrap-vcpkg.bat
```

1. Make an environment variable `VCPKG_ROOT` so you can access it with `$env:VCPKG_ROOT$` anywhere in powershell / `%VCPKG_ROOT%` in `cmd.exe`.
2. Add `C:\Dev\vcpkg` to your PATH as well, so you can run `vcpkg` commands from anywhere. For example, I have a key value pair in my PATH as `VCPKG_ROOT` `C:\Dev\vcpkg`.

`fmt` is problematic. If you `cd $env:VCPKG_ROOT` and `git log --oneline --grep="\[fmt\]"` you will see we need to step back to the proper vcpkg hash, to enforce lower usage. Otherwise, we get a conflict when using `dv-processing` which erroneously states any version of fmt `>=8.1.1` works. Hence the checkout to an older version of `vcpkg` that supports it.

### VS 2017/2022
For MSVC, download [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/).

Technically, you don't need the IDE, just the MSVC compiler. You should check mark these items when you have downloaded and opened the installer:
- Desktop development with C++
- For safety get MSVC 142 as well

### CMake
You will need [cmake](https://cmake.org/download/) as well if you do not have it.

Note that anything that updates your path variable will not 

### Package Installation
The `--triplet=x64-windows` will install these libraries to be used in your system. If you want them local to the project (not recommended) you should remove that. 

1. Run `vcpkg install --triplet=x64-windows dv-processing glew glfw3 glm eigen3` in the project root (this can take up to an hour, run this and come back in a while).
2. You will need [cmake](https://cmake.org/download/) as well if you do not have it.

### Initialize Submodules
1. Run `git submodule update --init`

### Building
`cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DCMAKE_BUILD_TYPE=Release` builds in release mode, which doesn't come with debugging symbols.

# Running
## Using Visual Studio GUI
Run `explorer.exe .` and open `build/`. You should see a `.sln` extension, like `nova.sln`. Double click that. You can look more at the VS section to understand ho to actually use it. I only use VS to debug sometimes as I personally just use CLI + VSCode to develop at this point.

## run.ps1
`.ps1` files are PowerShell scripts. Open PowerShell in the terminal (type `powershell.exe` if it isn't the native Windows VSCode terminal).

Run `run.ps1`. You can look at it to see what it does.

# TODO
1. Maybe make a `vcpkg.json` manifest

# Making a dev branch
Don't work off main; make a branch `dev-<yourname>` with
```
git fetch origin
git checkout dev
git pull origin dev
```
Then use your name with `git checkout -b dev-<NAME>`. Then `git push -u origin dev-<NAME>` (the same branch you just made). You can view the branches with `git branch -v -a` - don't mess with the ones that have `remotes/` prepended to them.

You can base off of commits from whatever branch, but we should set up pull request to go in this order
```
main
    dev
        dev-andrew
        dev-gage
        ...
```