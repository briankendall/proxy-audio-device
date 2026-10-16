#include <vector>
#include <CoreAudio/CoreAudio.h>
#import "WindowDelegate.h"
#include "AudioDevice.h"
#include "ProxyAudioDevice.h"

int onDevicesChanged(AudioObjectID inObjectID,
                     UInt32 inNumberAddresses,
                     const AudioObjectPropertyAddress *inAddresses,
                     void *inClientData);

// Persisted in the settings app's own preferences: a UID -> display name map so
// we can show a friendly name for devices that are currently disconnected (the
// driver only stores UIDs; CoreAudio can't name a device that isn't present).
static NSString *const kNameCacheKey = @"deviceNameCacheByUID";

// Column identifiers for the priority table.
static NSString *const kDeviceColumnID = @"device";
static NSString *const kStatusColumnID = @"status";

// --- Programmatic layout constants (tweak here to adjust the priority list UI) ---
// Extra height added to the window to make room for the priority list, in points.
static const CGFloat kPriorityUIExtraHeight = 150.0;
static const CGFloat kPriorityListLeftX = 158.0;
static const CGFloat kPriorityListWidth = 336.0;

@implementation WindowDelegate {
    NSMutableArray<NSString *> *priorityUIDs;
    NSDictionary<NSString *, NSString *> *connectedNamesByUID;
    NSString *activeUID;
    bool priorityUIBuilt;
    int initializationAttemptInterval;
    NSTimer *refreshTimer;
    NSString *lastDisplaySignature;
}

- (void)awakeFromNib {
    self.deviceNameTextField.stringValue = NSLocalizedString(@"< Loading... >", nil);
    self.deviceNameTextField.enabled = NO;
    self.outputDeviceComboBox.enabled = NO;
    self.bufferSizeComboBox.enabled = NO;
    self.proxiedDeviceIsActiveRadioButton.enabled = NO;
    self.userIsActiveRadioButton.enabled = NO;
    self.alwaysRadioButton.enabled = NO;
    self.hideWhenUnavailableCheckbox.enabled = NO;
    priorityUIDs = [NSMutableArray array];
    connectedNamesByUID = @{};
    priorityUIBuilt = false;
    initializationAttemptInterval = 3;
    [self keepTryingToInitializeUntilSuccess];
}

- (void)keepTryingToInitializeUntilSuccess {
    // For some reason, sometimes when the app launches right after the system boots we'll get bunk data
    // for all of the connected audio devices. If that happens then we'll try to initialize again after
    // a few seconds. We're initially waiting three seconds because that seems to work. Using less time
    // can actually cause the audio server to crash, so we want to be careful not to query it too often!
    bool success = [self initialize];

    if (!success) {
        NSLog(@"NB: failed to initialize, will try again in a sec...");
        [NSTimer scheduledTimerWithTimeInterval:initializationAttemptInterval target:self selector:@selector(keepTryingToInitializeUntilSuccess) userInfo:nil repeats:NO];
        // Increase the length of time between attempting to initialize by two seconds each time, just to be safe:
        initializationAttemptInterval += 2;
    }
}

- (bool)initialize {
    if (![self setCurrentProcessAsConfigurator]) {
        return false;
    }

    self.deviceNameTextField.stringValue = [self currentDeviceName];

    if (![self proxyAudioDeviceAvailable]) {
        // It's expected that we won't find the Proxy Audio Device if it's not installed, so this
        // technically isn't a failure case where we'd want to try initializing the app again.
        return true;
    }

    [self setupPriorityListUI];

    if (![self refreshPriorityList]) {
        return false;
    }

    if (![self setupListenerForCurrentAudioDevices]) {
        return false;
    }

    [self startRefreshTimer];

    [self.bufferSizeComboBox selectItemWithObjectValue:[self currentOutputDeviceBufferFrameSize]];
    self.deviceNameTextField.enabled = YES;
    self.bufferSizeComboBox.enabled = YES;
    self.proxiedDeviceIsActiveRadioButton.enabled = YES;
    self.userIsActiveRadioButton.enabled = YES;
    self.alwaysRadioButton.enabled = YES;
    self.hideWhenUnavailableCheckbox.enabled = YES;
    self.hideWhenUnavailableCheckbox.state =
        [self currentHideWhenUnavailable] ? NSControlStateValueOn : NSControlStateValueOff;
    self.autoFailbackCheckbox.state = [self currentAutoFailback] ? NSControlStateValueOn : NSControlStateValueOff;

    ProxyAudioDevice::ActiveCondition condition = [self currentOutputDeviceActiveCondition];

    if (condition == ProxyAudioDevice::ActiveCondition::proxiedDeviceActive) {
        self.proxiedDeviceIsActiveRadioButton.state = NSControlStateValueOn;
        self.userIsActiveRadioButton.state = NSControlStateValueOff;
        self.alwaysRadioButton.state = NSControlStateValueOff;
    } else if (condition == ProxyAudioDevice::ActiveCondition::userActive) {
        self.proxiedDeviceIsActiveRadioButton.state = NSControlStateValueOff;
        self.userIsActiveRadioButton.state = NSControlStateValueOn;
        self.alwaysRadioButton.state = NSControlStateValueOff;
    } else {
        self.proxiedDeviceIsActiveRadioButton.state = NSControlStateValueOff;
        self.userIsActiveRadioButton.state = NSControlStateValueOff;
        self.alwaysRadioButton.state = NSControlStateValueOn;
    }

    return true;
}

