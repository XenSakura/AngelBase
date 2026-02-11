module;

#include <slang/slang.h>
#include <slang/slang-com-helper.h>
#include <slang/slang-com-ptr.h>
#include <simdjson/ondemand.h>
#include <vulkan/vulkan.hpp>


export module ShaderManager;

import std;
import JsonParser;
namespace Rendering
{

    export struct ShaderReflection
    {
        struct UniformBuffer
        {
            std::string name;
            uint32_t binding;
            uint32_t set;
            size_t size;
            std::vector<std::pair<std::string, size_t>> members; // member name, offset
        };
    
        struct Texture
        {
            std::string name;
            uint32_t binding;
            uint32_t set;
            bool isSampler;
        };
    
        struct PushConstant
        {
            std::string name;
            size_t size;
            size_t offset;
        };
    
        struct VertexInput
        {
            std::string name;
            uint32_t location;
            std::string type; // "float3", "float2", etc.
        };
    
        std::vector<UniformBuffer> uniformBuffers;
        std::vector<Texture> textures;
        std::vector<PushConstant> pushConstants;
        std::vector<VertexInput> vertexInputs;
        std::string raw_json;
        
    };

    export struct CompiledShader
    {
        
        enum ShaderError
        {
            eSuccess,
            eFailedToOpen, //bad path
            eFailedToLoad, // failed to compile
            //eFailedToFindEntry, // wrong entry point
            //eFailedToComposeShader,// system error, log
            eFailedToLinkShader, //system error, log
            eFailedToGetSpirv //system error, log
        };
        CompiledShader() = default;
        CompiledShader(ShaderError e)
            :error(e)
        {}
        ShaderError error;
        std::string name;
        std::string source;
        ISlangBlob* spirv_code;
        ShaderReflection reflection_data;
        
        size_t source_hash;
        std::filesystem::file_time_type last_edited;
    };
    /**
     * TODO: Deprecate this class and use a dedicated pipeline manager. for now this will handle File I/O and compiling shaders
     */
    export class ShaderManager
    {
    public:
        ShaderManager()
        {
            slang::createGlobalSession(global_session.writeRef());
            CompiledShaderCode.reserve(128);
        }
        
        void initialize(const std::string& compiler_profile = "spirv_1_5", const slang::CompilerOptionEntry& entry = {})
        {
            slang::SessionDesc session_desc = {};
            
            slang::TargetDesc target_desc = {};
            target_desc.format = SLANG_SPIRV;
            target_desc.profile = global_session->findProfile(compiler_profile.c_str());

            session_desc.targets = &target_desc;
            session_desc.targetCount = static_cast<SlangInt>(1);
            session_desc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;

            std::array<slang::CompilerOptionEntry, 2> options = 
            {{
                slang::CompilerOptionEntry{
                    slang::CompilerOptionName::EmitSpirvDirectly,
                    slang::CompilerOptionValue{slang::CompilerOptionValueKind::Int, 1}
                }
            }};
            session_desc.compilerOptionEntries = options.data();
            session_desc.compilerOptionEntryCount = static_cast<uint32_t>(options.size());

            Slang::ComPtr<slang::ISession> session = nullptr;
            global_session->createSession(session_desc, session.writeRef());

            slang_session = session;

            if (!slang_session.get())
            {
                throw std::runtime_error("Failed to create slang session");
            }
        }

        std::optional<std::string> readShaderSource(const std::string& path)
        {
            std::ifstream file(path, std::ios::in | std::ios::binary);
            if (!file.is_open())
            {
                // Could log error here: "Failed to open shader file: " + path
                assert(false && std::string("Failed to open shader file: " + path).c_str());
                return std::nullopt;
            }
    
            // Read entire file into string
            std::string source;
            file.seekg(0, std::ios::end);
            source.resize(file.tellg());
            file.seekg(0, std::ios::beg);
            file.read(&source[0], source.size());
            file.close();
    
            return source;
        }

