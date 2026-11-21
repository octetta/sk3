#import <Cocoa/Cocoa.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api.h"
#include "skred.h"
#include "sk3_gui_common.h"

#define DEFAULT_WIDTH  640
#define DEFAULT_HEIGHT 580
#define COLLAPSED_HEIGHT 120

@interface Sk3AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate, NSTextFieldDelegate, NSDraggingDestination>
@property (strong, nonatomic) NSWindow *window;
@property (strong, nonatomic) NSTextField *statusLabel;
@property (strong, nonatomic) NSTextField *udpField;
@property (strong, nonatomic) NSTextField *tcpField;
@property (strong, nonatomic) NSButton *restartBtn;
@property (strong, nonatomic) NSButton *toggleBtn;
@property (strong, nonatomic) NSButton *themeBtn;
@property (strong, nonatomic) NSButton *fontBtn;

@property (strong, nonatomic) NSTextField *cmdLbl;
@property (strong, nonatomic) NSTextField *cmdInput;
@property (strong, nonatomic) NSButton *sendBtn;
@property (strong, nonatomic) NSButton *testBtn;
@property (strong, nonatomic) NSButton *silenceBtn;
@property (strong, nonatomic) NSButton *copyLogBtn;
@property (strong, nonatomic) NSButton *clearBtn;
@property (strong, nonatomic) NSScrollView *logScroll;
@property (strong, nonatomic) NSTextView *logView;

@property (strong, nonatomic) NSMutableArray<NSString *> *history;
@property (assign, nonatomic) NSInteger historyIdx;
@property (copy, nonatomic) NSString *currentDraft;

@property (assign, nonatomic) sk3_font_size_t curFontSize;
@property (assign, nonatomic) sk3_theme_t curTheme;
@property (assign, nonatomic) BOOL isCollapsed;
@property (assign, nonatomic) CGFloat prevExpandedH;
@end

@implementation Sk3AppDelegate

- (NSFont *)currentMonoFont {
    CGFloat pt = (self.curFontSize == SK3_FONT_SIZE_SMALL) ? 11.0 :
                 (self.curFontSize == SK3_FONT_SIZE_LARGE) ? 18.0 : 13.5;
    NSFont *font = [NSFont fontWithName:@"Menlo" size:pt];
    if (!font) font = [NSFont userFixedPitchFontOfSize:pt];
    if (!font) font = [NSFont monospacedSystemFontOfSize:pt weight:NSFontWeightRegular];
    return font;
}

- (void)applyTheme {
    NSColor *bg, *panel, *text, *textDim, *statusOk;
    switch (self.curTheme) {
        case SK3_THEME_AMBER:
            bg = [NSColor colorWithCalibratedRed:0.07 green:0.05 blue:0.02 alpha:1.0];
            panel = [NSColor colorWithCalibratedRed:0.11 green:0.08 blue:0.04 alpha:1.0];
            text = [NSColor colorWithCalibratedRed:1.0 green:0.69 blue:0.0 alpha:1.0];
            textDim = [NSColor colorWithCalibratedRed:0.7 green:0.45 blue:0.0 alpha:1.0];
            statusOk = [NSColor colorWithCalibratedRed:1.0 green:0.75 blue:0.1 alpha:1.0];
            break;
        case SK3_THEME_GREEN:
            bg = [NSColor colorWithCalibratedRed:0.03 green:0.07 blue:0.03 alpha:1.0];
            panel = [NSColor colorWithCalibratedRed:0.05 green:0.12 blue:0.05 alpha:1.0];
            text = [NSColor colorWithCalibratedRed:0.22 green:1.0 blue:0.08 alpha:1.0];
            textDim = [NSColor colorWithCalibratedRed:0.11 green:0.55 blue:0.04 alpha:1.0];
            statusOk = [NSColor colorWithCalibratedRed:0.0 green:1.0 blue:0.4 alpha:1.0];
            break;
        case SK3_THEME_LIGHT:
            bg = [NSColor colorWithCalibratedRed:0.95 green:0.95 blue:0.96 alpha:1.0];
            panel = [NSColor whiteColor];
            text = [NSColor colorWithCalibratedRed:0.1 green:0.1 blue:0.1 alpha:1.0];
            textDim = [NSColor colorWithCalibratedRed:0.4 green:0.4 blue:0.4 alpha:1.0];
            statusOk = [NSColor colorWithCalibratedRed:0.0 green:0.55 blue:0.12 alpha:1.0];
            break;
        case SK3_THEME_DARK:
        default:
            bg = [NSColor colorWithCalibratedRed:0.10 green:0.10 blue:0.11 alpha:1.0];
            panel = [NSColor colorWithCalibratedRed:0.14 green:0.14 blue:0.16 alpha:1.0];
            text = [NSColor colorWithCalibratedRed:0.9 green:0.9 blue:0.9 alpha:1.0];
            textDim = [NSColor colorWithCalibratedRed:0.55 green:0.55 blue:0.58 alpha:1.0];
            statusOk = [NSColor colorWithCalibratedRed:0.22 green:1.0 blue:0.08 alpha:1.0];
            break;
    }

    self.window.backgroundColor = bg;
    self.statusLabel.textColor = statusOk;
    self.cmdLbl.textColor = textDim;

    self.cmdInput.backgroundColor = panel;
    self.cmdInput.textColor = text;
    self.cmdInput.font = [self currentMonoFont];

    self.logView.backgroundColor = panel;
    self.logView.textColor = text;
    self.logView.font = [self currentMonoFont];

    self.themeBtn.title = [NSString stringWithUTF8String:sk3_theme_name(self.curTheme)];
    self.fontBtn.title = [NSString stringWithUTF8String:sk3_font_size_name(self.curFontSize)];
}