- (bool)setCurrentProcessAsConfigurator {
    AudioDeviceID proxyAudioBox = AudioDevice::audioDeviceIDForBoxUID(CFSTR(kBox_UID));

    if (proxyAudioBox == kAudioObjectUnknown) {
        NSLog(@"Error: unable to find proxy audio device");
        // It's expected that we won't find the Proxy Audio Device if it's not installed, so this
        // technically isn't a failure case where we'd want to try initializing the app again.
        return true;
    }

    if (!AudioDevice::setIdentifyValue(proxyAudioBox, getpid())) {
        NSLog(@"Error: unable to set current process as configurator");
        return false;
    }

    return true;
}

int onDevicesChanged(AudioObjectID inObjectID,
                     UInt32 inNumberAddresses,
                     const AudioObjectPropertyAddress *inAddresses,
                     void *inClientData) {
#pragma unused(inObjectID, inNumberAddresses, inAddresses)
    dispatch_async(dispatch_get_main_queue(), ^{
        WindowDelegate *delegate = (__bridge WindowDelegate *)inClientData;
        [delegate refreshPriorityList];
    });

    return noErr;
}

- (bool)setupListenerForCurrentAudioDevices {
    AudioObjectPropertyAddress listenerPropertyAddress = {
        kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};
    OSStatus err =
        AudioObjectAddPropertyListener(kAudioObjectSystemObject, &listenerPropertyAddress, &onDevicesChanged, (__bridge_retained void *)self);

    if (err != noErr) {
        NSLog(@"Error: could not set up listener for audio devices changing");
        return false;
    }

    return true;
}

- (bool)proxyAudioDeviceAvailable {
    return AudioDevice::audioDeviceIDForBoxUID(CFSTR(kBox_UID)) != kAudioObjectUnknown;
}

- (NSString *)currentDeviceName {
    AudioDeviceID proxyAudioBox = AudioDevice::audioDeviceIDForBoxUID(CFSTR(kBox_UID));
    AudioDevice::setIdentifyValue(proxyAudioBox, -((SInt32)ProxyAudioDevice::ConfigType::deviceName));
    NSString *result = (__bridge_transfer NSString *)AudioDevice::copyObjectName(proxyAudioBox);

    return result ? result : NSLocalizedString(@"< Proxy Audio Device not found >", nil);
}

- (IBAction)deviceNameEntered:(id)sender {
#pragma unused(sender)
    NSString *newName = [self.deviceNameTextField.stringValue
        stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];

    if (newName.length == 0) {
        self.deviceNameTextField.stringValue = [self currentDeviceName];
        return;
    }

    AudioDeviceID proxyAudioBox = AudioDevice::audioDeviceIDForBoxUID(CFSTR(kBox_UID));
    AudioDevice::setObjectName(proxyAudioBox,
                               (__bridge CFStringRef)[NSString stringWithFormat:@"deviceName=%@", newName]);
}

#pragma mark - Configuration channel helpers

// Reads a configuration value from the driver using the identify-value hack:
// set the box's identify property to the negative ConfigType, then read back its
// name property. Returns nil if nothing was returned.
- (NSString *)readConfigValueForType:(ProxyAudioDevice::ConfigType)type {
    AudioDeviceID proxyAudioBox = AudioDevice::audioDeviceIDForBoxUID(CFSTR(kBox_UID));
    AudioDevice::setIdentifyValue(proxyAudioBox, -((SInt32)type));
    return (__bridge_transfer NSString *)AudioDevice::copyObjectName(proxyAudioBox);
}

