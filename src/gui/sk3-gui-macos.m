#import <Cocoa/Cocoa.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api.h"
#include "skred.h"

@interface Sk3AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate, NSTextFieldDelegate>
@property (strong, nonatomic) NSWindow *window;
@property (strong, nonatomic) NSTextField *statusLabel;
@property (strong, nonatomic) NSTextField *udpField;
@property (strong, nonatomic) NSTextField *tcpField;
@property (strong, nonatomic) NSButton *restartBtn;
@property (strong, nonatomic) NSTextField *cmdInput;
@property (strong, nonatomic) NSButton *sendBtn;
@property (strong, nonatomic) NSButton *testBtn;
@property (strong, nonatomic) NSButton *silenceBtn;
@property (strong, nonatomic) NSButton *clearBtn;
@property (strong, nonatomic) NSTextView *logView;
@property (strong, nonatomic) NSMutableArray<NSString *> *history;
@property (assign, nonatomic) NSInteger historyIdx;
@property (copy, nonatomic) NSString *currentDraft;
@end

@implementation Sk3AppDelegate

- (void)appendLog:(NSString *)text {
    if (!text || text.length == 0) return;
    dispatch_async(dispatch_get_main_queue(), ^{
        NSAttributedString *attrStr = [[NSAttributedString alloc] initWithString:text attributes:@{
            NSFontAttributeName: [NSFont userFixedPitchFontOfSize:12.0] ?: [NSFont monospacedSystemFontOfSize:12.0 weight:NSFontWeightRegular],
            NSForegroundColorAttributeName: [NSColor labelColor]
        }];
        [[self.logView textStorage] appendAttributedString:attrStr];
        [self.logView scrollRangeToVisible:NSMakeRange(self.logView.string.length, 0)];
    });
}

- (void)startEngine {
    int udpPort = [self.udpField.stringValue intValue];
    int tcpPort = [self.tcpField.stringValue intValue];
    if (udpPort <= 0) udpPort = 60440;
    if (tcpPort <= 0) tcpPort = 60440;

    skred_stop();
    skred_set_audio_device(-1, -1);
    int r = skred_start(128, 256, udpPort);
    int hp = 0;
    if (tcpPort > 0) {
        hp = skred_http_start(tcpPort);
    }
    skred_logger(1);

    if (r == 0) {
        self.statusLabel.stringValue = [NSString stringWithFormat:@"● RUNNING: sk3 v%s (UDP: %d, TCP: %d)",
                                         skred_version(), udpPort, hp > 0 ? hp : tcpPort];
        self.statusLabel.textColor = [NSColor colorWithCalibratedRed:0.0 green:0.55 blue:0.1 alpha:1.0];

        NSString *banner = [NSString stringWithFormat:
            @"=== sk3 v%s Audio Engine ===\n"
            "Status: Active (256 voices, 48kHz audio)\n"
            "UDP Port: %d (Opcode dispatch & datagrams)\n"
            "TCP Port: %d (Raw TCP, HTTP/1.1, WebSocket)\n"
            "Engine ready for Skode commands.\n",
            skred_version(), udpPort, hp > 0 ? hp : tcpPort];
        [self appendLog:banner];
    } else {
        self.statusLabel.stringValue = [NSString stringWithFormat:@"○ AUDIO START FAILED (Error %d)", r];
        self.statusLabel.textColor = [NSColor systemRedColor];
        [self appendLog:@"Error: Failed to initialize audio output device.\n"];
    }
}

