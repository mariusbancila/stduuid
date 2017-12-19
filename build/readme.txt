Create a project from this folder by running the command:

Windows

   cmake -G "Visual Studio 15 2017" [arch] ..

where arch is an optional parameter and can be Win64 or ARM. Without one specified, x86 target
Mac

   cmake -G Xcode ..

If you don't have CMake installed you can get it from https://cmake.org/