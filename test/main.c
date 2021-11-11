
#include <stdio.h>
#include <stdlib.h>

/*
    SSR_DEFAULT_MEMORY_BLOCK_SIZE can be used to set memory allocation chunk size.
    If SSR_DIRTY_ALLOCATOR is defined, all allocated memory will be memzeroed before use.
    SSR_IMPL must be defined in order to create implementation.
*/
#define SSR_DEFAULT_MEMORY_BLOCK_SIZE 256
#define SSR_DIRTY_ALLOCATOR
#define SSR_IMPL
#include "../simple_spirv_reflection.h"

void* read_spirv(const char* path, size_t* sizeBytes)
{
    FILE *f = fopen(path, "rb");
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* result = malloc(size + 1);
    fread(result, 1, size, f);
    fclose(f);
    result[size] = 0;
    
    *sizeBytes = size;
    return result;
}

void* allocate_func(void* userData, size_t size)
{
    printf("Allocating %zd bytes from %s allocator\n", size, (const char*)userData);
    return malloc(size);
}

void free_func(void* userData, void* ptr, size_t size)
{
    printf("Freeing %zd bytes from %s allocator\n", size, (const char*)userData);
    free(ptr);
}

void print_reflection_info(SimpleSpirvReflection* reflection)
{
    printf("=================================================================\n");
    printf("Shader type is : %s\n", reflection->type == SSR_SHADER_VERTEX ? "vertex" : "fragment");
    printf("Entry point name : %s\n", reflection->entryPointName);
    printf("Shader outputs :\n");
    for (int it = 0; it < reflection->numOutputs; it++)
    {
        SsrShaderIO* output = &reflection->outputs[it];
        if (output->isBuiltIn) continue;
        printf("    %d. Name : %s. Type : %s. Size in bytes : %zd\n", it, output->name, ssr_type_to_str(output->type->type), ssr_get_type_size(output->type));
    }
    printf("Shader inputs :\n");
    for (int it = 0; it < reflection->numInputs; it++)
    {
        SsrShaderIO* input = &reflection->inputs[it];
        if (input->isBuiltIn) continue;
        printf("    %d. Name : %s. Type : %s. Size in bytes : %zd\n", it, input->name, ssr_type_to_str(input->type->type), ssr_get_type_size(input->type));
    }
    printf("Shader uniforms :\n");
    for (int it = 0; it < reflection->numUniforms; it++)
    {
        SsrUniform* uniform = &reflection->uniforms[it];
        printf("    %d. Name : %s. Kind : %s. Type : %s. Set : %d. Binding : %d. Size in bytes : %zd\n", it, uniform->name, ssr_uniform_kind_to_str(uniform->kind), ssr_type_to_str(uniform->type->type), uniform->set, uniform->binding, ssr_get_type_size(uniform->type));
    }
    printf("Shader push constant : %s\n", reflection->pushConstantType ? "present" : "non-present");
    if (reflection->pushConstantType)
    {
        printf("Shader push constant name : %s\n", reflection->pushConstantName);
        printf("Shader push constant type : %s\n", ssr_type_to_str(reflection->pushConstantType->type));
        printf("Shader push constant type size : %zd\n", ssr_get_type_size(reflection->pushConstantType));
    }
}

int main(int argc, char* argv[])
{
    //
    // 1. Read the file
    //
    size_t vertexShaderSize = 0;
    size_t fragmentShaderSize = 0;
    void* vertexShader = read_spirv("test\\example_shaders\\example_vert.spv", &vertexShaderSize);
    void* fragmentShader = read_spirv("test\\example_shaders\\example_frag.spv", &fragmentShaderSize);
    //
    // 2. Make an allocator for reflection system
    //
    SsrAllocator persistentAllocator = (SsrAllocator)
    {
        .userData = "persistent",
        .alloc = allocate_func,
        .free = free_func,
    };
    SsrAllocator nonPersistentAllocator = (SsrAllocator)
    {
        .userData = "non persistent",
        .alloc = allocate_func,
        .free = free_func,
    };
    //
    // 3. Construct SsrCreateInfo structure and get reflection data through ssr_construct call
    //
    SimpleSpirvReflection vertexReflection = {0};
    SsrCreateInfo vertexReflectionCreateInfo = (SsrCreateInfo)
    {
        .persistentAllocator = &persistentAllocator,
        .nonPersistentAllocator = &nonPersistentAllocator,
        .bytecode = vertexShader,
        .bytecodeNumWords = vertexShaderSize / 4,
    };
    ssr_construct(&vertexReflection, &vertexReflectionCreateInfo);
    SimpleSpirvReflection fragmentReflection = {0};
    SsrCreateInfo fragmentReflectionCreateInfo = (SsrCreateInfo)
    {
        .persistentAllocator = &persistentAllocator,
        .nonPersistentAllocator = &nonPersistentAllocator,
        .bytecode = fragmentShader,
        .bytecodeNumWords = fragmentShaderSize / 4,
    };
    ssr_construct(&fragmentReflection, &fragmentReflectionCreateInfo);
    //
    // 4. Do whatever you want with reflection data
    //
    print_reflection_info(&vertexReflection);
    print_reflection_info(&fragmentReflection);
    //
    // 5. Don't forget to destroy reflection and free file memory
    //
    ssr_destroy(&fragmentReflection);
    ssr_destroy(&vertexReflection);
    free(fragmentShader);
    free(vertexShader);
    
    return 0;
}