// Writes a "key=value" configuration string to the driver. __bridge (not
// __bridge_retained) is correct here: setObjectName only needs the string for
// the duration of the synchronous call, so handing it a +1 reference would leak.
- (void)writeConfigString:(NSString *)keyValue {
    AudioDeviceID proxyAudioBox = AudioDevice::audioDeviceIDForBoxUID(CFSTR(kBox_UID));
    AudioDevice::setObjectName(proxyAudioBox, (__bridge CFStringRef)keyValue);
}

#pragma mark - Priority list model

- (NSArray<NSString *> *)readPriorityUIDsFromDriver {
    NSString *blob = [self readConfigValueForType:ProxyAudioDevice::ConfigType::outputDevicePriorityList];

    if (blob.length == 0) {
        return @[];
    }

    NSMutableArray<NSString *> *result = [NSMutableArray array];

    for (NSString *uid in [blob componentsSeparatedByString:@"\n"]) {
        if (uid.length > 0) {
            [result addObject:uid];
        }
    }

    return result;
}

- (void)writePriorityUIDsToDriver {
    NSString *blob = [priorityUIDs componentsJoinedByString:@"\n"];
    [self writeConfigString:[NSString stringWithFormat:@"outputDevicePriorityList=%@", blob]];
}

- (NSString *)currentActiveOutputDeviceUID {
    NSString *uid = [self readConfigValueForType:ProxyAudioDevice::ConfigType::currentActiveOutputDevice];
    return uid.length > 0 ? uid : nil;
}

- (BOOL)currentAutoFailback {
    NSString *value = [self readConfigValueForType:ProxyAudioDevice::ConfigType::outputDeviceAutoFailback];
    return value.intValue != 0;
}

#pragma mark - Display name cache

- (NSString *)cachedNameForUID:(NSString *)uid {
    NSDictionary *cache = [[NSUserDefaults standardUserDefaults] dictionaryForKey:kNameCacheKey];
    return cache[uid];
}

- (void)cacheName:(NSString *)name forUID:(NSString *)uid {
    if (name.length == 0 || uid.length == 0) {
        return;
    }

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSMutableDictionary *cache = [([defaults dictionaryForKey:kNameCacheKey] ?: @{}) mutableCopy];

    if ([cache[uid] isEqualToString:name]) {
        return;
    }

    cache[uid] = name;
    [defaults setObject:cache forKey:kNameCacheKey];
}

- (NSString *)displayNameForUID:(NSString *)uid {
    NSString *liveName = connectedNamesByUID[uid];

    if (liveName.length > 0) {
        return liveName;
    }

    NSString *cachedName = [self cachedNameForUID:uid];

    if (cachedName.length > 0) {
        return cachedName;
    }

    return uid;
}

// Returns the UIDs and names of currently-connected output devices (excluding
// the proxy device), and refreshes the display-name cache as a side effect.
- (NSDictionary<NSString *, NSString *> *)connectedOutputDeviceNamesByUID {
    NSMutableDictionary<NSString *, NSString *> *result = [NSMutableDictionary dictionary];
    std::vector<AudioDeviceID> devices = AudioDevice::devicesWithOutputCapabilitiesThatAreNotProxyAudioDevice();

    for (AudioDeviceID device : devices) {
        NSString *uid = (__bridge_transfer NSString *)AudioDevice::copyDeviceUID(device);
        NSString *name = (__bridge_transfer NSString *)AudioDevice::copyObjectName(device);

        if (uid.length > 0 && name.length > 0) {
            result[uid] = name;
            [self cacheName:name forUID:uid];
        }
    }

    return result;
}

#pragma mark - Priority list refresh

// Some devices (USB/Bluetooth) stay enumerated but report not-alive on
// disconnect, which does not change kAudioHardwarePropertyDevices and so fires no
// device-list notification. A light poll keeps the status column and active
// marker honest regardless of which notifications the system sends.
- (void)startRefreshTimer {
    if (refreshTimer) {
        return;
    }

    refreshTimer = [NSTimer scheduledTimerWithTimeInterval:1.5
                                                    target:self
                                                  selector:@selector(pollRefresh)
                                                  userInfo:nil
                                                   repeats:YES];
}

- (void)pollRefresh {
    // Only do the work while the settings window is actually on screen.
    if (self.priorityTableView.window.isVisible) {
        [self refreshPriorityList];
    }
}

