#include "OSXWindow.h"
#include "OSXWindowFrameView.h"
#include "WindowData_OSX.h"
#include <MiniFB.h>
#include <MiniFB_enums.h>
#include <MiniFB_internal.h>
#include <Cocoa/Cocoa.h>
#if defined(USE_METAL_API)
#include <Carbon/Carbon.h>
#include <MetalKit/MetalKit.h>
#endif
#include <unistd.h>

void init_keycodes();

#if defined(USE_METAL_API)
id<MTLDevice>  g_metal_device;
id<MTLLibrary> g_library;

Vertex gVertices[4] = {
    {-1.0, -1.0, 0, 1},
    {-1.0,  1.0, 0, 1},
    { 1.0, -1.0, 0, 1},
    { 1.0,  1.0, 0, 1},
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

NSString* g_shadersSrc = @
"	#include <metal_stdlib>\n"  
    "using namespace metal;\n"

    "struct VertexOutput {\n"
        "float4 pos [[position]];\n"
        "float2 texcoord;\n"
    "};\n"

    "vertex VertexOutput vertFunc(unsigned int vID[[vertex_id]])\n"
    "{\n"
        "VertexOutput out;\n"

        "out.pos.x = (float)(vID / 2) * 4.0 - 1.0;\n"
        "out.pos.y = (float)(vID % 2) * 4.0 - 1.0;\n"
        "out.pos.z = 0.0;\n"
        "out.pos.w = 1.0;\n"

        "out.texcoord.x = (float)(vID / 2) * 2.0;\n"
        "out.texcoord.y = 1.0 - (float)(vID % 2) * 2.0;\n"

        "return out;\n"
    "}\n"

    "struct Vertex\n"
    "{\n"
        "float4 position [[position]];\n"
    "};\n"

    "vertex VertexOutput vertFunc2(unsigned int vID[[vertex_id]], const device Vertex *pos [[buffer(0)]])\n"
    "{\n"
        "VertexOutput out;\n"

        "out.pos = pos[vID].position;\n"

        "out.texcoord.x = (float)(vID / 2);\n"
        "out.texcoord.y = 1.0 - (float)(vID % 2);\n"

        "return out;\n"
    "}\n"

    "fragment float4 fragFunc(VertexOutput input [[stage_in]],\n"
            "texture2d<half> colorTexture [[ texture(0) ]])\n"
    "{\n"
        "constexpr sampler textureSampler(mag_filter::nearest, min_filter::nearest);\n"

        // Sample the texture to obtain a color
        "const half4 colorSample = colorTexture.sample(textureSampler, input.texcoord);\n"

        // We return the color of the texture
        "return float4(colorSample);\n"
        //"return float4(input.texcoord.x, input.texcoord.y, 0.0, 1.0);\n"
    "}\n";

#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(USE_METAL_API)
static bool 
create_shaders(SWindowData_OSX *window_data_osx) {
    // Error
    NSError* nsError = 0x0;
    NSError** nsErrorPtr = &nsError;

    id<MTLLibrary> library = [g_metal_device newLibraryWithSource:g_shadersSrc
        options:[[MTLCompileOptions alloc] init]
        error:nsErrorPtr];

    // Error update
    if (nsError || !library) {
        NSLog(@"Unable to create shaders %@", nsError); 
        return false;
    }                            

    g_library = library;
    NSLog(@"Names %@", [g_library functionNames]);

    id<MTLFunction> vertex_shader_func = [g_library newFunctionWithName:@"vertFunc2"];
    id<MTLFunction> fragment_shader_func = [g_library newFunctionWithName:@"fragFunc"];

    if (!vertex_shader_func) {
        printf("Unable to get vertFunc!\n");
        return false;
    }

    if (!fragment_shader_func) {
        printf("Unable to get fragFunc!\n");
        return false;
    }

    // Create a reusable pipeline state
    MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineStateDescriptor.label = @"MyPipeline";
    pipelineStateDescriptor.vertexFunction = vertex_shader_func;
    pipelineStateDescriptor.fragmentFunction = fragment_shader_func;
    pipelineStateDescriptor.colorAttachments[0].pixelFormat = 80; //bgra8Unorm;

    NSError *error = 0x0;
    window_data_osx->metal.pipeline_state = [g_metal_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
    if (!window_data_osx->metal.pipeline_state)
    {
        NSLog(@"Failed to created pipeline state, error %@", error);
    }

    return true;
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct mfb_window *
mfb_open(const char *title, unsigned width, unsigned height)
{
    return mfb_open_ex(title, width, height, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct mfb_window *
mfb_open_ex(const char *title, unsigned width, unsigned height, unsigned flags)
{
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

    init_keycodes();

    SWindowData *window_data = malloc(sizeof(SWindowData));
    memset(window_data, 0, sizeof(SWindowData));

    SWindowData_OSX *window_data_osx = malloc(sizeof(SWindowData_OSX));
    memset(window_data_osx, 0, sizeof(SWindowData_OSX));
    window_data->specific = window_data_osx;

    window_data->window_width  = width;
    window_data->window_height = height;

    window_data->dst_width     = width;
    window_data->dst_height    = height;

    window_data->buffer_width  = width;
    window_data->buffer_height = height;
    window_data->buffer_stride = width * 4;

    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

#if defined(USE_METAL_API)
    g_metal_device = MTLCreateSystemDefaultDevice();

    if (!g_metal_device) {
        printf("Your device/OS doesn't support Metal.");
        return 0x0;
    }

    if (!create_shaders((SWindowData_OSX *) window_data->specific)) {
        return 0x0;
    }
#endif

    NSWindowStyleMask styles =  NSWindowStyleMaskClosable | NSWindowStyleMaskTitled;

    if (flags & WF_BORDERLESS)
        styles |= NSWindowStyleMaskBorderless;

    if (flags & WF_RESIZABLE)
        styles |= NSWindowStyleMaskResizable;

    NSRect rectangle = NSMakeRect(0, 0, width, height);
    window_data_osx->window = [[OSXWindow alloc] initWithContentRect:rectangle styleMask:styles backing:NSBackingStoreBuffered defer:NO windowData:window_data];
    if (!window_data_osx->window)
        return 0x0;

#if defined(USE_METAL_API)
    window_data->draw_buffer = malloc(width * height * 4);

    if (!window_data->draw_buffer)
        return 0x0;

    // Setup command queue
    window_data_osx->metal.command_queue = [g_metal_device newCommandQueue];

    WindowViewController* viewController = [WindowViewController new];

    MTLTextureDescriptor* textureDescriptor = [[MTLTextureDescriptor alloc] init];

    // Indicate that each pixel has a blue, green, red, and alpha channel, where each channel is
    // an 8-bit unsigned normalized value (i.e. 0 maps to 0.0 and 255 maps to 1.0)
    textureDescriptor.pixelFormat = MTLPixelFormatBGRA8Unorm;

    // Set the pixel dimensions of the texture
    textureDescriptor.width = width;
    textureDescriptor.height = height;

    // Create the texture from the device by using the descriptor
    
    for (int i = 0; i < MaxBuffersInFlight; ++i) {
        viewController->m_texture_buffers[i] = [g_metal_device newTextureWithDescriptor:textureDescriptor];
    }

    // Used for syncing the CPU and GPU
    viewController->m_semaphore = dispatch_semaphore_create(MaxBuffersInFlight);
    viewController->m_draw_buffer = window_data->draw_buffer;
    viewController->m_width = width;
    viewController->m_height = height;

    MTKView* view = [[MTKView alloc] initWithFrame:rectangle];
    view.device = g_metal_device; 
    view.delegate = viewController;
    view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [window_data_osx->window.contentView addSubview:view];

    //window_data->buffer_width  = width;
    //window_data->buffer_height = height;
    //window_data->buffer_stride = width * 4;

    //[window_data->window updateSize];
#endif

    [window_data_osx->window setTitle:[NSString stringWithUTF8String:title]];
    [window_data_osx->window setReleasedWhenClosed:NO];
    [window_data_osx->window performSelectorOnMainThread:@selector(makeKeyAndOrderFront:) withObject:nil waitUntilDone:YES];
    [window_data_osx->window setAcceptsMouseMovedEvents:YES];

    [window_data_osx->window center];

    [NSApp activateIgnoringOtherApps:YES];

#if defined(USE_METAL_API)
    [NSApp finishLaunching];
#endif

    mfb_set_keyboard_callback((struct mfb_window *) window_data, keyboard_default);

#if defined(USE_METAL_API)
    NSLog(@"Window created using Metal API");
#else
    NSLog(@"Window created using Cocoa API");
#endif

    [pool drain];

    return (struct mfb_window *) window_data;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void 
destroy_window_data(SWindowData *window_data) 
{
    if(window_data == 0x0)
        return;

    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    
    SWindowData_OSX   *window_data_osx = (SWindowData_OSX *) window_data->specific;
    if(window_data_osx != 0x0) {
        OSXWindow   *window = window_data_osx->window;
        [window removeWindowData];
        [window performClose:nil];

        memset(window_data_osx, 0, sizeof(SWindowData_OSX));
        free(window_data_osx);
    }
    memset(window_data, 0, sizeof(SWindowData));
    free(window_data);

    [pool drain];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void 
update_events(SWindowData *window_data)
{
    NSEvent* event;

    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    do
    {
        event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES];
        if (event) {
            [NSApp sendEvent:event];
        }
    }
    while ((window_data->close == false) && event);

    [pool release];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

mfb_update_state 
mfb_update(struct mfb_window *window, void *buffer)
{
    if(window == 0x0) {
        return STATE_INVALID_WINDOW;
    }

    SWindowData *window_data = (SWindowData *) window;
    if(window_data->close) {
        destroy_window_data(window_data);
        return STATE_EXIT;
    }

    if(buffer == 0x0) {
        return STATE_INVALID_BUFFER;
    }

#if defined(USE_METAL_API)
    memcpy(window_data->draw_buffer, buffer, window_data->buffer_width * window_data->buffer_height * 4);
#else
    window_data->draw_buffer = buffer;
#endif

    update_events(window_data);
    if(window_data->close == false) {
        SWindowData_OSX *window_data_osx = (SWindowData_OSX *) window_data->specific;
        [[window_data_osx->window contentView] setNeedsDisplay:YES];
    }

    return STATE_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

mfb_update_state 
mfb_update_events(struct mfb_window *window)
{
    if(window == 0x0) {
        return STATE_INVALID_WINDOW;
    }

    SWindowData *window_data = (SWindowData *) window;
    if(window_data->close) {
        destroy_window_data(window_data);
        return STATE_EXIT;
    }

    update_events(window_data);
    if(window_data->close == false) {
        SWindowData_OSX *window_data_osx = (SWindowData_OSX *) window_data->specific;
        [[window_data_osx->window contentView] setNeedsDisplay:YES];
    }

    return STATE_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool 
mfb_set_viewport(struct mfb_window *window, unsigned offset_x, unsigned offset_y, unsigned width, unsigned height) 
{
    SWindowData *window_data = (SWindowData *) window;

    if(offset_x + width > window_data->window_width) {
        return false;
    }
    if(offset_y + height > window_data->window_height) {
        return false;
    }

    window_data->dst_offset_x = offset_x;
    window_data->dst_offset_y = offset_y;
    window_data->dst_width    = width;
    window_data->dst_height   = height;

#if defined(USE_METAL_API)
    float x1 =  ((float) offset_x           / window_data->window_width)  * 2.0f - 1.0f;
    float x2 = (((float) offset_x + width)  / window_data->window_width)  * 2.0f - 1.0f;
    float y1 =  ((float) offset_y           / window_data->window_height) * 2.0f - 1.0f;
    float y2 = (((float) offset_y + height) / window_data->window_height) * 2.0f - 1.0f;

    gVertices[0].x = x1;
    gVertices[0].y = y1;

    gVertices[1].x = x1;
    gVertices[1].y = y2;

    gVertices[2].x = x2;
    gVertices[2].y = y1;

    gVertices[3].x = x2;
    gVertices[3].y = y2;
#endif

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern short int g_keycodes[512];

void 
init_keycodes() 
{
    // Clear keys
    for (unsigned int i = 0; i < sizeof(g_keycodes) / sizeof(g_keycodes[0]); ++i) 
        g_keycodes[i] = 0;

    g_keycodes[0x1D] = KB_KEY_0;
    g_keycodes[0x12] = KB_KEY_1;
    g_keycodes[0x13] = KB_KEY_2;
    g_keycodes[0x14] = KB_KEY_3;
    g_keycodes[0x15] = KB_KEY_4;
    g_keycodes[0x17] = KB_KEY_5;
    g_keycodes[0x16] = KB_KEY_6;
    g_keycodes[0x1A] = KB_KEY_7;
    g_keycodes[0x1C] = KB_KEY_8;
    g_keycodes[0x19] = KB_KEY_9;
    g_keycodes[0x00] = KB_KEY_A;
    g_keycodes[0x0B] = KB_KEY_B;
    g_keycodes[0x08] = KB_KEY_C;
    g_keycodes[0x02] = KB_KEY_D;
    g_keycodes[0x0E] = KB_KEY_E;
    g_keycodes[0x03] = KB_KEY_F;
    g_keycodes[0x05] = KB_KEY_G;
    g_keycodes[0x04] = KB_KEY_H;
    g_keycodes[0x22] = KB_KEY_I;
    g_keycodes[0x26] = KB_KEY_J;
    g_keycodes[0x28] = KB_KEY_K;
    g_keycodes[0x25] = KB_KEY_L;
    g_keycodes[0x2E] = KB_KEY_M;
    g_keycodes[0x2D] = KB_KEY_N;
    g_keycodes[0x1F] = KB_KEY_O;
    g_keycodes[0x23] = KB_KEY_P;
    g_keycodes[0x0C] = KB_KEY_Q;
    g_keycodes[0x0F] = KB_KEY_R;
    g_keycodes[0x01] = KB_KEY_S;
    g_keycodes[0x11] = KB_KEY_T;
    g_keycodes[0x20] = KB_KEY_U;
    g_keycodes[0x09] = KB_KEY_V;
    g_keycodes[0x0D] = KB_KEY_W;
    g_keycodes[0x07] = KB_KEY_X;
    g_keycodes[0x10] = KB_KEY_Y;
    g_keycodes[0x06] = KB_KEY_Z;
    
    g_keycodes[0x27] = KB_KEY_APOSTROPHE;
    g_keycodes[0x2A] = KB_KEY_BACKSLASH;
    g_keycodes[0x2B] = KB_KEY_COMMA;
    g_keycodes[0x18] = KB_KEY_EQUAL;
    g_keycodes[0x32] = KB_KEY_GRAVE_ACCENT;
    g_keycodes[0x21] = KB_KEY_LEFT_BRACKET;
    g_keycodes[0x1B] = KB_KEY_MINUS;
    g_keycodes[0x2F] = KB_KEY_PERIOD;
    g_keycodes[0x1E] = KB_KEY_RIGHT_BRACKET;
    g_keycodes[0x29] = KB_KEY_SEMICOLON;
    g_keycodes[0x2C] = KB_KEY_SLASH;
    g_keycodes[0x0A] = KB_KEY_WORLD_1;
    
    g_keycodes[0x33] = KB_KEY_BACKSPACE;
    g_keycodes[0x39] = KB_KEY_CAPS_LOCK;
    g_keycodes[0x75] = KB_KEY_DELETE;
    g_keycodes[0x7D] = KB_KEY_DOWN;
    g_keycodes[0x77] = KB_KEY_END;
    g_keycodes[0x24] = KB_KEY_ENTER;
    g_keycodes[0x35] = KB_KEY_ESCAPE;
    g_keycodes[0x7A] = KB_KEY_F1;
    g_keycodes[0x78] = KB_KEY_F2;
    g_keycodes[0x63] = KB_KEY_F3;
    g_keycodes[0x76] = KB_KEY_F4;
    g_keycodes[0x60] = KB_KEY_F5;
    g_keycodes[0x61] = KB_KEY_F6;
    g_keycodes[0x62] = KB_KEY_F7;
    g_keycodes[0x64] = KB_KEY_F8;
    g_keycodes[0x65] = KB_KEY_F9;
    g_keycodes[0x6D] = KB_KEY_F10;
    g_keycodes[0x67] = KB_KEY_F11;
    g_keycodes[0x6F] = KB_KEY_F12;
    g_keycodes[0x69] = KB_KEY_F13;
    g_keycodes[0x6B] = KB_KEY_F14;
    g_keycodes[0x71] = KB_KEY_F15;
    g_keycodes[0x6A] = KB_KEY_F16;
    g_keycodes[0x40] = KB_KEY_F17;
    g_keycodes[0x4F] = KB_KEY_F18;
    g_keycodes[0x50] = KB_KEY_F19;
    g_keycodes[0x5A] = KB_KEY_F20;
    g_keycodes[0x73] = KB_KEY_HOME;
    g_keycodes[0x72] = KB_KEY_INSERT;
    g_keycodes[0x7B] = KB_KEY_LEFT;
    g_keycodes[0x3A] = KB_KEY_LEFT_ALT;
    g_keycodes[0x3B] = KB_KEY_LEFT_CONTROL;
    g_keycodes[0x38] = KB_KEY_LEFT_SHIFT;
    g_keycodes[0x37] = KB_KEY_LEFT_SUPER;
    g_keycodes[0x6E] = KB_KEY_MENU;
    g_keycodes[0x47] = KB_KEY_NUM_LOCK;
    g_keycodes[0x79] = KB_KEY_PAGE_DOWN;
    g_keycodes[0x74] = KB_KEY_PAGE_UP;
    g_keycodes[0x7C] = KB_KEY_RIGHT;
    g_keycodes[0x3D] = KB_KEY_RIGHT_ALT;
    g_keycodes[0x3E] = KB_KEY_RIGHT_CONTROL;
    g_keycodes[0x3C] = KB_KEY_RIGHT_SHIFT;
    g_keycodes[0x36] = KB_KEY_RIGHT_SUPER;
    g_keycodes[0x31] = KB_KEY_SPACE;
    g_keycodes[0x30] = KB_KEY_TAB;
    g_keycodes[0x7E] = KB_KEY_UP;
    
    g_keycodes[0x52] = KB_KEY_KP_0;
    g_keycodes[0x53] = KB_KEY_KP_1;
    g_keycodes[0x54] = KB_KEY_KP_2;
    g_keycodes[0x55] = KB_KEY_KP_3;
    g_keycodes[0x56] = KB_KEY_KP_4;
    g_keycodes[0x57] = KB_KEY_KP_5;
    g_keycodes[0x58] = KB_KEY_KP_6;
    g_keycodes[0x59] = KB_KEY_KP_7;
    g_keycodes[0x5B] = KB_KEY_KP_8;
    g_keycodes[0x5C] = KB_KEY_KP_9;
    g_keycodes[0x45] = KB_KEY_KP_ADD;
    g_keycodes[0x41] = KB_KEY_KP_DECIMAL;
    g_keycodes[0x4B] = KB_KEY_KP_DIVIDE;
    g_keycodes[0x4C] = KB_KEY_KP_ENTER;
    g_keycodes[0x51] = KB_KEY_KP_EQUAL;
    g_keycodes[0x43] = KB_KEY_KP_MULTIPLY;
    g_keycodes[0x4E] = KB_KEY_KP_SUBTRACT;
}