- (void)executeCommand:(NSString *)rawCmd {
    if (!rawCmd) return;
    NSString *trimmed = [rawCmd stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (trimmed.length == 0) return;

    if (self.history.count == 0 || ![self.history.lastObject isEqualToString:trimmed]) {
        [self.history addObject:trimmed];
        if (self.history.count > 128) {
            [self.history removeObjectAtIndex:0];
        }
    }
    self.historyIdx = -1;
    self.currentDraft = @"";

    [self appendLog:[NSString stringWithFormat:@"\n> %@\n", trimmed]];

    int r = skred_command([trimmed UTF8String]);
    char *log = skred_log();
    if (log && strlen(log) > 0) {
        [self appendLog:[NSString stringWithUTF8String:log]];
    }
    if (r > 0) {
        [self appendLog:[NSString stringWithFormat:@"r = %d\n", r]];
    }

    self.cmdInput.stringValue = @"";
}

- (void)sendClicked:(id)sender {
    [self executeCommand:self.cmdInput.stringValue];
    [self.window makeFirstResponder:self.cmdInput];
}

- (void)restartClicked:(id)sender {
    [self startEngine];
    [self.window makeFirstResponder:self.cmdInput];
}

- (void)testClicked:(id)sender {
    [self executeCommand:@"v0 w1 f440 a-6 l1;"];
    [self.window makeFirstResponder:self.cmdInput];
}

- (void)silenceClicked:(id)sender {
    [self executeCommand:@"O Z0; S-1;"];
    [self.window makeFirstResponder:self.cmdInput];
}

- (void)clearClicked:(id)sender {
    self.logView.string = @"";
    [self.window makeFirstResponder:self.cmdInput];
}

#pragma mark - NSTextFieldDelegate (Enter & History Navigation)

- (BOOL)control:(NSControl *)control textView:(NSTextView *)textView doCommandBySelector:(SEL)commandSelector {
    if (control == self.cmdInput) {
        if (commandSelector == @selector(insertNewline:)) {
            [self executeCommand:self.cmdInput.stringValue];
            return YES;
        } else if (commandSelector == @selector(moveUp:)) {
            if (self.history.count > 0) {
                if (self.historyIdx == -1) {
                    self.currentDraft = self.cmdInput.stringValue;
                    self.historyIdx = (NSInteger)self.history.count - 1;
                } else if (self.historyIdx > 0) {
                    self.historyIdx--;
                }
                self.cmdInput.stringValue = self.history[self.historyIdx];
            }
            return YES;
        } else if (commandSelector == @selector(moveDown:)) {
            if (self.historyIdx != -1) {
                self.historyIdx++;
                if (self.historyIdx < (NSInteger)self.history.count) {
                    self.cmdInput.stringValue = self.history[self.historyIdx];
                } else {
                    self.historyIdx = -1;
                    self.cmdInput.stringValue = self.currentDraft ?: @"";
                }
            }
            return YES;
        }
    }
    return NO;
}

#pragma mark - App Lifecycle

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    self.history = [NSMutableArray array];
    self.historyIdx = -1;
    self.currentDraft = @"";

    // Create Main Window
    NSRect frame = NSMakeRect(0, 0, 620, 560);
    NSUInteger styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                           NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:styleMask
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    self.window.title = @"sk3 - Audio Engine & Control Server";
    self.window.minSize = NSMakeSize(540, 420);
    [self.window center];
    self.window.delegate = self;

    NSView *content = self.window.contentView;
    content.autoresizesSubviews = YES;

    // Header: Status Label
    self.statusLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 516, 580, 24)];
    self.statusLabel.editable = NO;
    self.statusLabel.bezeled = NO;
    self.statusLabel.drawsBackground = NO;
    self.statusLabel.font = [NSFont boldSystemFontOfSize:14.0];
    self.statusLabel.stringValue = @"Starting audio engine...";
    self.statusLabel.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    [content addSubview:self.statusLabel];

    // Ports Config Row: UDP Port Label & Field
    NSTextField *udpLbl = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 480, 70, 22)];
    udpLbl.editable = NO;
    udpLbl.bezeled = NO;
    udpLbl.drawsBackground = NO;
    udpLbl.stringValue = @"UDP Port:";
    udpLbl.font = [NSFont systemFontOfSize:13.0];
    udpLbl.autoresizingMask = NSViewMinYMargin;
    [content addSubview:udpLbl];

    self.udpField = [[NSTextField alloc] initWithFrame:NSMakeRect(95, 478, 70, 24)];
    self.udpField.stringValue = @"60440";
    self.udpField.font = [NSFont systemFontOfSize:13.0];
    self.udpField.autoresizingMask = NSViewMinYMargin;
    [content addSubview:self.udpField];

    // TCP Port Label & Field
    NSTextField *tcpLbl = [[NSTextField alloc] initWithFrame:NSMakeRect(180, 480, 70, 22)];
    tcpLbl.editable = NO;
    tcpLbl.bezeled = NO;
    tcpLbl.drawsBackground = NO;
    tcpLbl.stringValue = @"TCP Port:";
    tcpLbl.font = [NSFont systemFontOfSize:13.0];
    tcpLbl.autoresizingMask = NSViewMinYMargin;
    [content addSubview:tcpLbl];

    self.tcpField = [[NSTextField alloc] initWithFrame:NSMakeRect(255, 478, 70, 24)];
    self.tcpField.stringValue = @"60440";
    self.tcpField.font = [NSFont systemFontOfSize:13.0];
    self.tcpField.autoresizingMask = NSViewMinYMargin;
    [content addSubview:self.tcpField];

    // Restart Button
    self.restartBtn = [NSButton buttonWithTitle:@"Restart Server" target:self action:@selector(restartClicked:)];
    self.restartBtn.frame = NSMakeRect(345, 476, 125, 28);
    self.restartBtn.bezelStyle = NSBezelStyleRounded;
    self.restartBtn.autoresizingMask = NSViewMinYMargin;
    [content addSubview:self.restartBtn];

    // Command Input Label
    NSTextField *cmdLbl = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 442, 580, 20)];
    cmdLbl.editable = NO;
    cmdLbl.bezeled = NO;
    cmdLbl.drawsBackground = NO;
    cmdLbl.stringValue = @"Skode Command (Enter to execute, Up/Down for history):";
    cmdLbl.font = [NSFont systemFontOfSize:12.0];
    cmdLbl.textColor = [NSColor secondaryLabelColor];
    cmdLbl.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    [content addSubview:cmdLbl];

    // Command Input Field
    self.cmdInput = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 412, 480, 26)];
    self.cmdInput.placeholderString = @"e.g. v0 w1 f440 a-6 l1; or (120 4 (b-1 c0))";
    self.cmdInput.font = [NSFont systemFontOfSize:13.0];
    self.cmdInput.delegate = self;
    self.cmdInput.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    [content addSubview:self.cmdInput];

    // Send Button
    self.sendBtn = [NSButton buttonWithTitle:@"Send" target:self action:@selector(sendClicked:)];
    self.sendBtn.frame = NSMakeRect(510, 410, 90, 28);
    self.sendBtn.bezelStyle = NSBezelStyleRounded;
    self.sendBtn.autoresizingMask = NSViewMinXMargin | NSViewMinYMargin;
    [content addSubview:self.sendBtn];

    // Quick Action Buttons Row
    self.testBtn = [NSButton buttonWithTitle:@"Test Tone" target:self action:@selector(testClicked:)];
    self.testBtn.frame = NSMakeRect(20, 374, 100, 26);
    self.testBtn.bezelStyle = NSBezelStyleRounded;
    self.testBtn.autoresizingMask = NSViewMinYMargin;
    [content addSubview:self.testBtn];

    self.silenceBtn = [NSButton buttonWithTitle:@"Silence (O Z0)" target:self action:@selector(silenceClicked:)];
    self.silenceBtn.frame = NSMakeRect(130, 374, 130, 26);
    self.silenceBtn.bezelStyle = NSBezelStyleRounded;
    self.silenceBtn.autoresizingMask = NSViewMinYMargin;
    [content addSubview:self.silenceBtn];

    self.clearBtn = [NSButton buttonWithTitle:@"Clear Log" target:self action:@selector(clearClicked:)];
    self.clearBtn.frame = NSMakeRect(510, 374, 90, 26);
    self.clearBtn.bezelStyle = NSBezelStyleRounded;
    self.clearBtn.autoresizingMask = NSViewMinXMargin | NSViewMinYMargin;
    [content addSubview:self.clearBtn];

    // Monospace Output Log ScrollView & TextView
    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 20, 580, 344)];
    scroll.hasVerticalScroller = YES;
    scroll.hasHorizontalScroller = NO;
    scroll.borderType = NSBezelBorder;
    scroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    NSSize contentSize = [scroll contentSize];
    self.logView = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, contentSize.width, contentSize.height)];
    self.logView.minSize = NSMakeSize(0.0, contentSize.height);
    self.logView.maxSize = NSMakeSize(FLT_MAX, FLT_MAX);
    self.logView.verticallyResizable = YES;
    self.logView.horizontallyResizable = NO;
    self.logView.autoresizingMask = NSViewWidthSizable;
    self.logView.textContainer.containerSize = NSMakeSize(contentSize.width, FLT_MAX);
    self.logView.textContainer.widthTracksTextView = YES;
    self.logView.editable = NO;
    self.logView.selectable = YES;
    self.logView.font = [NSFont userFixedPitchFontOfSize:12.0] ?: [NSFont monospacedSystemFontOfSize:12.0 weight:NSFontWeightRegular];

    scroll.documentView = self.logView;
    [content addSubview:scroll];

    // Set dock icon if available
    NSString *iconPath = [[NSBundle mainBundle] pathForResource:@"osko" ofType:@"icns"];
    if (iconPath) {
        NSImage *icon = [[NSImage alloc] initWithContentsOfFile:iconPath];
        if (icon) {
            [NSApp setApplicationIconImage:icon];
        }
    }

    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
    [self.window makeFirstResponder:self.cmdInput];

    [self startEngine];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

