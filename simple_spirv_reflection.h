/* date = November 6th 2021 9:17 pm */

#ifndef SIMPLE_SPRIV_REFLECTION_H
#define SIMPLE_SPRIV_REFLECTION_H

#include <stdint.h>
#include <stdbool.h>

typedef uint32_t SsrSpirvWord;

typedef enum
{
    SSR_SHADER_VERTEX,
    SSR_SHADER_FRAGMENT,
} SsrShader;

typedef enum
{
    SSR_TYPE_SCALAR,
    SSR_TYPE_VECTOR,
    SSR_TYPE_MATRIX,
    SSR_TYPE_STRUCT,
    SSR_TYPE_SAMPLER,
    SSR_TYPE_IMAGE,
    SSR_TYPE_ARRAY,
} SsrType;

typedef enum
{
    SSR_IMAGE_1D,
    SSR_IMAGE_2D,
    SSR_IMAGE_3D,
    SSR_IMAGE_CUBE,
    SSR_IMAGE_RECT,
    SSR_IMAGE_BUFFER,
    SSR_IMAGE_SUBPASS_DATA,
} SsrImageDim;

#define SSR_IMAGE_PARAM_UNKNOWN (~((uint32_t)0))

typedef enum
{
    SSR_IMAGE_DEPTH,
    SSR_IMAGE_NOT_DEPTH,
} SsrImageDepthParameters;

typedef enum
{
    SSR_IMAGE_USED_WITH_SAMPLER,
    SSR_IMAGE_NOT_USED_WITH_SAMPLER,
} SsrImageSampleParameters;

typedef struct SsrTypeInfo
{
    SsrType type;
    struct SsrTypeInfo* next;
    union
    {
        struct
        {
            size_t bitWidth;
            bool isInteger;
            bool isSigned;
        } scalar;
        struct
        {
            struct SsrTypeInfo* componentType;
            size_t numComponents;
        } vector;
        struct
        {
            struct SsrTypeInfo* columnType;
            size_t numColumns;
        } matrix;
        struct
        {
            const char* typeName;
            struct SsrTypeInfo** members;
            const char** memberNames;
            size_t numMembers;
        } structure;
        struct
        {
            int dummy; // Nothing ??
        } sampler;
        struct
        {
            struct SsrTypeInfo* sampledType;
            SsrImageDim dim;
            SsrImageDepthParameters depthParameters;
            SsrImageSampleParameters sampleParameters;
            bool isArrayed;
            bool isSampled;
            bool isMultisampled;
        } image;
        struct
        {
            struct SsrTypeInfo* entryType;
            size_t size;
            bool isRuntimeArray;
        } array;
    } info;
} SsrTypeInfo;

typedef struct
{
    const char* name;
    SsrTypeInfo* type;
    uint32_t location;
    bool isBuiltIn;
} SsrShaderIO;

typedef enum
{
    SSR_UNIFORM_SAMPLER,
    SSR_UNIFORM_SAMPLED_IMAGE,
    SSR_UNIFORM_STORAGE_IMAGE,
    SSR_UNIFORM_COMBINED_IMAGE_SAMPLER,
    SSR_UNIFORM_UNIFORM_TEXEL_BUFFER,
    SSR_UNIFORM_STORAGE_TEXEL_BUFFER,
    SSR_UNIFORM_UNIFORM_BUFFER,
    SSR_UNIFORM_STORAGE_BUFFER,
    SSR_UNIFORM_INPUT_ATTACHMENT,
    SSR_UNIFORM_ACCELERATION_STRUCTURE,
} SsrUniformKind;

typedef struct
{
    const char* name;
    SsrTypeInfo* typeInfo;
    SsrUniformKind kind;
    uint32_t set;
    uint32_t binding;
    uint32_t inputAttachmentIndex;
} SsrUniform;

typedef struct
{
    void* userData;
    void* (*alloc)(void* userData, size_t size);
    void  (*free)(void* userData, void* ptr, size_t size);
} SsrAllocator;

typedef struct
{
    SsrAllocator allocator;
    
    SsrShader type;
    const char* entryPointName;
    
    SsrTypeInfo* typeInfos;
    
    SsrTypeInfo* pushConstantType;
    const char* pushConstantName;
    
    SsrShaderIO* inputs;
    size_t numInputs;
    
    SsrShaderIO* outputs;
    size_t numOutputs;
    
    SsrUniform* uniforms;
    size_t numUniforms;
    
} SimpleSpirvReflection;

