set mpich_path=C:\\Program Files (x86)\\MPICH2\\bin
set path=%mpich_path%;%path%

start "" "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\devenv.exe" build\x64\MPIProcessPool.sln

