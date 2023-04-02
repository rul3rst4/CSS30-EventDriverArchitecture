// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#include <Windows.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include <iostream>
#include <librdkafka/rdkafkacpp.h>
#include <optional>
#include <format>

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif
#include <future>
#include <implot_internal.h>

struct ScrollingBuffer {
    int MaxSize;
    int Offset;
    ImVector<ImVec2> Data;
    ScrollingBuffer(int max_size = 2000) {
        MaxSize = max_size;
        Offset = 0;
        Data.reserve(MaxSize);
    }
    void AddPoint(float x, float y) {
        if (Data.size() < MaxSize)
            Data.push_back(ImVec2(x, y));
        else {
            Data[Offset] = ImVec2(x, y);
            Offset = (Offset + 1) % MaxSize;
        }
    }
    void Erase() {
        if (Data.size() > 0) {
            Data.shrink(0);
            Offset = 0;
        }
    }
};

// utility structure for realtime plot
struct RollingBuffer {
    float Span;
    ImVector<ImVec2> Data;
    RollingBuffer() {
        Span = 10.0f;
        Data.reserve(2000);
    }
    void AddPoint(float x, float y) {
        //float xmod = fmodf(x, Span);
        //if (!Data.empty() && xmod < Data.back().x)
        //    Data.shrink(0);
        Data.push_back(ImVec2(x, y));
    }
};

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

class KafkaConsumer {
public:
    std::unique_ptr<RdKafka::KafkaConsumer> consumer;
    std::unique_ptr<RdKafka::Conf> config;
    const std::string topic;
    const std::string brokers = "localhost:9092";
    std::string errstr;
    std::future<std::unique_ptr<RdKafka::Message>> message_receiver;
    RollingBuffer  rdata;

public:
    KafkaConsumer(const std::string& topic) : topic{topic}
    {
        config = std::unique_ptr<RdKafka::Conf>(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));

        if (config->set("bootstrap.servers", brokers, errstr) !=
            RdKafka::Conf::CONF_OK) {
            std::cerr << errstr << std::endl;
            exit(1);
        }

        if (config->set("group.id", "1", errstr) != RdKafka::Conf::CONF_OK) {
            std::cerr << errstr << std::endl;
            exit(1);
        }

        consumer = std::unique_ptr<RdKafka::KafkaConsumer>(RdKafka::KafkaConsumer::create(config.get(), errstr));

        if (!consumer) {
            std::cerr << "Failed to create consumer: " << errstr << std::endl;
            exit(1);
        }
        std::vector<std::string> topics{topic};
        consumer->subscribe(topics);
        run();
    }

    ~KafkaConsumer() {
        consumer->close();
    }

    void run() {
        message_receiver = std::async(std::launch::async, [this]() {
            auto msg = std::unique_ptr<RdKafka::Message>(consumer->consume(1000));
            return msg;
        });
    }

    std::optional<float> is_message_ready() {
        if (message_receiver.valid() && message_receiver.wait_for(std::chrono::seconds(0)) ==
            std::future_status::ready) {
                auto result = message_receiver.get();
                run();
                return msg_consume(result.get());
        }

        return {};
    }

    std::optional<float> msg_consume(RdKafka::Message* message) const {
        const RdKafka::Headers* headers;

        switch (message->err()) {
        case RdKafka::ERR__TIMED_OUT:
            break;

        case RdKafka::ERR_NO_ERROR:
            /* Real message */
            return std::stof(std::string(static_cast<const char *>(message->payload()), message->len()));
            break;

        case RdKafka::ERR__PARTITION_EOF:
            /* Last message */
            break;

        case RdKafka::ERR__UNKNOWN_TOPIC:
        case RdKafka::ERR__UNKNOWN_PARTITION:
            std::cerr << "Consume failed: " << message->errstr() << std::endl;
            break;

        default:
            /* Errors */
            std::cerr << "Consume failed: " << message->errstr() << std::endl;
        }

        return {};
    }
};

void ShowRealTimeData() {
    static std::vector<std::unique_ptr<KafkaConsumer>> consumers;
    static std::unordered_map<std::string, bool> consumers_enabled{
        {"bitcoin", false},
        {"ethereum", false},
        {"monero", false},
        {"litecoin", false},
        {"dogecoin", false}
    };

    ImGui::Begin("Available Consumers");
    for (auto& [consumer, enabled] : consumers_enabled) {
        if (ImGui::Checkbox(consumer.c_str(), &enabled)) {
            if (enabled) {
                consumers.emplace_back(std::make_unique<KafkaConsumer>(consumer));
            }
            else {
                std::erase_if(consumers, [&consumer](const std::unique_ptr<KafkaConsumer>& c) {
                    return c->topic == consumer;
                });
            }
        }
    }
    
    ImGui::End();

    ImGui::BulletText("This example assumes 60 FPS. Higher FPS requires larger buffer size.");
    static float t = 0;
    t += ImGui::GetIO().DeltaTime;

    static float history = 10.0f;
    ImGui::SliderFloat("History", &history, 1, 3600, "%.1f s");

    //static ImPlotAxisFlags flags = ImPlotAxisFlags_NoTickLabels;

    if (ImPlot::BeginPlot("##Rolling", ImVec2(-1, 800))) {
        ImPlot::SetupAxes("time", "value");
        //ImPlot::SetupAxesLimits(0, history, -150, 30000);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, history, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -200, 30000);
        for (auto& consumer : consumers) {
            auto lastValue = consumer->rdata.Data.empty() ? 0 : consumer->rdata.Data.back().y;
            consumer->rdata.AddPoint(t, consumer->is_message_ready().value_or(lastValue));
            consumer->rdata.Span = history;

            //ImPlot::SetupAxis(ImAxis_X2, consumer->topic.c_str(), ImPlotAxisFlags_AuxDefault);
            //ImPlot::SetupAxisLimits(ImAxis_X2, 0, 100);

            //ImPlot::SetAxes(ImAxis_X2, ImAxis_Y1);
            ImPlot::PlotLine(consumer->topic.c_str(), &consumer->rdata.Data[0].x, &consumer->rdata.Data[0].y, consumer->rdata.Data.size(), 0, 0, 2 * sizeof(float));

            ImPlotContext& gp = *GImPlot;
            IM_ASSERT_USER_ERROR(gp.CurrentItems != NULL, "IsPlotItemHighlight() needs to be called within an itemized context!");
            ImPlot::SetupLock();
            ImGuiID id = ImGui::GetIDWithSeed(consumer->topic.c_str(), NULL, gp.CurrentItems->ID);
            ImPlotItem* item = gp.CurrentItems->GetItem(id);

            if(item && item->Show) {
                ImVec4 col = ImPlot::GetLastItemColor();
                ImPlot::Annotation(t, lastValue, col, ImVec2(0, 0), true, std::to_string(lastValue).c_str());


            }

        }
        ImPlot::EndPlot();
    }
}

// Main code
int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "Dear ImGui GLFW+OpenGL3+CG", NULL, NULL);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    glewExperimental = true;
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        std::cerr << "Error: " << glewGetErrorString(err) << std::endl;
        glfwTerminate();
        return -1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    //InitConsumer();

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    // Our state
    bool show_demo_window = true;
    auto clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = NULL;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    while (!glfwWindowShouldClose(window))
#endif
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);
        ImPlot::ShowDemoWindow();

        static GLuint zbuffer_bit = 0;
        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            ImGui::Begin("Info");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state

            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::Text("OpenGL Version: %s", glGetString(GL_VERSION));

            ImGui::End();
        }

        ShowRealTimeData();

        // Rendering
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT | zbuffer_bit);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    ImPlot::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