- (void)applicationWillTerminate:(NSNotification *)notification {
    skred_stop();
}

@end

static void setupMainMenu(void) {
    NSMenu *mainMenu = [[NSMenu alloc] init];

    // App menu
    NSMenuItem *appMenuItem = [[NSMenuItem alloc] init];
    [mainMenu addItem:appMenuItem];
    NSMenu *appMenu = [[NSMenu alloc] initWithTitle:@"sk3"];
    [appMenu addItemWithTitle:@"About sk3" action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:@"Hide sk3" action:@selector(hide:) keyEquivalent:@"h"];
    NSMenuItem *hideOthers = [appMenu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
    [hideOthers setKeyEquivalentModifierMask:(NSEventModifierFlagOption | NSEventModifierFlagCommand)];
    [appMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:@"Quit sk3" action:@selector(terminate:) keyEquivalent:@"q"];
    [appMenuItem setSubmenu:appMenu];

    // Edit menu (required for Cmd+C, Cmd+V, Cmd+X, Cmd+A)
    NSMenuItem *editMenuItem = [[NSMenuItem alloc] init];
    [mainMenu addItem:editMenuItem];
    NSMenu *editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
    [editMenu addItemWithTitle:@"Undo" action:@selector(undo:) keyEquivalent:@"z"];
    [editMenu addItemWithTitle:@"Redo" action:@selector(redo:) keyEquivalent:@"Z"];
    [editMenu addItem:[NSMenuItem separatorItem]];
    [editMenu addItemWithTitle:@"Cut" action:@selector(cut:) keyEquivalent:@"x"];
    [editMenu addItemWithTitle:@"Copy" action:@selector(copy:) keyEquivalent:@"c"];
    [editMenu addItemWithTitle:@"Paste" action:@selector(paste:) keyEquivalent:@"v"];
    [editMenu addItemWithTitle:@"Select All" action:@selector(selectAll:) keyEquivalent:@"a"];
    [editMenuItem setSubmenu:editMenu];

    // Window menu
    NSMenuItem *windowMenuItem = [[NSMenuItem alloc] init];
    [mainMenu addItem:windowMenuItem];
    NSMenu *windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
    [windowMenu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
    [windowMenu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];
    [windowMenuItem setSubmenu:windowMenu];

    [NSApp setMainMenu:mainMenu];
}

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        setupMainMenu();

        Sk3AppDelegate *delegate = [[Sk3AppDelegate alloc] init];
        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}
