# SimpleSpirvReflection

Simple, single-header, stb-style SPIR-V reflection library.

Just include simple_spirv_reflection.h in your project as many times as you want.
Define **SSR_IMPL** before you include this file in *one* C or C++ file to create the implementation.

Library can be configured using #defines :
 - if **SSR_DIRTY_ALLOCATOR** is defined library will make sure to zero all allocated memory
 - **SSR_DEFAULT_MEMORY_BLOCK_SIZE** can be used to alter the size of allocated memory blocks (default size is 4KB)
