# NOVA
Neuromorphic Optics Visualization Application

# Setup
## Dependencies
### VCPKG
You don't have to use `C:\Dev\`, that's just what I use. You can make your own.
```
git clone https://github.com/microsoft/vcpkg.git C:\Dev\vcpkg
cd C:\Dev\vcpkg
git checkout -b fmt-10.2.1 b8ec6abf5d
.\bootstrap-vcpkg.bat
```

You should make an environment variable `VCPKG_ROOT` so you can access it with `$env:VCPKG_ROOT` anywhere. Then add it `C:\Dev\vcpkg` to your PATH, so you can run `vcpkg` anywhere. For example, I have a key value pair of `VCPKG_ROOT` `C:\Dev\vcpkg`.

`fmt` is problematic. If you `cd $env:VCPKG_ROOT` and `git log --oneline --grep="\[fmt\]"` you will see we need to step back to the proper vcpkg hash, to enforce lower usage. Otherwise, we get a conflict when using `dv-processing` which erroneously states any version of fmt `>=8.1.1` works. Hence the checkout to an older version of `vcpkg` that supports it.

### VS 2017/2022
For MSVC, download [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/).

You need at the bare minimum a working version of MSVC, so select

- Desktop development with C++
- For safety get MSVC 142 as well

### Dependencies
The `--triplet=x64-windows` will install these libraries to be used in your system. If you want them local to the project (not recommended) you should remove that. 

1. Run `vcpkg install --triplet=x64-windows dv-processing glew glfw3 glm eigen3` in the project root (this can take up to an hour, run this and come back in a while).
2. [cmake](https://cmake.org/download/)

### Initialize Submodules
1. Run `git submodule

### Building
`cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DCMAKE_BUILD_TYPE=Release` builds in release mode, which doesn't come with debugging symbols.

# Running
## Using Visual Studio GUI
Run `explorer.exe .` and open `build/`. You should see a `.sln` extension, like `nova.sln`. Double click that. You can look more at the VS section to understand ho to actually use it. I only use VS to debug sometimes as I personally just use CLI + VSCode to develop at this point.

## run.ps1
`.ps1` files are PowerShell scripts. Open PowerShell in the terminal (type `powershell.exe` if it isn't the native Windows VSCode terminal).

Run `run.ps1`. You can look at it to see what it does.

# TODO
1. Should make a `vcpkg.json` manifest
2. Can add GLM / GLEW / ... to that `vcpkg.json` (maybe)
3. Test on other devices

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