// Mirrors the driver's definition of "available": present in the system AND
// reporting itself as alive. Keeps the app's status column in agreement with the
// device the driver actually routes to.
- (BOOL)isUIDAvailable:(NSString *)uid {
    if (uid.length == 0) {
        return NO;
    }

    AudioDeviceID device = AudioDevice::audioDeviceIDForDeviceUID((__bridge CFStringRef)uid);

    if (device == kAudioObjectUnknown) {
        return NO;
    }

    AudioObjectPropertyAddress aliveAddress = {
        kAudioDevicePropertyDeviceIsAlive, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};
    UInt32 alive = 0;
    UInt32 size = sizeof(alive);
    OSStatus err = AudioObjectGetPropertyData(device, &aliveAddress, 0, NULL, &size, &alive);

    return (err == noErr && alive == 1);
}

- (bool)refreshPriorityList {
    connectedNamesByUID = [self connectedOutputDeviceNamesByUID];
    priorityUIDs = [[self readPriorityUIDsFromDriver] mutableCopy];
    activeUID = [self currentActiveOutputDeviceUID];

    [self reloadTableIfChanged];
    [self updatePriorityButtonStates];

    // We have succeeded in talking to the driver as long as we got this far.
    return true;
}

// A compact fingerprint of everything the table actually displays: chain order,
// each row's availability, and which row is active.
- (NSString *)currentDisplaySignature {
    NSMutableString *signature = [NSMutableString string];

    for (NSString *uid in priorityUIDs) {
        BOOL available = [self isUIDAvailable:uid];
        BOOL active = (activeUID.length > 0 && [activeUID isEqualToString:uid]);
        [signature appendFormat:@"%@|%d|%d;", uid, available ? 1 : 0, active ? 1 : 0];
    }

    return signature;
}

// Reloads the table only when its displayed contents have actually changed. This
// keeps the 1.5s poll from calling reloadData while nothing has changed, which
// would otherwise steal the table's focus/selection and make reordering with the
// up/down buttons fight the refresh. Selection is preserved across real reloads.
- (void)reloadTableIfChanged {
    NSString *signature = [self currentDisplaySignature];

    if ([signature isEqualToString:lastDisplaySignature]) {
        return;
    }

    lastDisplaySignature = signature;

    NSIndexSet *selectedRows = [self.priorityTableView selectedRowIndexes];
    [self.priorityTableView reloadData];

    if (selectedRows.count > 0 && selectedRows.lastIndex < priorityUIDs.count) {
        [self.priorityTableView selectRowIndexes:selectedRows byExtendingSelection:NO];
    }
}

- (void)updatePriorityButtonStates {
    NSInteger selectedRow = self.priorityTableView.selectedRow;
    BOOL hasSelection = (selectedRow >= 0 && selectedRow < (NSInteger)priorityUIDs.count);

    self.removeDeviceButton.enabled = hasSelection;
    self.moveUpButton.enabled = hasSelection && selectedRow > 0;
    self.moveDownButton.enabled = hasSelection && selectedRow < (NSInteger)priorityUIDs.count - 1;

    // The add button is enabled when there is at least one connected device that
    // is not already in the chain.
    self.addDeviceButton.enabled = ([self connectedDevicesNotInChain].count > 0);
}

// After we change the chain, the driver re-evaluates the active device
// asynchronously (and a reorder fires no system device-change event), so the
// "Active" marker can briefly lag. Re-read it shortly after to catch up.
- (void)scheduleActiveMarkerRefresh {
    __weak WindowDelegate *weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.4 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        WindowDelegate *strongSelf = weakSelf;

        if (!strongSelf) {
            return;
        }

        strongSelf->activeUID = [strongSelf currentActiveOutputDeviceUID];
        [strongSelf reloadTableIfChanged];
    });
}

- (NSArray<NSString *> *)connectedDevicesNotInChain {
    NSMutableArray<NSString *> *result = [NSMutableArray array];

    for (NSString *uid in connectedNamesByUID) {
        if (![priorityUIDs containsObject:uid]) {
            [result addObject:uid];
        }
    }

    return result;
}

#pragma mark - NSTableViewDataSource / NSTableViewDelegate

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
#pragma unused(tableView)
    return (NSInteger)priorityUIDs.count;
}

