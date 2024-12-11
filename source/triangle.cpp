#include <SPIRV/GlslangToSpv.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_main.h>

#include <fmt/core.h>

struct ApplicationContext {
    static constexpr char title[] = "SDL3 GPU minimal example";

    int width = 1920;
    int height = 1080;
    SDL_Window* window = nullptr;
    SDL_GPUDevice* graphicDevice = nullptr;
    SDL_GPUGraphicsPipeline* trianglePipeline = nullptr;
    SDL_GPUViewport viewport;
} app;

[[nodiscard]] auto compileGLSLtoSPIRV(std::string_view name, std::string_view source, EShLanguage stage) -> std::vector<uint32_t> {
    if (!glslang::InitializeProcess()) {
        return {};
    }

    glslang::TShader shader { stage };

    auto sourcePtr = std::data(source);
    shader.setStrings(&sourcePtr, 1);
    shader.setSourceEntryPoint("main");
    shader.setEntryPoint("main");
    shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientOpenGL, glslang::EShTargetOpenGL_450);

    auto includer = glslang::TShader::ForbidIncluder {};
    if (!shader.parse(GetDefaultResources(), glslang::EShTargetOpenGL_450, false,
            static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules), includer)) {
        fmt::println("ERROR ({}): {}", name, shader.getInfoLog());
        return {};
    }

    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(EShMsgDefault)) {
        fmt::println("ERROR ({}): {}", name, program.getInfoLog());
        return {};
    }

    auto intermediate = program.getIntermediate(stage);
    if (!intermediate) {
        return {};
    }

    std::vector<uint32_t> sprivBin;

    glslang::SpvOptions spirvOptions;
    glslang::GlslangToSpv(*intermediate, sprivBin, &spirvOptions);

    return sprivBin;
}

[[nodiscard]] SDL_GPUShader* loadShader(SDL_GPUDevice* device, std::string_view shaderFilePath, Uint32 samplerCount = 0,
    Uint32 uniformBufferCount = 0, Uint32 storageBufferCount = 0, Uint32 storageTextureCount = 0) {

    auto ext = shaderFilePath.find_last_of('.');
    if (ext == std::string_view::npos) {
        return nullptr;
    }

    EShLanguage stage {};
    SDL_GPUShaderStage shaderStage;

    if (shaderFilePath.substr(ext) == ".vert") {
        stage = EShLangVertex;
        shaderStage = SDL_GPU_SHADERSTAGE_VERTEX;
    } else if (shaderFilePath.substr(ext) == ".frag") {
        stage = EShLangFragment;
        shaderStage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    }

    size_t sourceSize = 0;
    auto source = SDL_LoadFile(std::data(shaderFilePath), &sourceSize);
    auto code = compileGLSLtoSPIRV(shaderFilePath, std::string_view { reinterpret_cast<const char*>(source), sourceSize }, stage);
    SDL_free(source);

    if (code.empty()) {
        fmt::println("Error: Failed to compile shader!");
        return nullptr;
    }

    SDL_GPUShaderCreateInfo shaderInfo {};
    shaderInfo.code_size = std::size(code) * sizeof(uint32_t);
    shaderInfo.code = reinterpret_cast<const uint8_t*>(std::data(code));
    shaderInfo.entrypoint = "main";
    shaderInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    shaderInfo.stage = shaderStage;
    shaderInfo.num_samplers = samplerCount;
    shaderInfo.num_storage_textures = storageTextureCount;
    shaderInfo.num_storage_buffers = storageBufferCount;
    shaderInfo.num_uniform_buffers = uniformBufferCount;

    if (auto shader = SDL_CreateGPUShader(device, &shaderInfo); shader) {
        return shader;
    }

    fmt::println("Error: Failed to create shader!");

    return nullptr;
}

