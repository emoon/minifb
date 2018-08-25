
#include "OSXWindow.h"
#include <Cocoa/Cocoa.h>
#include <unistd.h>
#include "MiniFB.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void* g_updateBuffer = 0;
int g_width = 0;
int g_height = 0;
static OSXWindow* window_;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int mfb_open(const char* name, int width, int height)
{
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

	g_width = width;
	g_height = height;

	[NSApplication sharedApplication];
	[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

	NSWindowStyleMask styles = NSWindowStyleMaskResizable | NSWindowStyleMaskClosable | NSWindowStyleMaskTitled;

	NSRect rectangle = NSMakeRect(0, 0, width, height);
	window_ = [[OSXWindow alloc] initWithContentRect:rectangle styleMask:styles backing:NSBackingStoreBuffered defer:NO];

	if (!window_)
		return 0;

	[window_ setTitle:[NSString stringWithUTF8String:name]];
	[window_ setReleasedWhenClosed:NO];
	[window_ performSelectorOnMainThread:@selector(makeKeyAndOrderFront:) withObject:nil waitUntilDone:YES];

	[window_ center];

	[NSApp activateIgnoringOtherApps:YES];

	[pool drain];

	return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void mfb_close()
{
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

	if (window_)
		[window_ close]; 

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

	if (window_->closed)
		state = -1;

	return state;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int mfb_update(void* buffer)
{
	g_updateBuffer = buffer;
	int state = updateEvents();
	[[window_ contentView] setNeedsDisplay:YES];
	return state;
}