- (NSView *)tableView:(NSTableView *)tableView
    viewForTableColumn:(NSTableColumn *)tableColumn
                   row:(NSInteger)row {
    if (row < 0 || row >= (NSInteger)priorityUIDs.count) {
        return nil;
    }

    NSString *uid = priorityUIDs[(NSUInteger)row];
    NSString *identifier = tableColumn.identifier;
    NSTextField *cell = [tableView makeViewWithIdentifier:identifier owner:self];

    if (!cell) {
        // Configure a non-editable label by hand: +[NSTextField labelWithString:]
        // is 10.12+, and we support macOS 10.11.
        cell = [[NSTextField alloc] initWithFrame:NSZeroRect];
        cell.editable = NO;
        cell.selectable = NO;
        cell.bezeled = NO;
        cell.drawsBackground = NO;
        cell.identifier = identifier;
        cell.lineBreakMode = NSLineBreakByTruncatingTail;
    }

    if ([identifier isEqualToString:kDeviceColumnID]) {
        cell.stringValue = [self displayNameForUID:uid];
        cell.toolTip = uid;
    } else {
        BOOL available = [self isUIDAvailable:uid];
        BOOL isActive = (activeUID.length > 0 && [activeUID isEqualToString:uid]);

        if (isActive) {
            cell.stringValue = NSLocalizedString(@"● Active", nil);
            cell.textColor = [NSColor systemGreenColor];
        } else if (available) {
            cell.stringValue = NSLocalizedString(@"Available", nil);
            cell.textColor = [NSColor secondaryLabelColor];
        } else {
            cell.stringValue = NSLocalizedString(@"Unavailable", nil);
            cell.textColor = [NSColor tertiaryLabelColor];
        }
    }

    return cell;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification {
#pragma unused(notification)
    [self updatePriorityButtonStates];
}

#pragma mark - Priority list actions

- (void)addDeviceClicked:(id)sender {
#pragma unused(sender)
    NSArray<NSString *> *candidates = [self connectedDevicesNotInChain];

    if (candidates.count == 0) {
        return;
    }

    // Sort candidates by display name for a predictable menu order.
    NSArray<NSString *> *sorted = [candidates sortedArrayUsingComparator:^NSComparisonResult(NSString *a, NSString *b) {
        return [[self displayNameForUID:a] caseInsensitiveCompare:[self displayNameForUID:b]];
    }];

    NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];

    for (NSString *uid in sorted) {
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:[self displayNameForUID:uid]
                                                      action:@selector(addSpecificDevice:)
                                               keyEquivalent:@""];
        item.target = self;
        item.representedObject = uid;
        [menu addItem:item];
    }

    [menu popUpMenuPositioningItem:nil
                        atLocation:NSMakePoint(0, self.addDeviceButton.bounds.size.height)
                            inView:self.addDeviceButton];
}

- (void)addSpecificDevice:(NSMenuItem *)sender {
    NSString *uid = sender.representedObject;

    if (uid.length == 0 || [priorityUIDs containsObject:uid]) {
        return;
    }

    [priorityUIDs addObject:uid];
    [self writePriorityUIDsToDriver];
    [self refreshPriorityList];
    [self scheduleActiveMarkerRefresh];

    NSInteger newRow = (NSInteger)priorityUIDs.count - 1;
    [self.priorityTableView selectRowIndexes:[NSIndexSet indexSetWithIndex:newRow] byExtendingSelection:NO];
}

- (void)removeDeviceClicked:(id)sender {
#pragma unused(sender)
    NSInteger row = self.priorityTableView.selectedRow;

    if (row < 0 || row >= (NSInteger)priorityUIDs.count) {
        return;
    }

    [priorityUIDs removeObjectAtIndex:(NSUInteger)row];
    [self writePriorityUIDsToDriver];
    [self refreshPriorityList];
    [self scheduleActiveMarkerRefresh];
}

- (void)moveDeviceUpClicked:(id)sender {
#pragma unused(sender)
    [self moveSelectedRowByOffset:-1];
}

- (void)moveDeviceDownClicked:(id)sender {
#pragma unused(sender)
    [self moveSelectedRowByOffset:1];
}

- (void)moveSelectedRowByOffset:(NSInteger)offset {
    NSInteger row = self.priorityTableView.selectedRow;
    NSInteger target = row + offset;

    if (row < 0 || row >= (NSInteger)priorityUIDs.count || target < 0 || target >= (NSInteger)priorityUIDs.count) {
        return;
    }

    NSString *uid = priorityUIDs[(NSUInteger)row];
    [priorityUIDs removeObjectAtIndex:(NSUInteger)row];
    [priorityUIDs insertObject:uid atIndex:(NSUInteger)target];
    [self writePriorityUIDsToDriver];
    [self refreshPriorityList];
    [self.priorityTableView selectRowIndexes:[NSIndexSet indexSetWithIndex:target] byExtendingSelection:NO];
    [self scheduleActiveMarkerRefresh];
}

