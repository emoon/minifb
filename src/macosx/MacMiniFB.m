#include "OSXWindow.h"
#include "OSXWindowFrameView.h"
#include "OSXWindowData.h"
#include <MiniFB.h>
#include <MiniFB_enums.h>
#include <MiniFB_internal.h>
#include <Cocoa/Cocoa.h>
#if defined(USE_METAL_API)
#include <Carbon/Carbon.h>
#include <MetalKit/MetalKit.h>
#endif
#include <unistd.h>

SWindowData g_window_data = { 0 };

void init_keycodes();

#if defined(USE_METAL_API)
extern id<MTLDevice> g_metal_device;
extern id<MTLCommandQueue> g_command_queue;
extern id<MTLLibrary> g_library;
extern id<MTLRenderPipelineState> g_pipeline_state; 

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

	"vertex VertexOutput vertFunc2(unsigned int vID[[vertex_id]], device Vertex *pos [[buffer(0)]])\n"
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
static bool create_shaders() {
	// Error
	NSError* nsError = NULL;
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

    NSError *error = NULL;
    g_pipeline_state = [g_metal_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
    if (!g_pipeline_state)
    {
        NSLog(@"Failed to created pipeline state, error %@", error);
    }

	return true;
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int mfb_open(const char* name, int width, int height)
{
	return mfb_open_ex(name, width, height, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int mfb_open_ex(const char* name, int width, int height, int flags)
{
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

    init_keycodes();

	g_window_data.window_width  = width;
	g_window_data.window_height = height;

    g_window_data.dst_width     = width;
    g_window_data.dst_height    = height;

	g_window_data.buffer_width  = width;
	g_window_data.buffer_height = height;

	[NSApplication sharedApplication];
	[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

#if defined(USE_METAL_API)
	g_metal_device = MTLCreateSystemDefaultDevice();

	if (!g_metal_device) {
		printf("Your device/OS doesn't support Metal.");
		return -1;
	}

	if (!create_shaders()) {
		return -2;
	}
#endif

	NSWindowStyleMask styles =  NSWindowStyleMaskClosable | NSWindowStyleMaskTitled;

	if (flags & WF_BORDERLESS)
		styles |= NSWindowStyleMaskBorderless;

	if (flags & WF_RESIZABLE)
		styles |= NSWindowStyleMaskResizable;

	NSRect rectangle = NSMakeRect(0, 0, width, height);
	g_window_data.window = [[OSXWindow alloc] initWithContentRect:rectangle styleMask:styles backing:NSBackingStoreBuffered defer:NO];
	if (!g_window_data.window)
		return -3;

#if defined(USE_METAL_API)
	g_window_data.draw_buffer = malloc(width * height * 4);

	if (!g_window_data.draw_buffer)
		return -4;

	// Setup command queue
	g_command_queue = [g_metal_device newCommandQueue];

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
    viewController->m_draw_buffer = g_window_data.draw_buffer;
    viewController->m_width = width;
    viewController->m_height = height;

    MTKView* view = [[MTKView alloc] initWithFrame:rectangle];
    view.device = g_metal_device; 
    view.delegate = viewController;
    view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [g_window_data.window.contentView addSubview:view];

	g_window_data.buffer_width  = width;
	g_window_data.buffer_height = height;

	//[g_window_data.window updateSize];
#endif

	[g_window_data.window setTitle:[NSString stringWithUTF8String:name]];
	[g_window_data.window setReleasedWhenClosed:NO];
	[g_window_data.window performSelectorOnMainThread:@selector(makeKeyAndOrderFront:) withObject:nil waitUntilDone:YES];
	[g_window_data.window setAcceptsMouseMovedEvents:YES];

	[g_window_data.window center];

	[NSApp activateIgnoringOtherApps:YES];

#if defined(USE_METAL_API)
	[NSApp finishLaunching];
#endif

    if (g_keyboard_func == 0x0) {
        mfb_keyboard_callback(keyboard_default);
    }

#if defined(USE_METAL_API)
    NSLog(@"Window created using Metal API");
#else
    NSLog(@"Window created using Cocoa API");
#endif

	[pool drain];

	return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void keyboard_default(void *user_data, Key key, KeyMod mod, bool isPressed) {
    kUnused(user_data);
    kUnused(mod);
    kUnused(isPressed);
    if (key == KB_KEY_ESCAPE)
        g_window_data.close = true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void mfb_close()
{
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

	g_window_data.close = true;
	if (g_window_data.window)
		[g_window_data.window close]; 

	[pool drain];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int update_events()
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
    while ((g_window_data.close == false) && event);

	[pool release];

	if(g_window_data.close == true)
		return -1;

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int mfb_update(void* buffer)
{
	if(buffer == 0x0)
		return -2;

#if defined(USE_METAL_API)
	memcpy(g_window_data.draw_buffer, buffer, g_window_data.buffer_width * g_window_data.buffer_height * 4);
#else
	g_window_data.draw_buffer = buffer;
#endif

	int state = update_events();
	if(g_window_data.close == false)
		[[g_window_data.window contentView] setNeedsDisplay:YES];

	return state;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool mfb_set_viewport(unsigned offset_x, unsigned offset_y, unsigned width, unsigned height) 
{
    if(offset_x + width > g_window_data.window_width) {
        return false;
    }
    if(offset_y + height > g_window_data.window_height) {
        return false;
    }

    g_window_data.dst_offset_x = offset_x;
    g_window_data.dst_offset_y = offset_y;
    g_window_data.dst_width    = width;
    g_window_data.dst_height   = height;

#if defined(USE_METAL_API)
    float x1 =  ((float) offset_x           / g_window_data.window_width)  * 2.0f - 1.0f;
    float x2 = (((float) offset_x + width)  / g_window_data.window_width)  * 2.0f - 1.0f;
    float y1 =  ((float) offset_y           / g_window_data.window_height) * 2.0f - 1.0f;
    float y2 = (((float) offset_y + height) / g_window_data.window_height) * 2.0f - 1.0f;

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

extern short int keycodes[512];

void init_keycodes() 
{
    // Clear keys
    for (unsigned int i = 0; i < sizeof(keycodes) / sizeof(keycodes[0]); ++i) 
        keycodes[i] = 0;

    keycodes[0x1D] = KB_KEY_0;
    keycodes[0x12] = KB_KEY_1;
    keycodes[0x13] = KB_KEY_2;
    keycodes[0x14] = KB_KEY_3;
    keycodes[0x15] = KB_KEY_4;
    keycodes[0x17] = KB_KEY_5;
    keycodes[0x16] = KB_KEY_6;
    keycodes[0x1A] = KB_KEY_7;
    keycodes[0x1C] = KB_KEY_8;
    keycodes[0x19] = KB_KEY_9;
    keycodes[0x00] = KB_KEY_A;
    keycodes[0x0B] = KB_KEY_B;
    keycodes[0x08] = KB_KEY_C;
    keycodes[0x02] = KB_KEY_D;
    keycodes[0x0E] = KB_KEY_E;
    keycodes[0x03] = KB_KEY_F;
    keycodes[0x05] = KB_KEY_G;
    keycodes[0x04] = KB_KEY_H;
    keycodes[0x22] = KB_KEY_I;
    keycodes[0x26] = KB_KEY_J;
    keycodes[0x28] = KB_KEY_K;
    keycodes[0x25] = KB_KEY_L;
    keycodes[0x2E] = KB_KEY_M;
    keycodes[0x2D] = KB_KEY_N;
    keycodes[0x1F] = KB_KEY_O;
    keycodes[0x23] = KB_KEY_P;
    keycodes[0x0C] = KB_KEY_Q;
    keycodes[0x0F] = KB_KEY_R;
    keycodes[0x01] = KB_KEY_S;
    keycodes[0x11] = KB_KEY_T;
    keycodes[0x20] = KB_KEY_U;
    keycodes[0x09] = KB_KEY_V;
    keycodes[0x0D] = KB_KEY_W;
    keycodes[0x07] = KB_KEY_X;
    keycodes[0x10] = KB_KEY_Y;
    keycodes[0x06] = KB_KEY_Z;
    
    keycodes[0x27] = KB_KEY_APOSTROPHE;
    keycodes[0x2A] = KB_KEY_BACKSLASH;
    keycodes[0x2B] = KB_KEY_COMMA;
    keycodes[0x18] = KB_KEY_EQUAL;
    keycodes[0x32] = KB_KEY_GRAVE_ACCENT;
    keycodes[0x21] = KB_KEY_LEFT_BRACKET;
    keycodes[0x1B] = KB_KEY_MINUS;
    keycodes[0x2F] = KB_KEY_PERIOD;
    keycodes[0x1E] = KB_KEY_RIGHT_BRACKET;
    keycodes[0x29] = KB_KEY_SEMICOLON;
    keycodes[0x2C] = KB_KEY_SLASH;
    keycodes[0x0A] = KB_KEY_WORLD_1;
    
    keycodes[0x33] = KB_KEY_BACKSPACE;
    keycodes[0x39] = KB_KEY_CAPS_LOCK;
    keycodes[0x75] = KB_KEY_DELETE;
    keycodes[0x7D] = KB_KEY_DOWN;
    keycodes[0x77] = KB_KEY_END;
    keycodes[0x24] = KB_KEY_ENTER;
    keycodes[0x35] = KB_KEY_ESCAPE;
    keycodes[0x7A] = KB_KEY_F1;
    keycodes[0x78] = KB_KEY_F2;
    keycodes[0x63] = KB_KEY_F3;
    keycodes[0x76] = KB_KEY_F4;
    keycodes[0x60] = KB_KEY_F5;
    keycodes[0x61] = KB_KEY_F6;
    keycodes[0x62] = KB_KEY_F7;
    keycodes[0x64] = KB_KEY_F8;
    keycodes[0x65] = KB_KEY_F9;
    keycodes[0x6D] = KB_KEY_F10;
    keycodes[0x67] = KB_KEY_F11;
    keycodes[0x6F] = KB_KEY_F12;
    keycodes[0x69] = KB_KEY_F13;
    keycodes[0x6B] = KB_KEY_F14;
    keycodes[0x71] = KB_KEY_F15;
    keycodes[0x6A] = KB_KEY_F16;
    keycodes[0x40] = KB_KEY_F17;
    keycodes[0x4F] = KB_KEY_F18;
    keycodes[0x50] = KB_KEY_F19;
    keycodes[0x5A] = KB_KEY_F20;
    keycodes[0x73] = KB_KEY_HOME;
    keycodes[0x72] = KB_KEY_INSERT;
    keycodes[0x7B] = KB_KEY_LEFT;
    keycodes[0x3A] = KB_KEY_LEFT_ALT;
    keycodes[0x3B] = KB_KEY_LEFT_CONTROL;
    keycodes[0x38] = KB_KEY_LEFT_SHIFT;
    keycodes[0x37] = KB_KEY_LEFT_SUPER;
    keycodes[0x6E] = KB_KEY_MENU;
    keycodes[0x47] = KB_KEY_NUM_LOCK;
    keycodes[0x79] = KB_KEY_PAGE_DOWN;
    keycodes[0x74] = KB_KEY_PAGE_UP;
    keycodes[0x7C] = KB_KEY_RIGHT;
    keycodes[0x3D] = KB_KEY_RIGHT_ALT;
    keycodes[0x3E] = KB_KEY_RIGHT_CONTROL;
    keycodes[0x3C] = KB_KEY_RIGHT_SHIFT;
    keycodes[0x36] = KB_KEY_RIGHT_SUPER;
    keycodes[0x31] = KB_KEY_SPACE;
    keycodes[0x30] = KB_KEY_TAB;
    keycodes[0x7E] = KB_KEY_UP;
    
    keycodes[0x52] = KB_KEY_KP_0;
    keycodes[0x53] = KB_KEY_KP_1;
    keycodes[0x54] = KB_KEY_KP_2;
    keycodes[0x55] = KB_KEY_KP_3;
    keycodes[0x56] = KB_KEY_KP_4;
    keycodes[0x57] = KB_KEY_KP_5;
    keycodes[0x58] = KB_KEY_KP_6;
    keycodes[0x59] = KB_KEY_KP_7;
    keycodes[0x5B] = KB_KEY_KP_8;
    keycodes[0x5C] = KB_KEY_KP_9;
    keycodes[0x45] = KB_KEY_KP_ADD;
    keycodes[0x41] = KB_KEY_KP_DECIMAL;
    keycodes[0x4B] = KB_KEY_KP_DIVIDE;
    keycodes[0x4C] = KB_KEY_KP_ENTER;
    keycodes[0x51] = KB_KEY_KP_EQUAL;
    keycodes[0x43] = KB_KEY_KP_MULTIPLY;
    keycodes[0x4E] = KB_KEY_KP_SUBTRACT;
}