typedef struct
{
    SsrAllocator* persistentAllocator;
    SsrAllocator* nonPersistentAllocator;
    SsrSpirvWord* bytecode;
    size_t bytecodeNumWords;
} SsrCreateInfo;

void ssr_construct(SimpleSpirvReflection* reflection, SsrCreateInfo* createInfo);
void ssr_destroy(SimpleSpirvReflection* reflection);

#endif //SIMPLE_SPRIV_REFLECTION_H

#ifndef SIMPLE_SPRIV_REFLECTION_IMPL
#define SIMPLE_SPRIV_REFLECTION_IMPL

#include <spirv-headers/spirv.h>

#ifndef ssr_printf
#   include <stdio.h>
#   define ssr_printf(fmt, ...) printf(fmt, __VA_ARGS__)
#endif

#ifndef ssr_assert
#   include <assert.h>
#   define ssr_assert(condition) assert(condition)
#endif

#ifndef ssr_memset
#   include <string.h>
#   define ssr_memset(ptr, val, size) memset(ptr, val, size)
#endif

#ifndef ssr_strlen
#   include <string.h>
#   define ssr_strlen(str) strlen(str)
#endif

#ifndef ssr_memcpy
#   include <string.h>
#   define ssr_memcpy(dst, src, size) memcpy(dst, src, size)
#endif

#ifndef ssr_strcmp
#   include <string.h>
#   define ssr_strcmp(first, second) (strcmp(first, second) == 0)
#endif

#ifndef ssr_array_size
#   define ssr_array_size(array) (sizeof(array) / sizeof(array[0]))
#endif

#ifndef ssr_indent
#   define ssr_indent "    "
#endif

#define ssr_opcode(word) ((uint16_t)word)

typedef enum
{
    SSR_DECORATION_BLOCK        = 0x00000001,
    SSR_DECORATION_BUFFER_BLOCK = 0x00000002,
    SSR_DECORATION_OFFSET       = 0x00000004,
    SSR_DECORATION_INPUT_ATTACHMENT_INDEX = 0x00000008,
    SSR_DECORATION_LOCATION     = 0x00000010,
    SSR_DECORATION_BINDING      = 0x00000020,
    SSR_DECORATION_SET          = 0x00000040,
    SSR_DECORATION_BUILT_IN     = 0x00000080,
} SsrDecoration;

typedef struct
{
    SsrSpirvWord* declarationLocation;
    const char* name;
    SsrSpirvWord storageClass;
    uint32_t decorations;
    uint32_t location;
    uint32_t binding;
    uint32_t set;
    uint32_t inputAttachmentIndex;
} SsrSpirvId;

typedef struct
{
    SsrSpirvId* id;
    const char* name;
} SsrSpirvStructMember;

typedef struct
{
    SsrSpirvId* id;
    SsrSpirvStructMember* members;
    size_t numMembers;
} SsrSpirvStruct;

char* __ssr_save_string(const char* str, SsrAllocator* allocator)
{
    size_t length = ssr_strlen(str);
    char* result = allocator->alloc(allocator->userData, length + 1);
    ssr_memcpy(result, str, length);
    result[length] = 0;
    return result;
}