- (void)autoFailbackToggled:(id)sender {
#pragma unused(sender)
    BOOL on = (self.autoFailbackCheckbox.state == NSControlStateValueOn);
    [self writeConfigString:[NSString stringWithFormat:@"outputDeviceAutoFailback=%d", on ? 1 : 0]];
    [self scheduleActiveMarkerRefresh];
}

// Retained for the existing XIB action connection on the now-hidden combo box.
- (IBAction)outputDeviceSelected:(id)sender {
#pragma unused(sender)
}

#pragma mark - Buffer size

- (NSString *)currentOutputDeviceBufferFrameSize {
    AudioDeviceID proxyAudioBox = AudioDevice::audioDeviceIDForBoxUID(CFSTR(kBox_UID));
    AudioDevice::setIdentifyValue(proxyAudioBox, -((SInt32)ProxyAudioDevice::ConfigType::outputDeviceBufferFrameSize));
    NSString *result = (__bridge_transfer NSString *)AudioDevice::copyObjectName(proxyAudioBox);

    return result ? result : @"";
}

- (IBAction)outputDeviceBufferFrameSizeSelected:(id)sender {
#pragma unused(sender)
    NSString *newBufferFrameSizeString = self.bufferSizeComboBox.objectValueOfSelectedItem;

    if (!newBufferFrameSizeString) {
        NSLog(@"Error: got invalid buffer frame size value");
        return;
    }

    AudioDeviceID proxyAudioBox = AudioDevice::audioDeviceIDForBoxUID(CFSTR(kBox_UID));
    AudioDevice::setObjectName(
        proxyAudioBox,
        (__bridge CFStringRef)[NSString stringWithFormat:@"outputDeviceBufferFrameSize=%@", newBufferFrameSizeString]);
}

#pragma mark - Active condition

- (ProxyAudioDevice::ActiveCondition)currentOutputDeviceActiveCondition {
    AudioDeviceID proxyAudioBox = AudioDevice::audioDeviceIDForBoxUID(CFSTR(kBox_UID));
    AudioDevice::setIdentifyValue(proxyAudioBox, -((SInt32)ProxyAudioDevice::ConfigType::deviceActiveCondition));
    NSString *result = (__bridge_transfer NSString *)AudioDevice::copyObjectName(proxyAudioBox);

    return (ProxyAudioDevice::ActiveCondition)[result intValue];
}

- (void)setCurrentOutputDeviceActiveCondition:(ProxyAudioDevice::ActiveCondition)condition {
    AudioDeviceID proxyAudioBox = AudioDevice::audioDeviceIDForBoxUID(CFSTR(kBox_UID));
    AudioDevice::setObjectName(
        proxyAudioBox,
        (__bridge CFStringRef)[NSString stringWithFormat:@"outputDeviceActiveCondition=%d", condition]);
}

- (IBAction)proxiedDeviceIsActiveConditionSelected:(id)sender {
#pragma unused(sender)
    self.alwaysRadioButton.state = NSControlStateValueOff;
    self.userIsActiveRadioButton.state = NSControlStateValueOff;
    [self setCurrentOutputDeviceActiveCondition:ProxyAudioDevice::ActiveCondition::proxiedDeviceActive];
}

- (IBAction)userIsActiveConditionSelected:(id)sender {
#pragma unused(sender)
    self.alwaysRadioButton.state = NSControlStateValueOff;
    self.proxiedDeviceIsActiveRadioButton.state = NSControlStateValueOff;
    [self setCurrentOutputDeviceActiveCondition:ProxyAudioDevice::ActiveCondition::userActive];
}

- (IBAction)alwaysConditionSelected:(id)sender {
#pragma unused(sender)
    self.proxiedDeviceIsActiveRadioButton.state = NSControlStateValueOff;
    self.userIsActiveRadioButton.state = NSControlStateValueOff;
    [self setCurrentOutputDeviceActiveCondition:ProxyAudioDevice::ActiveCondition::always];
}

#pragma mark - Hide when unavailable