SDL_AppResult SDL_AppInit([[maybe_unused]] void** appstate, [[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    SDL_SetAppMetadata(app.title, "1.0", "com.example.sdl3.gpu.minimal");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fmt::println("{}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (app.graphicDevice = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr); !app.graphicDevice) {
        fmt::println("{}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (app.window = SDL_CreateWindow(app.title, app.width, app.height, SDL_WINDOW_RESIZABLE); !app.window) {
        fmt::println("{}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(app.window, &w, &h);
    app.viewport.x = 0;
    app.viewport.y = 0;
    app.viewport.w = w;
    app.viewport.h = h;
    app.viewport.min_depth = 0.f;
    app.viewport.max_depth = 1.f;

    if (!SDL_ClaimWindowForGPUDevice(app.graphicDevice, app.window)) {
        fmt::println("{}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    auto vertexShader = loadShader(app.graphicDevice, "../shaders/BasicTriangle.vert");
    if (!vertexShader) {
        fmt::println("{}", "Failed to create vertex shader!");
        return SDL_APP_FAILURE;
    }

    auto fragmentShader = loadShader(app.graphicDevice, "../shaders/Basic.frag");
    if (!fragmentShader) {
        fmt::println("{}", "Failed to create fragment shader!");
        return SDL_APP_FAILURE;
    }

    SDL_GPUColorTargetDescription targetDesc {};
    targetDesc.format = SDL_GetGPUSwapchainTextureFormat(app.graphicDevice, app.window);

    SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo {};
    pipelineCreateInfo.vertex_shader = vertexShader;
    pipelineCreateInfo.fragment_shader = fragmentShader;
    pipelineCreateInfo.vertex_input_state.num_vertex_attributes = 0;
    pipelineCreateInfo.vertex_input_state.num_vertex_buffers = 0;
    pipelineCreateInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipelineCreateInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipelineCreateInfo.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;
    pipelineCreateInfo.target_info.num_color_targets = 1;
    pipelineCreateInfo.target_info.color_target_descriptions = &targetDesc;

    if (app.trianglePipeline = SDL_CreateGPUGraphicsPipeline(app.graphicDevice, &pipelineCreateInfo); !app.trianglePipeline) {
        fmt::println("{}", "Failed to create graphics pipeline!");
        return SDL_APP_FAILURE;
    }

    SDL_ReleaseGPUShader(app.graphicDevice, vertexShader);
    SDL_ReleaseGPUShader(app.graphicDevice, fragmentShader);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent([[maybe_unused]] void* appstate, SDL_Event* event) {
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    } else if (event->type == SDL_EVENT_WINDOW_RESIZED) {
        if (app.width != event->window.data1 || app.height != event->window.data2) {
            app.width = event->window.data1;
            app.height = event->window.data2;
            app.viewport.w = app.width;
            app.viewport.h = app.height;
        }
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate([[maybe_unused]] void* appstate) {
    auto commandBuffer = SDL_AcquireGPUCommandBuffer(app.graphicDevice);
    if (!commandBuffer) {
        fmt::println("AcquireGPUCommandBuffer {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUTexture* swapchainTexture = nullptr;
    if (!SDL_AcquireGPUSwapchainTexture(commandBuffer, app.window, &swapchainTexture, nullptr, nullptr)) {
        fmt::println("AcquireGPUSwapchainTexture {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (swapchainTexture) {
        SDL_GPUColorTargetInfo colorTargetInfo {};
        colorTargetInfo.texture = swapchainTexture;
        colorTargetInfo.clear_color = { 0.0f, 0.0f, 0.0f, 1.0f };
        colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
        colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

        SDL_Rect scissorRect {};
        scissorRect.x = 0;
        scissorRect.y = 0;
        scissorRect.w = app.viewport.w;
        scissorRect.h = app.viewport.h;

        auto renderPass = SDL_BeginGPURenderPass(commandBuffer, &colorTargetInfo, 1, nullptr);
        SDL_BindGPUGraphicsPipeline(renderPass, app.trianglePipeline);
        SDL_SetGPUViewport(renderPass, &app.viewport);
        SDL_SetGPUScissor(renderPass, &scissorRect);
        SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);
        SDL_EndGPURenderPass(renderPass);
    }

    SDL_SubmitGPUCommandBuffer(commandBuffer);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit([[maybe_unused]] void* appstate, [[maybe_unused]] SDL_AppResult result) {
    SDL_ReleaseGPUGraphicsPipeline(app.graphicDevice, app.trianglePipeline);

    SDL_ReleaseWindowFromGPUDevice(app.graphicDevice, app.window);
    SDL_DestroyWindow(app.window);
    SDL_DestroyGPUDevice(app.graphicDevice);
}