SsrTypeInfo* __ssr_save_or_get_type_from_reflection(SsrSpirvStruct* structs, size_t numStructs, SsrSpirvId* ids, SsrSpirvId* id, SimpleSpirvReflection* reflection)
{
    SsrTypeInfo* matchedType = NULL;
    SsrTypeInfo resultType = {0};
    //
    // 1. Get type kind
    //
    uint16_t spvType = ssr_opcode(id->declarationLocation[0]);
    switch (spvType)
    {
        case SpvOpTypeFloat:        resultType.type = SSR_TYPE_SCALAR;
        case SpvOpTypeInt:          resultType.type = SSR_TYPE_SCALAR;
        case SpvOpTypeVector:       resultType.type = SSR_TYPE_VECTOR;
        case SpvOpTypeMatrix:       resultType.type = SSR_TYPE_MATRIX;
        case SpvOpTypeStruct:       resultType.type = SSR_TYPE_STRUCT;
        case SpvOpTypeSampler:      resultType.type = SSR_TYPE_SAMPLER;
        case SpvOpTypeSampledImage: resultType.type = SSR_TYPE_IMAGE;
        case SpvOpTypeImage:        resultType.type = SSR_TYPE_IMAGE;
        case SpvOpTypeRuntimeArray: resultType.type = SSR_TYPE_ARRAY;
        case SpvOpTypeArray:        resultType.type = SSR_TYPE_ARRAY;
        default:
        {
            ssr_assert(!"Unsupported OpType");
        } break;
    }
    //
    // 2. Process result type based on it's kind
    //
    switch(resultType.type)
    {
        case SSR_TYPE_SCALAR:
        {
            //
            // Scalar types can be either floats or doubles
            //
            switch(spvType)
            {
                case SpvOpTypeFloat:
                {
                    resultType.info.scalar.isInteger = false;
                    resultType.info.scalar.isSigned = true;
                    resultType.info.scalar.bitWidth = id->declarationLocation[2];
                } break;
                case SpvOpTypeInt:
                {
                    resultType.info.scalar.isInteger = true;
                    resultType.info.scalar.isSigned = id->declarationLocation[3] == 1;
                    resultType.info.scalar.bitWidth = id->declarationLocation[2];
                } break;
                default: ssr_assert(!"Unsupported scalar value type (bool?)");
            }
            //
            // Try to find scalar type with same properties in type list
            //
            SsrTypeInfo* alreadySavedValue = reflection->typeInfos;
            while (alreadySavedValue)
            {
                if (alreadySavedValue->type == SSR_TYPE_SCALAR &&
                    alreadySavedValue->info.scalar.isInteger == resultType.info.scalar.isInteger &&
                    alreadySavedValue->info.scalar.bitWidth == resultType.info.scalar.bitWidth &&
                    alreadySavedValue->info.scalar.isSigned == resultType.info.scalar.isSigned)
                {
                    matchedType = alreadySavedValue;
                    break;
                }
                alreadySavedValue = alreadySavedValue->next;
            }
        } break;
        case SSR_TYPE_VECTOR:
        {
            //
            // Save vector info
            //
            resultType.info.vector.numComponents = id->declarationLocation[3];
            resultType.info.vector.componentType = __ssr_save_or_get_type_from_reflection(structs, numStructs, ids, &ids[id->declarationLocation[2]], reflection);
            ssr_assert(resultType.info.vector.componentType->type == SSR_TYPE_SCALAR);
            //
            // Try to find vector type with same properties in type list
            //
            SsrTypeInfo* alreadySavedValue = reflection->typeInfos;
            while (alreadySavedValue)
            {
                if (alreadySavedValue->type == SSR_TYPE_VECTOR &&
                    alreadySavedValue->info.vector.numComponents == resultType.info.vector.numComponents &&
                    alreadySavedValue->info.vector.componentType == resultType.info.vector.componentType)
                {
                    matchedType = alreadySavedValue;
                    break;
                }
                alreadySavedValue = alreadySavedValue->next;
            }
        } break;
        case SSR_TYPE_MATRIX:
        {
            //
            // Save matrix info
            //
            resultType.info.matrix.numColumns = id->declarationLocation[3];
            resultType.info.matrix.columnType = __ssr_save_or_get_type_from_reflection(structs, numStructs, ids, &ids[id->declarationLocation[2]], reflection);
            ssr_assert(resultType.info.matrix.columnType->type == SSR_TYPE_VECTOR);
            //
            // Try to find matrix type with same properties in type list
            //
            SsrTypeInfo* alreadySavedValue = reflection->typeInfos;
            while (alreadySavedValue)
            {
                if (alreadySavedValue->type == SSR_TYPE_MATRIX &&
                    alreadySavedValue->info.matrix.numColumns == resultType.info.matrix.numColumns &&
                    alreadySavedValue->info.matrix.columnType == resultType.info.matrix.columnType)
                {
                    matchedType = alreadySavedValue;
                    break;
                }
                alreadySavedValue = alreadySavedValue->next;
            }
        } break;
        case SSR_TYPE_STRUCT:
        {
            //
            // Find corresponding SsrSpirvStruct
            //
            SsrSpirvStruct* spirvStruct = NULL;
            for (size_t structIt = 0; structIt < numStructs; structIt++)
            {
                if (structs[structIt].id == id)
                {
                    spirvStruct= &structs[structIt];
                    break;
                }
            }
            ssr_assert(spirvStruct);
            resultType.info.structure.typeName = spirvStruct->id->name;
            //
            // Try match struct
            //
            SsrTypeInfo* alreadySavedValue = reflection->typeInfos;
            while (alreadySavedValue)
            {
                if (alreadySavedValue->type == SSR_TYPE_STRUCT &&
                    ssr_strcmp(alreadySavedValue->info.structure.typeName, spirvStruct->id->name))
                {
                    matchedType = alreadySavedValue;
                    break;
                }
                alreadySavedValue = alreadySavedValue->next;
            }
            //
            // Save struct name and members if not matched
            //
            if (!matchedType)
            {
                resultType.info.structure.typeName = __ssr_save_string(spirvStruct->id->name, &reflection->allocator);
                resultType.info.structure.numMembers = spirvStruct->numMembers;
                resultType.info.structure.members = reflection->allocator.alloc(reflection->allocator.userData, sizeof(SsrTypeInfo*) * resultType.info.structure.numMembers);
                resultType.info.structure.memberNames = reflection->allocator.alloc(reflection->allocator.userData, sizeof(const char*) * resultType.info.structure.numMembers);
                for (size_t memberIt = 0; memberIt < resultType.info.structure.numMembers; memberIt++)
                {
                    resultType.info.structure.members[memberIt] = __ssr_save_or_get_type_from_reflection(structs, numStructs, ids, spirvStruct->members[memberIt].id, reflection);
                    resultType.info.structure.memberNames[memberIt] = __ssr_save_string(spirvStruct->members[memberIt].name, &reflection->allocator);
                }
            }
        } break;
        case SSR_TYPE_SAMPLER:
        {
            // Nothing (?)
        } break;
        case SSR_TYPE_IMAGE:
        {
            ssr_assert((spvType == SpvOpTypeSampledImage || spvType == SpvOpTypeImage) && "Unknown image type");
            //
            // Go to actual OpTypeImage and set isSampled param
            //
            SsrSpirvId* imageId = id;
            if (spvType == SpvOpTypeSampledImage)
            {
                resultType.info.image.isSampled = true;
                // OpTypeSampledImage just references OpTypeImage:
                // https://www.khronos.org/registry/SPIR-V/specs/unified1/SPIRV.html#OpTypeSampledImage
                imageId = &ids[id->declarationLocation[2]];
            }
            else if (spvType == SpvOpTypeImage)
            {
                resultType.info.image.isSampled = false;
            }
            else
            {
                ssr_assert(!"Unknown image type");
            }
            ssr_assert(ssr_opcode(imageId->declarationLocation[0]) == SpvOpTypeImage);
            //
            // Retrieve image params
            //
            const SpvDim imageDim = (SpvDim)imageId->declarationLocation[3];
            const SsrSpirvWord imageDepth = imageId->declarationLocation[4];
            const SsrSpirvWord isArrayed = imageId->declarationLocation[5];
            const SsrSpirvWord isMultisampled = imageId->declarationLocation[6];
            const SsrSpirvWord imageSampledInfo = imageId->declarationLocation[7];
            const SpvImageFormat imageFormat = (SpvImageFormat)imageId->declarationLocation[8];
            //
            // Get sampled type (can be void)
            //
            SsrSpirvId* sampledTypeId = &ids[imageId->declarationLocation[2]];
            if (sampledTypeId->declarationLocation[0] == SpvOpTypeVoid)
            {
                resultType.info.image.sampledType = NULL;
            }
            else
            {
                resultType.info.image.sampledType = __ssr_save_or_get_type_from_reflection(structs, numStructs, ids, sampledTypeId, reflection);
            }
            //
            // Convert image params to ssr's representation
            //
            switch (imageDim)
            {
                case SpvDim1D: resultType.info.image.dim = SSR_IMAGE_1D; break;
                case SpvDim2D: resultType.info.image.dim = SSR_IMAGE_2D; break;
                case SpvDim3D: resultType.info.image.dim = SSR_IMAGE_3D; break;
                case SpvDimCube: resultType.info.image.dim = SSR_IMAGE_CUBE; break;
                case SpvDimRect: resultType.info.image.dim = SSR_IMAGE_RECT; break;
                case SpvDimBuffer: resultType.info.image.dim = SSR_IMAGE_BUFFER; break;
                case SpvDimSubpassData: resultType.info.image.dim = SSR_IMAGE_SUBPASS_DATA; break;
                default: ssr_assert(!"Unknown SpvDim value");
            }
            switch (imageDepth)
            {
                case 0: resultType.info.image.depthParameters = SSR_IMAGE_NOT_DEPTH; break;
                case 1: resultType.info.image.depthParameters = SSR_IMAGE_DEPTH; break;
                case 2: resultType.info.image.depthParameters = SSR_IMAGE_PARAM_UNKNOWN; break;
                default: ssr_assert(!"Unknown OpTypeImage depth value");
            }
            switch (imageSampledInfo)
            {
                case 0: resultType.info.image.sampleParameters = SSR_IMAGE_PARAM_UNKNOWN; break;
                case 1: resultType.info.image.sampleParameters = SSR_IMAGE_USED_WITH_SAMPLER; break;
                case 2: resultType.info.image.sampleParameters = SSR_IMAGE_NOT_USED_WITH_SAMPLER; break;
                default: ssr_assert(!"Unknown OpTypeImage sample value");
            }
            resultType.info.image.isArrayed = isArrayed != 0;
            resultType.info.image.isMultisampled = isMultisampled != 0;
            //
            // Try match image
            //
            SsrTypeInfo* alreadySavedValue = reflection->typeInfos;
            while (alreadySavedValue)
            {
                if (alreadySavedValue->type == SSR_TYPE_IMAGE &&
                    alreadySavedValue->info.image.sampledType == resultType.info.image.sampledType &&
                    alreadySavedValue->info.image.isSampled == resultType.info.image.isSampled &&
                    alreadySavedValue->info.image.dim == resultType.info.image.dim &&
                    alreadySavedValue->info.image.depthParameters == resultType.info.image.depthParameters &&
                    alreadySavedValue->info.image.sampleParameters == resultType.info.image.sampleParameters &&
                    alreadySavedValue->info.image.isArrayed == resultType.info.image.isArrayed &&
                    alreadySavedValue->info.image.isMultisampled == resultType.info.image.isMultisampled)
                {
                    matchedType = alreadySavedValue;
                    break;
                }
                alreadySavedValue = alreadySavedValue->next;
            }
        } break;
        case SSR_TYPE_ARRAY:
        {
            //
            // Retrieve array data
            //
            switch (spvType)
            {
                case SpvOpTypeRuntimeArray:
                {
                    resultType.info.array.isRuntimeArray = true;
                    resultType.info.array.size = 0;
                } break;
                case SpvOpTypeArray:
                {
                    resultType.info.array.isRuntimeArray = false;
                    // Go to OpConstant describing array length
                    SsrSpirvId* arrayLength = &ids[id->declarationLocation[3]];
                    // Check that constant type is integer
                    SsrSpirvId* arrayLengthType = &ids[arrayLength->declarationLocation[1]];
                    ssr_assert(ssr_opcode(arrayLengthType->declarationLocation[0]) == SpvOpTypeInt);
                    // Check constant bit width and save array size
                    SsrSpirvWord arrayLengthTypeBitWidth = arrayLengthType->declarationLocation[2];
                    switch (arrayLengthTypeBitWidth)
                    {
                        case 32: resultType.info.array.size = *((uint64_t*)&arrayLength->declarationLocation[3]); break;
                        default: ssr_assert(!"Unsupported array size constat bit width");
                    };
                } break;
                default: ssr_assert(!"Unknown array type");
            }
            resultType.info.array.entryType = __ssr_save_or_get_type_from_reflection(structs, numStructs, ids, &ids[id->declarationLocation[2]], reflection);
            //
            // Try match array
            //
            SsrTypeInfo* alreadySavedValue = reflection->typeInfos;
            while (alreadySavedValue)
            {
                if (alreadySavedValue->type == SSR_TYPE_ARRAY &&
                    alreadySavedValue->info.array.isRuntimeArray == resultType.info.array.isRuntimeArray &&
                    alreadySavedValue->info.array.size == resultType.info.array.size &&
                    alreadySavedValue->info.array.entryType == resultType.info.array.entryType)
                {
                    matchedType = alreadySavedValue;
                    break;
                }
                alreadySavedValue = alreadySavedValue->next;
            }
        } break;
        default: ssr_assert(!"Unsupported SSR_TYPE_* value");
    }
    //
    // 3. If type is not yet saved in reflection, save it
    //
    if (!matchedType)
    {
        matchedType = reflection->allocator.alloc(reflection->allocator.userData, sizeof(SsrTypeInfo));
        ssr_memcpy(matchedType, &resultType, sizeof(SsrTypeInfo));
        if (reflection->typeInfos)
        {
            SsrTypeInfo* lastTypeInfo = reflection->typeInfos;
            while (lastTypeInfo->next) lastTypeInfo = lastTypeInfo->next;
            lastTypeInfo->next = matchedType;
        }
        else
        {
            reflection->typeInfos = matchedType;
        }
    }
    return matchedType;
}