// The driver reports this preference as "1" or "0" (see copyConfigurationValue
// in ProxyAudioDevice.cpp). Anything non-"1" is treated as false.
- (bool)currentHideWhenUnavailable {
    AudioDeviceID proxyAudioBox = AudioDevice::audioDeviceIDForBoxUID(CFSTR(kBox_UID));
    AudioDevice::setIdentifyValue(proxyAudioBox, -((SInt32)ProxyAudioDevice::ConfigType::deviceHideWhenUnavailable));
    NSString *result = (__bridge_transfer NSString *)AudioDevice::copyObjectName(proxyAudioBox);

    return [result intValue] != 0;
}

- (void)setCurrentHideWhenUnavailable:(bool)hide {
    AudioDeviceID proxyAudioBox = AudioDevice::audioDeviceIDForBoxUID(CFSTR(kBox_UID));
    AudioDevice::setObjectName(
        proxyAudioBox,
        (__bridge CFStringRef)[NSString stringWithFormat:@"outputDeviceHideWhenUnavailable=%d", hide ? 1 : 0]);
}

- (IBAction)hideWhenUnavailableToggled:(id)sender {
#pragma unused(sender)
    [self setCurrentHideWhenUnavailable:(self.hideWhenUnavailableCheckbox.state == NSControlStateValueOn)];
}

#pragma mark - Programmatic priority list UI

// Finds a subview of `parent` whose frame origin matches (x, y). Used to locate
// the static "Proxy device name:" and "Proxied device:" labels, which have no
// outlets, by their known XIB frame origins -- locale-independent.
static NSView *findSubviewByOrigin(NSView *parent, CGFloat x, CGFloat y) {
    for (NSView *view in parent.subviews) {
        if (fabs(view.frame.origin.x - x) < 1.0 && fabs(view.frame.origin.y - y) < 1.0) {
            return view;
        }
    }

    return nil;
}

