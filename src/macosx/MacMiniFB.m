#include "OSXWindow.h"
#include "OSXWindowFrameView.h"
#include <Cocoa/Cocoa.h>
#if defined(USE_METAL_API)
#include <Carbon/Carbon.h>
#include <MetalKit/MetalKit.h>
#endif
#include <unistd.h>
#include "MiniFB.h"

#if defined(USE_METAL_API)
extern id<MTLDevice> g_metal_device;
extern id<MTLCommandQueue> g_command_queue;
extern id<MTLLibrary> g_library;
extern id<MTLRenderPipelineState> g_pipeline_state; 

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

NSString* g_shadersSrc = @
"	#include <metal_stdlib>\n"  
	"using namespace metal;\n"

	"struct VertexOutput {\n"
		"float4 pos [[position]];\n"
		"float2 texcoord;\n"
	"};\n"

	"vertex VertexOutput vertFunc(\n"
		"unsigned int vID[[vertex_id]])\n"
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

	"fragment float4 fragFunc(VertexOutput input [[stage_in]],\n"
			"texture2d<half> colorTexture [[ texture(0) ]])\n"
	"{\n"
	    "constexpr sampler textureSampler(mag_filter::nearest, min_filter::nearest);\n"

		// Sample the texture to obtain a color
		"const half4 colorSample = colorTexture.sample(textureSampler, input.texcoord);\n"

    	// We return the color of the texture
    	"return float4(colorSample);\n"
		//"return half4(input.texcoord.x, input.texcoord.y, 0.0, 1.0);\n"
	"}\n";

#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if !defined(USE_METAL_API)
void* g_updateBuffer = 0;
int g_width = 0;
int g_height = 0;
#endif
static OSXWindow *s_window;

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

	id<MTLFunction> vertex_shader_func = [g_library newFunctionWithName:@"vertFunc"];
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
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

#if !defined(USE_METAL_API)
	g_width = width;
	g_height = height;
#endif
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

	NSWindowStyleMask styles = NSWindowStyleMaskResizable | NSWindowStyleMaskClosable | NSWindowStyleMaskTitled;

	NSRect rectangle = NSMakeRect(0, 0, width, height);
	s_window = [[OSXWindow alloc] initWithContentRect:rectangle styleMask:styles backing:NSBackingStoreBuffered defer:NO];

	if (!s_window)
		return -3;

#if defined(USE_METAL_API)
	s_window->draw_buffer = malloc(width * height * 4);

	if (!s_window->draw_buffer)
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
    viewController->m_draw_buffer = s_window->draw_buffer;
    viewController->m_width = width;
    viewController->m_height = height;

    MTKView* view = [[MTKView alloc] initWithFrame:rectangle];
    view.device = g_metal_device; 
    view.delegate = viewController;
    view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [s_window.contentView addSubview:view];

	s_window->width = width;
	s_window->height = height;

	//[s_window updateSize];
#endif

	[s_window setTitle:[NSString stringWithUTF8String:name]];
	[s_window setReleasedWhenClosed:NO];
	[s_window performSelectorOnMainThread:@selector(makeKeyAndOrderFront:) withObject:nil waitUntilDone:YES];

	[s_window center];

	[NSApp activateIgnoringOtherApps:YES];

#if defined(USE_METAL_API)
	[NSApp finishLaunching];
#endif

	[pool drain];

	return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void mfb_close()
{
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

	if (s_window)
		[s_window close]; 

	[pool drain];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int updateEvents()
{
	int state = 0;
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES];
	if (event)
	{
		switch ([event type])
		{
			case NSEventTypeKeyDown:
			case NSEventTypeKeyUp:
			{
				state = -1;
				break;
			}

			default :
			{
				[NSApp sendEvent:event];
				break;
			}
		}
	}
	[pool release];

	if (s_window->closed)
		state = -1;

	return state;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int mfb_update(void* buffer)
{
#if defined(USE_METAL_API)
	memcpy(s_window->draw_buffer, buffer, s_window->width * s_window->height * 4);
#else
	g_updateBuffer = buffer;
#endif
	int state = updateEvents();
	[[s_window contentView] setNeedsDisplay:YES];
	return state;
}
