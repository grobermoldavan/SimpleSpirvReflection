
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

void print_type_info(const SsrTypeInfo* type)
{
    switch (type->type)
    {
        case SSR_TYPE_BOOL:
        {
            printf("Bool");
        } break;
        case SSR_TYPE_SCALAR:
        {
            if (ssr_is_int(type))
                printf("Int");
            else if (ssr_is_uint(type))
                printf("Uint");
            else
                printf("Float");
        } break;
        case SSR_TYPE_VECTOR:
        {
            printf("%zd component vector", ssr_get_vector_components_num(type));
        } break;
        case SSR_TYPE_MATRIX:
        {
            printf("%zdx%zd matrix", ssr_get_matrix_columns_num(type), ssr_get_matrix_rows_num(type));
        } break;
        case SSR_TYPE_STRUCT:
        {
            printf("Struct %s with following members : ", ssr_get_struct_type_name(type));
            for (int it = 0; it < ssr_get_struct_members_num(type); it++)
            {
                printf("%s", ssr_get_struct_member_name(type, it));
                if (it != (ssr_get_struct_members_num(type) - 1)) printf(", ");
            }
        } break;
        case SSR_TYPE_SAMPLER:
        {
            printf("Sampler");
        } break;
        case SSR_TYPE_IMAGE:
        {
            const bool isDepth = ssr_get_image_depth_parameters(type) == SSR_IMAGE_DEPTH;
            const bool isSampled = ssr_get_image_sample_parameters(type) == SSR_IMAGE_USED_WITH_SAMPLER;
            printf("Image (%s, %s)", isDepth ? "depth" : "not depth", isSampled ? "sampled" : "not sampled");
        } break;
        case SSR_TYPE_ARRAY:
        {
            const bool isConstantSize = ssr_get_array_size_kind(type) == SSR_ARRAY_SIZE_CONSTANT;
            printf("Array of %s size", isConstantSize ? "constant" : "non-constant");
        } break;
    }
}

void print_reflection_info(SimpleSpirvReflection* reflection)
{
    printf("=================================================================\n");
    printf("Shader type is : %s\n", reflection->shaderType == SSR_SHADER_TYPE_VERTEX ? "vertex" : reflection->shaderType == SSR_SHADER_TYPE_FRAGMENT ? "fragment" : "compute");
    printf("Entry point name : %s\n", reflection->entryPointName);
    printf("Shader outputs :\n");
    for (int it = 0; it < reflection->numOutputs; it++)
    {
        SsrShaderIO* output = &reflection->outputs[it];
        if (output->isBuiltIn) continue;
        printf("    %d. Name : %s. Type : ", it, output->name);
        print_type_info(output->type);
        printf(".\n");
    }
    printf("Shader inputs :\n");
    for (int it = 0; it < reflection->numInputs; it++)
    {
        SsrShaderIO* input = &reflection->inputs[it];
        if (input->isBuiltIn) continue;
        printf("    %d. Name : %s. Type : ", it, input->name);
        print_type_info(input->type);
        printf(".\n");
    }
    printf("Shader uniforms :\n");
    for (int it = 0; it < reflection->numUniforms; it++)
    {
        SsrUniform* uniform = &reflection->uniforms[it];
        printf("    %d. Name : %s. Kind : %s. Set : %d. Binding : %d. Type : ", it, uniform->name, ssr_uniform_kind_to_str(uniform->kind), uniform->set, uniform->binding);
        print_type_info(uniform->type);
        printf(".\n");
    }
    printf("Shader specialization constants :\n");
    for (int it = 0; it < reflection->numSpecializationConstants; it++)
    {
        SsrSpecializationConstant* constant = &reflection->specializationConstants[it];
        printf("    %d. Name : %s. Constant id : %d. Type : ", it, constant->name, constant->constantId);
        print_type_info(constant->type);
        printf(". ");
        if (ssr_is_int(constant->type))         printf("Default value : %d\n", constant->asInt);
        else if (ssr_is_uint(constant->type))   printf("Default value : %d\n", constant->asUint);
        else if (ssr_is_float(constant->type))  printf("Default value : %f\n", constant->asFloat);
        else                                    printf("Default value : %s\n", constant->asBool ? "true" : "false");
    }
    printf("Shader push constant : %s\n", reflection->pushConstantType ? "present" : "non-present");
    if (reflection->pushConstantType)
    {
        printf("Shader push constant name : %s\n", reflection->pushConstantName);
        printf("Shader push constant type : ");
        print_type_info(reflection->pushConstantType);
        printf(".\n");
        printf("Shader push constant type size : %zd\n", ssr_get_type_size(reflection->pushConstantType));
    }
    if (reflection->shaderType == SSR_SHADER_TYPE_COMPUTE)
    {
        printf("Shader work group size : %d, %d, %d\n", reflection->computeWorkGroupSizeX, reflection->computeWorkGroupSizeY, reflection->computeWorkGroupSizeZ);
    }
}

int main(int argc, char* argv[])
{
    //
    // 1. Read the file
    //
    size_t vertexShaderSize = 0;
    size_t fragmentShaderSize = 0;
    size_t computeShaderSize = 0;
    void* vertexShader = read_spirv("test\\example_shaders\\example.vert.spv", &vertexShaderSize);
    void* fragmentShader = read_spirv("test\\example_shaders\\example.frag.spv", &fragmentShaderSize);
    void* computeShader = read_spirv("test\\example_shaders\\example.comp.spv", &computeShaderSize);
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
    SimpleSpirvReflection computeReflection = {0};
    SsrCreateInfo computeReflectionCreateInfo = (SsrCreateInfo)
    {
        .persistentAllocator = &persistentAllocator,
        .nonPersistentAllocator = &nonPersistentAllocator,
        .bytecode = computeShader,
        .bytecodeNumWords = computeShaderSize / 4,
    };
    ssr_construct(&computeReflection, &computeReflectionCreateInfo);
    //
    // 4. Do whatever you want with reflection data
    //
    print_reflection_info(&vertexReflection);
    print_reflection_info(&fragmentReflection);
    print_reflection_info(&computeReflection);
    //
    // 5. Don't forget to destroy reflection and free file memory
    //
    ssr_destroy(&computeReflection);
    ssr_destroy(&fragmentReflection);
    ssr_destroy(&vertexReflection);
    free(fragmentShader);
    free(vertexShader);

    return 0;
}