        CompiledShader compileShader(const std::string& name, const std::string& directory)
        {
            //check to ensure it's a proper slang file and get the raw filename to save metadata
            std::string raw_filename = stripSlangFileExtension(name);
            
            Slang::ComPtr<slang::IModule> slang_module;
            
            std::string updated_path = directory + "/" +  name;
            
            //1. Try to get shader source from path
            //TODO: Replace this with I/O handler
            auto source = readShaderSource(updated_path);
            if (!source)
            {
                return CompiledShader(CompiledShader::eFailedToOpen);
            }

            //2. Load shader module for compiling
            Slang::ComPtr<slang::IBlob> diagnostics_blob;
            {
                slang_module = slang_session->loadModuleFromSourceString(
                    raw_filename.c_str(),
                    updated_path.c_str(),
                    // there is an option without src string but I think we should use this for now
                    // this allows us to change this out for our own I/O system who can get the relevant data without blocking.
                    //TODO: make a loading system that will handle I/O without blocking
                    source->c_str(),
                    diagnostics_blob.writeRef());
                diagnoseIfNeeded(diagnostics_blob);
                
                if (slang_module.get() == nullptr)
                {
                    return CompiledShader(CompiledShader::eFailedToLoad);
                }
            }
            

            //3. Get Target Code-- does all the steps for us
            Slang::ComPtr<ISlangBlob> spirv;
            slang_module->getTargetCode(0, spirv.writeRef(), diagnostics_blob.writeRef());
            diagnoseIfNeeded(diagnostics_blob);
            if (spirv.get() == nullptr)
            {
                return CompiledShader(CompiledShader::eFailedToGetSpirv);
            }

            //Link Shader specifically for reflection
            Slang::ComPtr<slang::IComponentType> linked_program;
            slang_module.get()->link(linked_program.writeRef(), diagnostics_blob.writeRef());
            if (linked_program.get() == nullptr)
            {
                return CompiledShader(CompiledShader::eFailedToLinkShader);
            }
            ShaderReflection shader_reflection = getReflectionData(slang_module);
            writeJsonFile(raw_filename, directory + "/cache", shader_reflection.raw_json);

            
            //TODO:perhaps a simpler way to do this with GetTargetCode
            
            // Slang::ComPtr<slang::IEntryPoint> entry_point;
            // //3. get entry point 
            // {
            //     slang_module->findEntryPointByName(entry_name.c_str(), entry_point.writeRef());
            //     if (entry_point.get() == nullptr)
            //     {
            //         return CompiledShader(CompiledShader::eFailedToFindEntry);
            //     }
            // }
            //
            // std::array<slang::IComponentType*, 2> component_types = { slang_module, entry_point };
            // Slang::ComPtr<slang::IComponentType> composed_program;
            // //4. Compose shader 
            // {
            //     slang_session->createCompositeComponentType(
            //         component_types.data(),
            //         component_types.size(),
            //         composed_program.writeRef(),
            //         diagnostics_blob.writeRef());
            //     diagnoseIfNeeded(diagnostics_blob);
            // }
            //
            // if (composed_program.get() == nullptr)
            // {
            //     return CompiledShader(CompiledShader::eFailedToLoad);
            // }
            //
            // // sub step, handle reflection
            // ShaderReflection shader_reflection = getReflectionData(composed_program.get());
            // WriteJsonFile(name, directory + "/cache", shader_reflection.raw_json);
            //
            // //5. Link shader
            // Slang::ComPtr<slang::IComponentType> linked_program;
            // composed_program->link(linked_program.writeRef(), diagnostics_blob.writeRef());
            //
            // if (linked_program.get() == nullptr)
            // {
            //     return CompiledShader(CompiledShader::eFailedToLinkShader);
            // }
            //
            // //7. Get the Spir-V code
            // Slang::ComPtr<slang::IBlob> spir_v_code;
            // linked_program->getEntryPointCode(0, 0, spir_v_code.writeRef(), diagnostics_blob.writeRef());
            // diagnoseIfNeeded(diagnostics_blob);
            //
            // if (spir_v_code.get() == nullptr)
            // {
            //     return CompiledShader(CompiledShader::eFailedToGetSpirv);
            // }
            //
            CompiledShader result = {};
            result.error = CompiledShader::eSuccess;
            result.name = name;
            result.source = source.value();
            
            result.spirv_code = spirv;
            saveSpirvToFile(spirv, directory + "/cache",raw_filename);
            
            std::cout << source.value() << std::endl;

            CompiledShaderCode.emplace(result.name, std::move(result));
            
            return CompiledShaderCode[name];
        }

        static std::string stripSlangFileExtension(const std::string& name)
        {
            std::string raw_name;
            if (name.size() > 6 && name.substr(name.size() - 6) == ".slang")
            {
                raw_name = name.substr(0, name.size() - 6);
            }
            else
            {
                assert(false && "This file is not a shader file of the extension type .slang!");
            }

            return raw_name;
        }