void __ssr_process_shader_io_variable(SsrSpirvStruct* structs, size_t numStructs, SsrSpirvId* ids, SsrSpirvId* ioDeclarationId, SimpleSpirvReflection* reflection, SsrShaderIO* io)
{
    io->name = __ssr_save_string(ioDeclarationId->name, &reflection->allocator);
    io->location = ioDeclarationId->location;
    ssr_assert(ssr_opcode(ioDeclarationId->declarationLocation[0]) == SpvOpVariable);
    SsrSpirvId* id = &ids[ioDeclarationId->declarationLocation[1]];
    ssr_assert(ssr_opcode(id->declarationLocation[0]) == SpvOpTypePointer);
    id = &ids[id->declarationLocation[3]];
    io->type = __ssr_save_or_get_type_from_reflection(structs, numStructs, ids, id, reflection);
    if (ioDeclarationId->decorations & SSR_DECORATION_BUILT_IN || id->decorations & SSR_DECORATION_BUILT_IN)
    {
        io->isBuiltIn = true;
    }
}

void __ssr_process_shader_input_variable(SsrSpirvStruct* structs, size_t numStructs, SsrSpirvId* ids, SsrSpirvId* inputVariable, SimpleSpirvReflection* reflection)
{
    SsrShaderIO* input = &reflection->inputs[reflection->numInputs++];
    __ssr_process_shader_io_variable(structs, numStructs, ids, inputVariable, reflection, input);
}

