@echo off

cd %cd%\test\example_shaders
for /r %%f in (\*.vert) do glslangValidator %%f -o %%f.spv -V -S vert
for /r %%f in (\*.frag) do glslangValidator %%f -o %%f.spv -V -S frag
for /r %%f in (\*.comp) do glslangValidator %%f -o %%f.spv -V -S comp
for /r %%f in (\*.spv) do spirv-dis %%f -o %%f.dis