- (void)setupPriorityListUI {
    if (priorityUIBuilt) {
        return;
    }

    NSWindow *window = self.deviceNameTextField.window;
    NSView *contentView = window.contentView;

    if (!window || !contentView) {
        return;
    }

    const CGFloat E = kPriorityUIExtraHeight;

    // Grow the window upward without disturbing the existing fixed-frame controls:
    // disable subview autoresizing for the height change, then reposition only the
    // controls we care about explicitly. Horizontal resizing is restored after.
    contentView.autoresizesSubviews = NO;

    NSRect frame = window.frame;
    frame.size.height += E;
    [window setFrame:frame display:YES];

    NSSize maxSize = window.contentMaxSize;
    NSSize minSize = window.contentMinSize;
    maxSize.height += E;
    minSize.height += E;
    window.contentMaxSize = maxSize;
    window.contentMinSize = minSize;

    contentView.autoresizesSubviews = YES;

    // Hide the legacy single-device combo box; it is replaced by the table.
    self.outputDeviceComboBox.hidden = YES;

    // Shift the top rows (name label/field and the "Proxied device:" label) up by
    // E so the opened band sits between them and the unchanged buffer-size row.
    NSView *nameLabel = findSubviewByOrigin(contentView, 10.0, 295.0);
    NSView *proxiedLabel = findSubviewByOrigin(contentView, 10.0, 263.0);

    if (nameLabel) {
        NSRect r = nameLabel.frame;
        r.origin.y += E;
        nameLabel.frame = r;
    }

    {
        NSRect r = self.deviceNameTextField.frame;
        r.origin.y += E;
        self.deviceNameTextField.frame = r;
    }

    if ([proxiedLabel isKindOfClass:[NSTextField class]]) {
        NSRect r = proxiedLabel.frame;
        r.origin.y += E;
        r.size.width = 142.0;
        proxiedLabel.frame = r;
        [(NSTextField *)proxiedLabel setStringValue:NSLocalizedString(@"Priority devices:", nil)];
    }

    // Lay out the table, the +/-/up/down buttons, and the auto-failback checkbox
    // in the opened band (between y=251 buffer row and y=413 proxied label).
    NSRect tableFrame = NSMakeRect(kPriorityListLeftX, 311.0, kPriorityListWidth, 96.0);
    NSScrollView *scrollView = [[NSScrollView alloc] initWithFrame:tableFrame];
    scrollView.hasVerticalScroller = YES;
    scrollView.borderType = NSBezelBorder;
    scrollView.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;

    NSTableView *tableView = [[NSTableView alloc] initWithFrame:scrollView.bounds];
    tableView.usesAlternatingRowBackgroundColors = YES;
    tableView.allowsMultipleSelection = NO;
    tableView.rowSizeStyle = NSTableViewRowSizeStyleDefault;
    tableView.dataSource = self;
    tableView.delegate = self;

    NSTableColumn *deviceColumn = [[NSTableColumn alloc] initWithIdentifier:kDeviceColumnID];
    deviceColumn.title = NSLocalizedString(@"Device", nil);
    deviceColumn.width = 224.0;
    deviceColumn.minWidth = 120.0;
    [tableView addTableColumn:deviceColumn];

    NSTableColumn *statusColumn = [[NSTableColumn alloc] initWithIdentifier:kStatusColumnID];
    statusColumn.title = NSLocalizedString(@"Status", nil);
    statusColumn.width = 96.0;
    statusColumn.minWidth = 80.0;
    [tableView addTableColumn:statusColumn];

    scrollView.documentView = tableView;
    [contentView addSubview:scrollView];
    self.priorityTableView = tableView;

    // Buttons row beneath the table.
    const CGFloat buttonY = 285.0;
    const CGFloat buttonW = 30.0;
    const CGFloat buttonH = 22.0;
    self.addDeviceButton = [self makeToolButtonWithTitle:@"+"
                                                       x:kPriorityListLeftX
                                                       y:buttonY
                                                   width:buttonW
                                                  height:buttonH
                                                  action:@selector(addDeviceClicked:)
                                                 toolTip:NSLocalizedString(@"Add a device to the priority list", nil)];
    self.removeDeviceButton = [self makeToolButtonWithTitle:@"−"
                                                          x:kPriorityListLeftX + buttonW
                                                          y:buttonY
                                                      width:buttonW
                                                     height:buttonH
                                                     action:@selector(removeDeviceClicked:)
                                                    toolTip:NSLocalizedString(@"Remove the selected device", nil)];
    self.moveUpButton = [self makeToolButtonWithTitle:@"▲"
                                                    x:kPriorityListLeftX + buttonW * 2 + 8.0
                                                    y:buttonY
                                                width:buttonW
                                               height:buttonH
                                               action:@selector(moveDeviceUpClicked:)
                                              toolTip:NSLocalizedString(@"Move the selected device up", nil)];
    self.moveDownButton = [self makeToolButtonWithTitle:@"▼"
                                                      x:kPriorityListLeftX + buttonW * 3 + 8.0
                                                      y:buttonY
                                                  width:buttonW
                                                 height:buttonH
                                                 action:@selector(moveDeviceDownClicked:)
                                                toolTip:NSLocalizedString(@"Move the selected device down", nil)];

    // Auto-failback checkbox beneath the buttons. +[NSButton checkboxWithTitle:...]
    // is 10.12+, so configure a switch button by hand for macOS 10.11 support.
    NSButton *checkbox = [[NSButton alloc] initWithFrame:NSZeroRect];
    [checkbox setButtonType:NSButtonTypeSwitch];
    checkbox.title = NSLocalizedString(@"Automatically switch back to higher-priority devices", nil);
    checkbox.target = self;
    checkbox.action = @selector(autoFailbackToggled:);
    checkbox.frame = NSMakeRect(kPriorityListLeftX, 257.0, kPriorityListWidth, 18.0);
    checkbox.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    [contentView addSubview:checkbox];
    self.autoFailbackCheckbox = checkbox;

    priorityUIBuilt = true;
}

- (NSButton *)makeToolButtonWithTitle:(NSString *)title
                                    x:(CGFloat)x
                                    y:(CGFloat)y
                                width:(CGFloat)width
                               height:(CGFloat)height
                               action:(SEL)action
                              toolTip:(NSString *)toolTip {
    // +[NSButton buttonWithTitle:...] is 10.12+; build a push button by hand to
    // keep macOS 10.11 compatibility.
    NSButton *button = [[NSButton alloc] initWithFrame:NSZeroRect];
    [button setButtonType:NSButtonTypeMomentaryPushIn];
    button.title = title;
    button.target = self;
    button.action = action;
    button.frame = NSMakeRect(x, y, width, height);
    button.bezelStyle = NSBezelStyleRounded;
    button.autoresizingMask = NSViewMaxXMargin | NSViewMinYMargin;
    button.toolTip = toolTip;
    NSView *contentView = self.deviceNameTextField.window.contentView;
    [contentView addSubview:button];
    return button;
}

@end
