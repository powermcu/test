#ifndef COCOA_APPLE_PLATFORM_H
#define COCOA_APPLE_PLATFORM_H

#if defined(HAVE_COCOA_METAL) || defined(HAVE_COCOATOUCH)

#ifdef HAVE_COCOA_METAL
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#endif

#if !defined(HAVE_COCOATOUCH)
@interface WindowListener : NSResponder<NSWindowDelegate>
@end

@implementation WindowListener

/* Similarly to SDL, we'll respond to key events by doing nothing so we don't beep.
 */
- (void)flagsChanged:(NSEvent *)event {
}

- (void)keyDown:(NSEvent *)event {
}

- (void)keyUp:(NSEvent *)event {
}

@end
#endif

@protocol ApplePlatform

/*! @brief renderView returns the current render view based on the viewType */
@property(readonly) id renderView;

/*! @brief isActive returns true if the application has focus */
@property(readonly) bool hasFocus;

@property(readwrite) apple_view_type_t viewType;

/*! @brief setVideoMode adjusts the video display to the specified mode */
- (void)setVideoMode:(gfx_ctx_mode_t)mode;

/*! @brief setCursorVisible specifies whether the cursor is visible */
- (void)setCursorVisible:(bool)v;

/*! @brief controls whether the screen saver should be disabled and
 * the displays should not sleep.
 */
- (bool)setDisableDisplaySleep:(bool)disable;
@end

extern id<ApplePlatform> apple_platform;

id<ApplePlatform> apple_platform;

#if defined(HAVE_COCOATOUCH)
@interface RetroArch_iOS : UINavigationController<ApplePlatform, UIApplicationDelegate,
UINavigationControllerDelegate> {
    UIView *_renderView;
    apple_view_type_t _vt;
}

@property (nonatomic) UIWindow* window;
@property (nonatomic) NSString* documentsDirectory;
@property (nonatomic) RAMenuBase* mainmenu;
@property (nonatomic) int menu_count;

+ (RetroArch_iOS*)get;

- (void)showGameView;
- (void)toggleUI;
- (void)supportOtherAudioSessions;

- (void)refreshSystemConfig;
- (void)mainMenuPushPop: (bool)pushp;
- (void)mainMenuRefresh;
@end

#else
@interface RetroArch_OSX : NSObject<ApplePlatform, NSApplicationDelegate> {
	NSWindow *_window;
	apple_view_type_t _vt;
	NSView *_renderView;
	id _sleepActivity;
	WindowListener *_listener;
}
#endif

#elif defined(HAVE_COCOA)
id apple_platform;
#if (defined(__MACH__) && (defined(__ppc__) || defined(__ppc64__)))
@interface RetroArch_OSX : NSObject
#else
@interface RetroArch_OSX : NSObject<NSApplicationDelegate>
#endif
{
	NSWindow *_window;
}
#endif

#if TARGET_OS_OSX
@property(nonatomic, retain) NSWindow IBOutlet *window;

@end
#endif

#endif