        //I'm not certain how useful this is-- we can always just cache the pipelines and descriptors
        // temp pasting of raw json
        bool writeJsonFile(const std::string& raw_filename, const std::string& path, const std::string& raw)
        {
            std::filesystem::path p (path);
            if (!std::filesystem::exists(p))
            {
                if (!std::filesystem::create_directories(p))
                {
                    return false;
                }
            }
            std::filesystem::path full_path = p / (raw_filename + ".json");

            std::ofstream file(full_path, std::ios::binary);
            if (!file.is_open())
            {
                return false;
            }

            file.write(raw.data(), raw.size());
    
            file.close();
            return true;
        }

        bool saveSpirvToFile(ISlangBlob* spir_v_code,
                     const std::string& output_dir,
                     const std::string& raw_filename)
        {
            
            namespace fs = std::filesystem;
    
            if (!spir_v_code) {
                return false;
            }
    
            // Create directory if it doesn't exist
            fs::path dir_path(output_dir);
            if (!fs::exists(dir_path)) {
                if (!fs::create_directories(dir_path)) {
                    return false;
                }
            }
            
            // Construct full file path
            fs::path file_path = dir_path / (raw_filename + ".spv");
    
            // Open file in binary mode
            std::ofstream file(file_path, std::ios::binary);
            if (!file.is_open()) {
                return false;
            }
    
            // Write SPIR-V data directly from blob
            file.write(static_cast<const char*>(spir_v_code->getBufferPointer()),
                       spir_v_code->getBufferSize());
    
            file.close();
            return true;
        }

        CompiledShader recompileShader(const std::string& name, const std::string& path)
        {
            CompiledShaderCode.erase(name);
            return compileShader(name, path);
        }
    
        void clearCache()
        {
            CompiledShaderCode.clear();
        }

        static void diagnoseIfNeeded(slang::IBlob* diagnosticsBlob)
        {
            if (diagnosticsBlob != nullptr)
            {
                std::cerr << static_cast<const char*>(diagnosticsBlob->getBufferPointer()) << std::endl;
            }
        }

        /**
         * ALERT: Call vk::destroyVulkanShaderModule as soon as you're done building the pipeline!
         * @param device 
         * @param name 
         * @return 
         */
        vk::ShaderModule createVulkanShaderModule(const vk::Device& device, const std::string& name)
        {
            auto it = CompiledShaderCode.find(name);
            if (it == CompiledShaderCode.end()) return VK_NULL_HANDLE;

            vk::ShaderModuleCreateInfo createInfo = {};
            createInfo.codeSize = it->second.spirv_code->getBufferSize();
            createInfo.pCode = static_cast<const uint32_t*>(it->second.spirv_code->getBufferPointer());

            vk::ShaderModule shaderModule = device.createShaderModule(createInfo);
            return shaderModule;
        }

        ShaderReflection getReflectionData(slang::IModule* module)
        {
            
            std::vector<slang::IComponentType*> components = {module};
            
            for (uint32_t i = 0; i < module->getDefinedEntryPointCount(); i++)
            {
                slang::IEntryPoint* point = nullptr;
                module->getDefinedEntryPoint(i, &point);
                components.emplace_back(point);
            }

            Slang::ComPtr<slang::IComponentType> composedProgram;
            slang_session->createCompositeComponentType(
                components.data(), components.size(),
                composedProgram.writeRef());

            Slang::ComPtr<slang::IComponentType> linkedProgram;
            composedProgram->link(linkedProgram.writeRef());
            
            ShaderReflection reflection = {};
            ISlangBlob* test;
            linkedProgram->getLayout()->toJson(&test);

            const char* jsonData = static_cast<const char*>(test->getBufferPointer());
            size_t jsonSize = test->getBufferSize();

            reflection.raw_json = std::string(jsonData, jsonData + jsonSize);
            
            return reflection;
        }

        ~ShaderManager()
        {
            auto session = slang_session.detach();
            session->release();
            auto global = global_session.detach();
            global->release();
            clearCache();
        }
    private:
        Slang::ComPtr<slang::ISession> slang_session = nullptr;
        Slang::ComPtr<slang::IGlobalSession> global_session = nullptr;
        std::unordered_map<std::string, CompiledShader> CompiledShaderCode;
    };
    
    
};