- (void)appendLog:(NSString *)text {
    if (!text || text.length == 0) return;
    dispatch_async(dispatch_get_main_queue(), ^{
        NSAttributedString *attrStr = [[NSAttributedString alloc] initWithString:text attributes:@{
            NSFontAttributeName: [self currentMonoFont],
            NSForegroundColorAttributeName: self.logView.textColor ?: [NSColor labelColor]
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

    char cmd_copy[2048];
    snprintf(cmd_copy, sizeof(cmd_copy), "%s", [trimmed UTF8String]);
    int r = skred_command(cmd_copy);
    char *log = skred_log();
    if (log && strlen(log) > 0) {
        [self appendLog:[NSString stringWithUTF8String:log]];
    }
    if (r > 0) {
        [self appendLog:[NSString stringWithFormat:@"r = %d\n", r]];
    }

    self.cmdInput.stringValue = @"";
}

- (void)toggleConsole:(id)sender {
    self.isCollapsed = !self.isCollapsed;
    NSRect frame = self.window.frame;
    if (self.isCollapsed) {
        self.prevExpandedH = frame.size.height;
        CGFloat diff = frame.size.height - COLLAPSED_HEIGHT;
        frame.origin.y += diff;
        frame.size.height = COLLAPSED_HEIGHT;
        self.toggleBtn.title = @"Console ▼";
        self.cmdLbl.hidden = YES;
        self.cmdInput.hidden = YES;
        self.sendBtn.hidden = YES;
        self.testBtn.hidden = YES;
        self.silenceBtn.hidden = YES;
        self.copyLogBtn.hidden = YES;
        self.clearBtn.hidden = YES;
        self.logScroll.hidden = YES;
    } else {
        CGFloat targetH = (self.prevExpandedH > COLLAPSED_HEIGHT + 100) ? self.prevExpandedH : DEFAULT_HEIGHT;
        CGFloat diff = targetH - frame.size.height;
        frame.origin.y -= diff;
        frame.size.height = targetH;
        self.toggleBtn.title = @"Console ▲";
        self.cmdLbl.hidden = NO;
        self.cmdInput.hidden = NO;
        self.sendBtn.hidden = NO;
        self.testBtn.hidden = NO;
        self.silenceBtn.hidden = NO;
        self.copyLogBtn.hidden = NO;
        self.clearBtn.hidden = NO;
        self.logScroll.hidden = NO;
    }
    [self.window setFrame:frame display:YES animate:YES];
}

- (void)cycleTheme:(id)sender {
    self.curTheme = (self.curTheme + 1) % SK3_THEME_COUNT;
    [self applyTheme];
}

- (void)cycleFontSize:(id)sender {
    self.curFontSize = (self.curFontSize + 1) % SK3_FONT_SIZE_COUNT;
    [self applyTheme];
}

- (void)copyLog:(id)sender {
    NSString *logStr = self.logView.string;
    if (logStr && logStr.length > 0) {
        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        [pb clearContents];
        [pb setString:logStr forType:NSPasteboardTypeString];
        [self appendLog:@"\n[Log copied to clipboard]\n"];
    }
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

#pragma mark - NSDraggingDestination (File Drop)

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
    NSPasteboard *pb = [sender draggingPasteboard];
    if ([pb canReadObjectForClasses:@[[NSURL class]] options:nil]) {
        return NSDragOperationCopy;
    }
    return NSDragOperationNone;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    NSPasteboard *pb = [sender draggingPasteboard];
    NSArray *urls = [pb readObjectsForClasses:@[[NSURL class]] options:nil];
    if (urls.count > 0) {
        NSURL *url = urls[0];
        NSString *path = [url path];
        if (path) {
            char log_msg[512] = {0};
            char cmd[2048] = {0};
            sk3_gui_format_file_drop([path UTF8String], log_msg, sizeof(log_msg), cmd, sizeof(cmd));
            if (log_msg[0]) [self appendLog:[NSString stringWithUTF8String:log_msg]];
            if (cmd[0]) {
                self.cmdInput.stringValue = [NSString stringWithUTF8String:cmd];
                [self.window makeFirstResponder:self.cmdInput];
            }
            return YES;
        }
    }
    return NO;
}

#pragma mark - NSTextFieldDelegate (Enter & History)

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
    self.curFontSize = SK3_FONT_SIZE_MEDIUM;
    self.curTheme = SK3_THEME_DARK;
    self.isCollapsed = NO;
    self.prevExpandedH = DEFAULT_HEIGHT;

    // Create Main Window
    NSRect frame = NSMakeRect(0, 0, DEFAULT_WIDTH, DEFAULT_HEIGHT);
    NSUInteger styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                           NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:styleMask
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    self.window.title = @"sk3 - Audio Engine & Control Server";
    self.window.minSize = NSMakeSize(560, COLLAPSED_HEIGHT);
    [self.window center];
    self.window.delegate = self;

    [self.window registerForDraggedTypes:@[NSPasteboardTypeFileURL]];

    NSView *content = self.window.contentView;
    content.autoresizesSubviews = YES;

    // Status Label
    self.statusLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 542, 600, 24)];
    self.statusLabel.editable = NO;
    self.statusLabel.bezeled = NO;
    self.statusLabel.drawsBackground = NO;
    self.statusLabel.font = [NSFont boldSystemFontOfSize:14.0];
    self.statusLabel.stringValue = @"Starting audio engine...";
    self.statusLabel.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    [content addSubview:self.statusLabel];

    // Ports Row: UDP
    NSTextField *udpLbl = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 508, 40, 22)];
    udpLbl.editable = NO;
    udpLbl.bezeled = NO;
    udpLbl.drawsBackground = NO;
    udpLbl.stringValue = @"UDP:";
    udpLbl.font = [NSFont systemFontOfSize:13.0];
    udpLbl.autoresizingMask = NSViewMinYMargin;
    [content addSubview:udpLbl];

    self.udpField = [[NSTextField alloc] initWithFrame:NSMakeRect(60, 506, 60, 24)];
    self.udpField.stringValue = @"60440";
    self.udpField.font = [self currentMonoFont];
    self.udpField.autoresizingMask = NSViewMinYMargin;
    [content addSubview:self.udpField];

    // TCP
    NSTextField *tcpLbl = [[NSTextField alloc] initWithFrame:NSMakeRect(130, 508, 40, 22)];
    tcpLbl.editable = NO;
    tcpLbl.bezeled = NO;
    tcpLbl.drawsBackground = NO;
    tcpLbl.stringValue = @"TCP:";
    tcpLbl.font = [NSFont systemFontOfSize:13.0];
    tcpLbl.autoresizingMask = NSViewMinYMargin;
    [content addSubview:tcpLbl];

    self.tcpField = [[NSTextField alloc] initWithFrame:NSMakeRect(170, 506, 60, 24)];
    self.tcpField.stringValue = @"60440";
    self.tcpField.font = [self currentMonoFont];
    self.tcpField.autoresizingMask = NSViewMinYMargin;
    [content addSubview:self.tcpField];

    // Restart Button
    self.restartBtn = [NSButton buttonWithTitle:@"Restart" target:self action:@selector(restartClicked:)];
    self.restartBtn.frame = NSMakeRect(238, 504, 75, 28);
    self.restartBtn.bezelStyle = NSBezelStyleRounded;
    self.restartBtn.autoresizingMask = NSViewMinYMargin;
    [content addSubview:self.restartBtn];

    // Console Toggle Button
    self.toggleBtn = [NSButton buttonWithTitle:@"Console ▲" target:self action:@selector(toggleConsole:)];
    self.toggleBtn.frame = NSMakeRect(318, 504, 95, 28);
    self.toggleBtn.bezelStyle = NSBezelStyleRounded;
    self.toggleBtn.autoresizingMask = NSViewMinYMargin;
    [content addSubview:self.toggleBtn];

    // Theme Button
    self.themeBtn = [NSButton buttonWithTitle:@"Theme: Dark" target:self action:@selector(cycleTheme:)];
    self.themeBtn.frame = NSMakeRect(418, 504, 110, 28);
    self.themeBtn.bezelStyle = NSBezelStyleRounded;
    self.themeBtn.autoresizingMask = NSViewMinYMargin;
    [content addSubview:self.themeBtn];

    // Font Button
    self.fontBtn = [NSButton buttonWithTitle:@"Font: M" target:self action:@selector(cycleFontSize:)];
    self.fontBtn.frame = NSMakeRect(533, 504, 85, 28);
    self.fontBtn.bezelStyle = NSBezelStyleRounded;
    self.fontBtn.autoresizingMask = NSViewMinYMargin;
    [content addSubview:self.fontBtn];

    // Command Input Label
    self.cmdLbl = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 468, 600, 20)];
    self.cmdLbl.editable = NO;
    self.cmdLbl.bezeled = NO;
    self.cmdLbl.drawsBackground = NO;
    self.cmdLbl.stringValue = @"Skode Command (Enter to run, Up/Down history):";
    self.cmdLbl.font = [NSFont systemFontOfSize:12.0];
    self.cmdLbl.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    [content addSubview:self.cmdLbl];

    // Command Input Field (Monospaced!)
    self.cmdInput = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 436, 500, 28)];
    self.cmdInput.placeholderString = @"e.g. v0 w1 f440 a-6 l1; or (120 4 (b-1 c0))";
    self.cmdInput.font = [self currentMonoFont];
    self.cmdInput.delegate = self;
    self.cmdInput.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    [content addSubview:self.cmdInput];

    // Send Button
    self.sendBtn = [NSButton buttonWithTitle:@"Send" target:self action:@selector(sendClicked:)];
    self.sendBtn.frame = NSMakeRect(530, 435, 90, 30);
    self.sendBtn.bezelStyle = NSBezelStyleRounded;
    self.sendBtn.autoresizingMask = NSViewMinXMargin | NSViewMinYMargin;
    [content addSubview:self.sendBtn];

    // Quick Action Buttons
    self.testBtn = [NSButton buttonWithTitle:@"Test Tone" target:self action:@selector(testClicked:)];
    self.testBtn.frame = NSMakeRect(20, 396, 95, 28);
    self.testBtn.bezelStyle = NSBezelStyleRounded;
    self.testBtn.autoresizingMask = NSViewMinYMargin;
    [content addSubview:self.testBtn];

    self.silenceBtn = [NSButton buttonWithTitle:@"Silence (O Z0)" target:self action:@selector(silenceClicked:)];
    self.silenceBtn.frame = NSMakeRect(120, 396, 125, 28);
    self.silenceBtn.bezelStyle = NSBezelStyleRounded;
    self.silenceBtn.autoresizingMask = NSViewMinYMargin;
    [content addSubview:self.silenceBtn];

    self.copyLogBtn = [NSButton buttonWithTitle:@"Copy Log" target:self action:@selector(copyLog:)];
    self.copyLogBtn.frame = NSMakeRect(430, 396, 95, 28);
    self.copyLogBtn.bezelStyle = NSBezelStyleRounded;
    self.copyLogBtn.autoresizingMask = NSViewMinXMargin | NSViewMinYMargin;
    [content addSubview:self.copyLogBtn];

    self.clearBtn = [NSButton buttonWithTitle:@"Clear Log" target:self action:@selector(clearClicked:)];
    self.clearBtn.frame = NSMakeRect(530, 396, 90, 28);
    self.clearBtn.bezelStyle = NSBezelStyleRounded;
    self.clearBtn.autoresizingMask = NSViewMinXMargin | NSViewMinYMargin;
    [content addSubview:self.clearBtn];

    // Log ScrollView & TextView (Monospaced!)
    self.logScroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 20, 600, 366)];
    self.logScroll.hasVerticalScroller = YES;
    self.logScroll.hasHorizontalScroller = NO;
    self.logScroll.borderType = NSBezelBorder;
    self.logScroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    NSSize contentSize = [self.logScroll contentSize];
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
    self.logView.font = [self currentMonoFont];

    self.logScroll.documentView = self.logView;
    [content addSubview:self.logScroll];

    [self applyTheme];

    // Set dock icon
    NSString *iconPath = [[NSBundle mainBundle] pathForResource:@"osko" ofType:@"icns"];
    if (iconPath) {
        NSImage *icon = [[NSImage alloc] initWithContentsOfFile:iconPath];
        if (icon) [NSApp setApplicationIconImage:icon];
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

    // Edit menu
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