void ssr_construct(SimpleSpirvReflection* reflection, SsrCreateInfo* createInfo)
{
#define ssr_save_decl_location(index) { SsrSpirvWord id = instruction[index]; ssr_assert(id < bound); ids[id].declarationLocation = instruction; }
    
    reflection->allocator = *createInfo->persistentAllocator;
    SsrAllocator* persistenAllocator = createInfo->persistentAllocator;
    SsrAllocator* nonPersistentAllocator = createInfo->nonPersistentAllocator;
    SsrSpirvWord* bytecode = createInfo->bytecode;
    size_t wordCount = createInfo->bytecodeNumWords;
    //
    // Basic header validation + retrieving number of unique ids (bound) used in bytecode
    //
    ssr_assert(bytecode[0] == SpvMagicNumber);
    size_t bound = bytecode[3];
    SsrSpirvWord* instruction = NULL;
    //
    // Allocate SsrSpirvId array which will hold all neccessary information about shader identifiers
    //
    SsrSpirvId* ids = nonPersistentAllocator->alloc(nonPersistentAllocator->userData, sizeof(SsrSpirvId) * bound);
    ssr_memset(ids, 0, sizeof(SsrSpirvId) * bound);
    //
    // Step 1. Filling ids array + some general shader info
    //
    size_t inputsCount = 0;
    size_t outputsCount = 0;
    size_t uniformsCount = 0;
    for (instruction = bytecode + 5; instruction < (bytecode + wordCount);)
    {
        uint16_t instructionOpCode = ssr_opcode(instruction[0]);
        uint16_t instructionWordCount = (uint16_t)(instruction[0] >> 16);
        switch (instructionOpCode)
        {
            case SpvOpEntryPoint:
            {
                ssr_assert(instructionWordCount >= 4);
                if (instruction[1] == SpvExecutionModelVertex)
                    reflection->type = SSR_SHADER_VERTEX;
                else if (instruction[1] == SpvExecutionModelFragment)
                    reflection->type = SSR_SHADER_FRAGMENT;
                else
                    ssr_assert(!"Only vertex and fragment shaders are supported");
                reflection->entryPointName = __ssr_save_string((char*)&instruction[3], &reflection->allocator);
            } break;
            case SpvOpName:
            {
                ssr_assert(instructionWordCount >= 3);
                ids[instruction[1]].name = (char*)&instruction[2];
            } break;
            case SpvOpConstant:
            {
                ssr_assert(instructionWordCount >= 3);
                ssr_save_decl_location(2);
            } break;
            case SpvOpTypeVoid:
            {
                ssr_assert(instructionWordCount == 2);
                ssr_save_decl_location(1);
            } break;
            case SpvOpTypeFloat:
            {
                ssr_assert(instructionWordCount == 3);
                ssr_save_decl_location(1);
            } break;
            case SpvOpTypeInt:
            {
                ssr_assert(instructionWordCount == 4);
                ssr_save_decl_location(1);
            } break;
            case SpvOpTypeVector:
            {
                ssr_assert(instructionWordCount == 4);
                ssr_save_decl_location(1);
            } break;
            case SpvOpTypeMatrix:
            {
                ssr_assert(instructionWordCount == 4);
                ssr_save_decl_location(1);
            } break;
            case SpvOpTypeStruct:
            {
                ssr_assert(instructionWordCount >= 2);
                ssr_save_decl_location(1);
            } break;
            case SpvOpTypeSampler:
            {
                ssr_assert(instructionWordCount == 2);
                ssr_save_decl_location(1);
            } break;
            case SpvOpTypeSampledImage:
            {
                ssr_assert(instructionWordCount == 3);
                ssr_save_decl_location(1);
            } break;
            case SpvOpTypeImage:
            {
                ssr_assert(instructionWordCount >= 9);
                ssr_save_decl_location(1);
            } break;
            case SpvOpTypeRuntimeArray:
            {
                ssr_assert(instructionWordCount == 3);
                ssr_save_decl_location(1);
            } break;
            case SpvOpTypeArray:
            {
                ssr_assert(instructionWordCount == 4);
                ssr_save_decl_location(1);
            } break;
            case SpvOpTypePointer:
            {
                ssr_assert(instructionWordCount == 4);
                ssr_save_decl_location(1);
                ids[instruction[1]].storageClass = instruction[2];
            } break;
            case SpvOpVariable:
            {
                ssr_assert(instructionWordCount >= 4);
                ssr_save_decl_location(2);
                SsrSpirvWord storageClass = instruction[3];
                SsrSpirvWord resultId = instruction[2];
                ids[resultId].storageClass = storageClass;
                if      (storageClass == SpvStorageClassInput) inputsCount += 1;
                else if (storageClass == SpvStorageClassOutput) outputsCount += 1;
                else if (storageClass == SpvStorageClassUniform) uniformsCount += 1;
                else if (storageClass == SpvStorageClassUniformConstant) uniformsCount += 1;
                else if (storageClass == SpvStorageClassStorageBuffer) uniformsCount += 1;
            } break;
            case SpvOpMemberDecorate:
            {
                ssr_assert(instructionWordCount >= 4);
                SpvDecoration decoration = (SpvDecoration)instruction[3];
                switch (decoration)
                {
                    case SpvDecorationBuiltIn:
                    {
                        // Here we basically saying that if structure member has "BuiltIn" decoration,
                        // then the whole structure is considered "BuiltIn" and will be ignored in reflection
                        ssr_assert(instructionWordCount == 5);
                        SsrSpirvWord id = instruction[1];
                        ssr_assert(id < bound);
                        ids[id].decorations |= SSR_DECORATION_BUILT_IN;
                    } break;
                }
            } break;
            case SpvOpDecorate:
            {
                SpvDecoration decoration = (SpvDecoration)instruction[2];
                switch (decoration)
                {
                    case SpvDecorationBlock:
                    {
                        ssr_assert(instructionWordCount == 3);
                        SsrSpirvWord id = instruction[1];
                        ssr_assert(id < bound);
                        ids[id].decorations |= SSR_DECORATION_BLOCK;
                    } break;
                    case SpvDecorationBufferBlock:
                    {
                        ssr_assert(instructionWordCount == 3);
                        SsrSpirvWord id = instruction[1];
                        ssr_assert(id < bound);
                        ids[id].decorations |= SSR_DECORATION_BUFFER_BLOCK;
                    } break;
                    case SpvDecorationBuiltIn:
                    {
                        ssr_assert(instructionWordCount == 4);
                        SsrSpirvWord id = instruction[1];
                        ssr_assert(id < bound);
                        ids[id].decorations |= SSR_DECORATION_BUILT_IN;
                    } break;
                    case SpvDecorationLocation:
                    {
                        ssr_assert(instructionWordCount == 4);
                        SsrSpirvWord id = instruction[1];
                        ssr_assert(id < bound);
                        ids[id].location = instruction[3];
                        ids[id].decorations |= SSR_DECORATION_LOCATION;
                    } break;
                    case SpvDecorationBinding:
                    {
                        ssr_assert(instructionWordCount == 4);
                        SsrSpirvWord id = instruction[1];
                        ssr_assert(id < bound);
                        ids[id].binding = instruction[3];
                        ids[id].decorations |= SSR_DECORATION_BINDING;
                    } break;
                    case SpvDecorationDescriptorSet:
                    {
                        ssr_assert(instructionWordCount == 4);
                        SsrSpirvWord id = instruction[1];
                        ssr_assert(id < bound);
                        ids[id].set = instruction[3];
                        ids[id].decorations |= SSR_DECORATION_SET;
                    } break;
                    case SpvDecorationInputAttachmentIndex:
                    {
                        ssr_assert(instructionWordCount == 4);
                        SsrSpirvWord id = instruction[1];
                        ssr_assert(id < bound);
                        ids[id].inputAttachmentIndex = instruction[3];
                        ids[id].decorations |= SSR_DECORATION_INPUT_ATTACHMENT_INDEX;
                    } break;
                }
            }
        }
    }
    //
    // Step 2. Retrieving information about shader structs
    //
    //
    // Step 2.1. Count number of structs and struct members
    //
    size_t numStructs = 0;
    size_t numStructMembers = 0;
    for (instruction = bytecode + 5; instruction < (bytecode + wordCount);)
    {
        uint16_t instructionOpCode = ssr_opcode(instruction[0]);
        uint16_t instructionWordCount = (uint16_t)(instruction[0] >> 16);
        switch (instructionOpCode)
        {
            case SpvOpTypeStruct:
            {
                numStructs += 1;
                numStructMembers += instructionWordCount - 2;
            } break;
        }
        instruction += instructionWordCount;
    }
    //
    // Step 2.2. Allocate structs and struct members arrays
    //
    SsrSpirvStruct* structs = nonPersistentAllocator->alloc(nonPersistentAllocator->userData, sizeof(SsrSpirvStruct) * numStructs);
    SsrSpirvStructMember* structMembers = nonPersistentAllocator->alloc(nonPersistentAllocator->userData, sizeof(SsrSpirvStructMember) * numStructMembers);
    //
    // Step 2.3. Save struct and struct members ids
    //
    size_t structCounter = 0;
    size_t memberCounter = 0;
    for (instruction = bytecode + 5; instruction < (bytecode + wordCount);)
    {
        uint16_t instructionOpCode = ssr_opcode(instruction[0]);
        uint16_t instructionWordCount = (uint16_t)(instruction[0] >> 16);
        switch (instructionOpCode)
        {
            case SpvOpTypeStruct:
            {
                SsrSpirvStruct* shaderStruct = &structs[structCounter++];
                shaderStruct->id = &ids[instruction[1]];
                shaderStruct->numMembers = instructionWordCount - 2;
                shaderStruct->members = &structMembers[memberCounter];
                memberCounter += shaderStruct->numMembers;
                for (size_t it = 0; it < shaderStruct->numMembers; it++)
                {
                    SsrSpirvStructMember* member = &shaderStruct->members[it];
                    member->id = &ids[instruction[2 + it]];
                }
            } break;
        }
        instruction += instructionWordCount;
    }
    //
    // Step 3. Process all input, ouptut variables, push constants and buffers
    //
    for (size_t it = 0; it < bound; it++)
    {
        SsrSpirvId* id = &ids[it];
        if (id->declarationLocation && ssr_opcode(id->declarationLocation[0]) == SpvOpVariable)
        {
            SpvStorageClass storageClass = (SpvStorageClass)ids[id->declarationLocation[2]].storageClass;
            if (storageClass == SpvStorageClassInput)
            {
                __ssr_process_shader_input_variable(structs, numStructs, ids, id, reflection);
            }
            else if (storageClass == SpvStorageClassOutput)
            {
                //ssr_process_shader_output_variable(structs, numStructs, ids, id, reflection);
            }
            else if (storageClass == SpvStorageClassPushConstant)
            {
                //ssr_process_shader_push_constant(structs, numStructs, ids, id, reflection);
            }
            else if (storageClass == SpvStorageClassUniform)
            {
                //ssr_process_shader_uniform(structs, numStructs, ids, id, reflection);
            }
            else if (storageClass == SpvStorageClassUniformConstant)
            {
                //ssr_process_shader_uniform(structs, numStructs, ids, id, reflection);
            }
            else if (storageClass == SpvStorageClassStorageBuffer)
            {
                //ssr_process_shader_uniform(structs, numStructs, ids, id, reflection);
            }
        }
    }
    //
    // Cleanup
    //
    nonPersistentAllocator->free(nonPersistentAllocator->userData, structs, sizeof(SsrSpirvStruct) * numStructs);
    nonPersistentAllocator->free(nonPersistentAllocator->userData, structMembers, sizeof(SsrSpirvStructMember) * numStructMembers);
    nonPersistentAllocator->free(nonPersistentAllocator->userData, ids, sizeof(SsrSpirvId) * bound);
#undef ssr_save_decl_location
}

void ssr_destroy(SimpleSpirvReflection* reflection)
{
}

#undef ssr_opcode

#endif //SIMPLE_SPRIV_REFLECTION_IMPL
