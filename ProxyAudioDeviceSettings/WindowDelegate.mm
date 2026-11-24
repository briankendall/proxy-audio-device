#include <vector>
#include <CoreAudio/CoreAudio.h>
#import "WindowDelegate.h"
#include "AudioDevice.h"
#include "ProxyAudioDevice.h"

int onDevicesChanged(AudioObjectID inObjectID,
                     UInt32 inNumberAddresses,
                     const AudioObjectPropertyAddress *inAddresses,
                     void *inClientData);

// Height added to the bottom of the window for the equalizer section.
static const CGFloat kEqualizerSectionHeight = 230;

@implementation WindowDelegate {
    std::vector<AudioDeviceID> currentDeviceList;
    int initializationAttemptInterval;
    NSButton *equalizerEnabledCheckbox;
    NSButton *equalizerFlatButton;
    // Index 0 is the preamp, then one per band.
    NSMutableArray<NSSlider *> *equalizerSliders;
    NSMutableArray<NSTextField *> *equalizerValueLabels;
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
    self.deviceIconPopUpButton.enabled = NO;
    [self populateDeviceIconPopUpButton];
    [self setupEqualizerControls];
    [self setEqualizerControlsEnabled:NO];
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
    
    if (![self refreshOutputDevices]) {
        return false;
    }
    
    if (![self setupListenerForCurrentAudioDevices]) {
        return false;
    }
    
    [self.bufferSizeComboBox selectItemWithObjectValue:[self currentOutputDeviceBufferFrameSize]];
    self.deviceNameTextField.enabled = YES;
    self.outputDeviceComboBox.enabled = YES;
    self.bufferSizeComboBox.enabled = YES;
    self.proxiedDeviceIsActiveRadioButton.enabled = YES;
    self.userIsActiveRadioButton.enabled = YES;
    self.alwaysRadioButton.enabled = YES;
    self.hideWhenUnavailableCheckbox.enabled = YES;
    self.hideWhenUnavailableCheckbox.state =
        [self currentHideWhenUnavailable] ? NSControlStateValueOn : NSControlStateValueOff;
    self.deviceIconPopUpButton.enabled = YES;
    [self.deviceIconPopUpButton selectItemWithTag:(NSInteger)[self currentDeviceIcon]];
    [self showEqualizerSettings:[self currentEqualizerSettings]];
    [self setEqualizerControlsEnabled:YES];

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
        [delegate refreshOutputDevices];
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
                               (__bridge_retained CFStringRef)[NSString stringWithFormat:@"deviceName=%@", newName]);
}

- (AudioDeviceID)currentOutputDevice {
    AudioDeviceID proxyAudioBox = AudioDevice::audioDeviceIDForBoxUID(CFSTR(kBox_UID));
    AudioDevice::setIdentifyValue(proxyAudioBox, -((SInt32)ProxyAudioDevice::ConfigType::outputDevice));
    NSString *outputDeviceUID = (__bridge_transfer NSString *)AudioDevice::copyObjectName(proxyAudioBox);
    
    return AudioDevice::audioDeviceIDForDeviceUID((__bridge_retained CFStringRef)outputDeviceUID);
}

- (bool)refreshOutputDevices {
    bool success = false;
    [self.outputDeviceComboBox removeAllItems];
    currentDeviceList = AudioDevice::devicesWithOutputCapabilitiesThatAreNotProxyAudioDevice();
    AudioDeviceID outputDevice = [self currentOutputDevice];
    
    for (unsigned int i = 0; i < currentDeviceList.size(); ++i) {
        NSString *deviceName = (__bridge_transfer NSString *)AudioDevice::copyObjectName(currentDeviceList[i]);
        
        if (!deviceName) {
            NSLog(@"Note: got null device name for audio device with device with ID: %d", currentDeviceList[i]);
            continue;
        }
        
        [self.outputDeviceComboBox addItemWithObjectValue:deviceName];
        
        if (outputDevice == currentDeviceList[i]) {
            [self.outputDeviceComboBox selectItemAtIndex:i];
        }
        
        success = true;
    }
    
    if (!success) {
        NSLog(@"Error: failed to get any information about current output devices!");
    }
    
    return success;
}

- (IBAction)outputDeviceSelected:(id)sender {
#pragma unused(sender)
    unsigned long index = (unsigned long)self.outputDeviceComboBox.indexOfSelectedItem;
    
    if (index < 0 || index >= currentDeviceList.size()) {
        NSLog(@"Error: got invalid selection index when trying to set output device");
        return;
    }

    NSString *uid = (__bridge_transfer NSString *)AudioDevice::copyDeviceUID(currentDeviceList[index]);
    
    if (!uid) {
        NSLog(@"Error: got invalid UID when trying to set output device");
        return;
    }

    AudioDeviceID proxyAudioBox = AudioDevice::audioDeviceIDForBoxUID(CFSTR(kBox_UID));
    AudioDevice::setObjectName(proxyAudioBox,
                               (__bridge_retained CFStringRef)[NSString stringWithFormat:@"outputDevice=%@", uid]);
}

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
        (__bridge_retained CFStringRef)
            [NSString stringWithFormat:@"outputDeviceBufferFrameSize=%@", newBufferFrameSizeString]);
}

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
        (__bridge_retained CFStringRef)
            [NSString stringWithFormat:@"outputDeviceActiveCondition=%d", condition]);
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
        (__bridge_retained CFStringRef)
            [NSString stringWithFormat:@"outputDeviceHideWhenUnavailable=%d", hide ? 1 : 0]);
}

- (IBAction)hideWhenUnavailableToggled:(id)sender {
    #pragma unused(sender)
    [self setCurrentHideWhenUnavailable:(self.hideWhenUnavailableCheckbox.state == NSControlStateValueOn)];
}

// The glyphs Control Center shows for output devices. Tags are DeviceIcon
// values; how the driver gets macOS to show each one is up to the driver.
- (void)populateDeviceIconPopUpButton {
    struct {
        NSString *title;
        NSString *symbolName;
        DeviceIcon icon;
    } choices[] = {
        {NSLocalizedString(@"Default", nil), @"speaker.wave.2.fill", DeviceIcon::standard},
        {NSLocalizedString(@"Laptop", nil), @"laptopcomputer", DeviceIcon::laptop},
        {NSLocalizedString(@"TV", nil), @"tv.and.hifispeaker.fill", DeviceIcon::tv},
    };

    [self.deviceIconPopUpButton removeAllItems];

    for (const auto &choice : choices) {
        [self.deviceIconPopUpButton addItemWithTitle:choice.title];
        NSMenuItem *item = self.deviceIconPopUpButton.lastItem;
        item.tag = (NSInteger)choice.icon;

        if (@available(macOS 11.0, *)) {
            item.image = [NSImage imageWithSystemSymbolName:choice.symbolName accessibilityDescription:choice.title];
        }
    }
}

- (DeviceIcon)currentDeviceIcon {
    AudioDeviceID proxyAudioBox = AudioDevice::audioDeviceIDForBoxUID(CFSTR(kBox_UID));
    AudioDevice::setIdentifyValue(proxyAudioBox, -((SInt32)ProxyAudioDevice::ConfigType::deviceIcon));
    NSString *result = (__bridge_transfer NSString *)AudioDevice::copyObjectName(proxyAudioBox);
    int value = result ? [result intValue] : 0;

    return isValidDeviceIcon(value) ? DeviceIcon(value) : kDeviceDefaultIcon;
}

- (IBAction)deviceIconSelected:(id)sender {
    #pragma unused(sender)
    AudioDeviceID proxyAudioBox = AudioDevice::audioDeviceIDForBoxUID(CFSTR(kBox_UID));
    AudioDevice::setObjectName(
        proxyAudioBox,
        (__bridge CFStringRef)[NSString stringWithFormat:@"deviceIcon=%ld", (long)self.deviceIconPopUpButton.selectedTag]);
}

#pragma mark Equalizer

- (NSTextField *)makeLabel:(NSString *)text frame:(NSRect)frame alignment:(NSTextAlignment)alignment small:(BOOL)small {
    NSTextField *label = [[NSTextField alloc] initWithFrame:frame];
    label.stringValue = text;
    label.bezeled = NO;
    label.drawsBackground = NO;
    label.editable = NO;
    label.selectable = NO;
    label.alignment = alignment;
    label.autoresizingMask = NSViewMaxXMargin | NSViewMaxYMargin;

    if (small) {
        label.font = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
        label.textColor = [NSColor secondaryLabelColor];
    }

    return label;
}

// The section is built in code rather than in the xib: it's 11 identical
// columns, which are easier to keep consistent as a loop.
- (void)setupEqualizerControls {
    NSWindow *window = self.deviceNameTextField.window;
    NSView *content = window.contentView;
    const CGFloat H = kEqualizerSectionHeight;

    // Existing controls are pinned to the top, so growing the window makes room at the bottom.
    NSSize size = content.frame.size;
    size.height += H;
    [window setContentSize:size];
    window.contentMinSize = NSMakeSize(window.contentMinSize.width, size.height);
    window.contentMaxSize = NSMakeSize(window.contentMaxSize.width, size.height);

    NSBox *separator = [[NSBox alloc] initWithFrame:NSMakeRect(20, H - 1, size.width - 40, 1)];
    separator.boxType = NSBoxSeparator;
    separator.autoresizingMask = NSViewWidthSizable | NSViewMaxYMargin;
    [content addSubview:separator];

    [content addSubview:[self makeLabel:NSLocalizedString(@"Equalizer:", nil)
                                  frame:NSMakeRect(10, H - 30, 142, 16)
                              alignment:NSTextAlignmentRight
                                  small:NO]];

    equalizerEnabledCheckbox = [[NSButton alloc] initWithFrame:NSMakeRect(157, H - 31, 200, 18)];
    [equalizerEnabledCheckbox setButtonType:NSButtonTypeSwitch];
    equalizerEnabledCheckbox.title = NSLocalizedString(@"Enabled", nil);
    equalizerEnabledCheckbox.target = self;
    equalizerEnabledCheckbox.action = @selector(equalizerChanged:);
    equalizerEnabledCheckbox.autoresizingMask = NSViewMaxXMargin | NSViewMaxYMargin;
    [content addSubview:equalizerEnabledCheckbox];

    equalizerFlatButton = [[NSButton alloc] initWithFrame:NSMakeRect(size.width - 100, H - 38, 86, 32)];
    equalizerFlatButton.bezelStyle = NSBezelStyleRounded;
    equalizerFlatButton.title = NSLocalizedString(@"Flat", nil);
    equalizerFlatButton.target = self;
    equalizerFlatButton.action = @selector(equalizerFlatPressed:);
    equalizerFlatButton.autoresizingMask = NSViewMinXMargin | NSViewMaxYMargin;
    [content addSubview:equalizerFlatButton];

    equalizerSliders = [NSMutableArray array];
    equalizerValueLabels = [NSMutableArray array];

    const CGFloat columnWidth = 40;
    const CGFloat sliderBottom = 42;
    const CGFloat sliderHeight = H - 100;

    for (int column = 0; column <= kEqualizerBandCount; column++) {
        // Leave a gap between the preamp and the bands.
        CGFloat x = 20 + column * columnWidth + (column > 0 ? 30 : 0);
        NSString *title;

        if (column == 0) {
            title = NSLocalizedString(@"Preamp", nil);
        } else {
            double frequency = kEqualizerBandFrequencies[column - 1];
            title = frequency >= 1000 ? [NSString stringWithFormat:@"%gk", frequency / 1000]
                                      : [NSString stringWithFormat:@"%d", (int)frequency];
        }

        // Taller than wide, so AppKit draws it vertically.
        NSSlider *slider = [[NSSlider alloc]
            initWithFrame:NSMakeRect(x + (columnWidth - 24) / 2, sliderBottom, 24, sliderHeight)];
        slider.minValue = kEqualizerMinGainDB;
        slider.maxValue = kEqualizerMaxGainDB;
        slider.doubleValue = 0;
        slider.numberOfTickMarks = 5;
        slider.continuous = YES;
        slider.target = self;
        slider.action = @selector(equalizerChanged:);
        slider.autoresizingMask = NSViewMaxXMargin | NSViewMaxYMargin;
        [content addSubview:slider];
        [equalizerSliders addObject:slider];

        NSTextField *valueLabel = [self makeLabel:@"0"
                                            frame:NSMakeRect(x - 5, sliderBottom + sliderHeight + 4, columnWidth + 10, 14)
                                        alignment:NSTextAlignmentCenter
                                            small:YES];
        [content addSubview:valueLabel];
        [equalizerValueLabels addObject:valueLabel];

        [content addSubview:[self makeLabel:title
                                      frame:NSMakeRect(x - 10, 20, columnWidth + 20, 16)
                                  alignment:NSTextAlignmentCenter
                                      small:NO]];
    }
}

- (void)setEqualizerControlsEnabled:(BOOL)enabled {
    equalizerEnabledCheckbox.enabled = enabled;
    equalizerFlatButton.enabled = enabled;

    for (NSSlider *slider in equalizerSliders) {
        slider.enabled = enabled;
    }
}

// Snap to half dB steps, so the stored values stay short and readable. The
// slider itself isn't snapped, since writing to it mid-drag fights the mouse.
static double snappedGain(NSSlider *slider) {
    return EqualizerClampGain(round(slider.doubleValue * 2) / 2);
}

- (void)updateEqualizerValueLabels {
    for (NSUInteger i = 0; i < equalizerSliders.count; i++) {
        double value = snappedGain(equalizerSliders[i]);
        equalizerValueLabels[i].stringValue = value == 0 ? @"0" : [NSString stringWithFormat:@"%+.1f", value];
    }
}

- (EqualizerSettings)equalizerSettingsFromControls {
    EqualizerSettings settings;
    settings.enabled = equalizerEnabledCheckbox.state == NSControlStateValueOn;

    for (NSUInteger i = 0; i < equalizerSliders.count; i++) {
        double value = snappedGain(equalizerSliders[i]);

        if (i == 0) {
            settings.preampDB = value;
        } else {
            settings.gainsDB[i - 1] = value;
        }
    }

    return settings;
}

- (void)showEqualizerSettings:(const EqualizerSettings &)settings {
    equalizerEnabledCheckbox.state = settings.enabled ? NSControlStateValueOn : NSControlStateValueOff;
    equalizerSliders[0].doubleValue = settings.preampDB;

    for (int band = 0; band < kEqualizerBandCount; band++) {
        equalizerSliders[band + 1].doubleValue = settings.gainsDB[band];
    }

    [self updateEqualizerValueLabels];
}

- (EqualizerSettings)currentEqualizerSettings {
    AudioDeviceID proxyAudioBox = AudioDevice::audioDeviceIDForBoxUID(CFSTR(kBox_UID));
    AudioDevice::setIdentifyValue(proxyAudioBox, -((SInt32)ProxyAudioDevice::ConfigType::equalizer));
    NSString *result = (__bridge_transfer NSString *)AudioDevice::copyObjectName(proxyAudioBox);
    EqualizerSettings settings;
    EqualizerSettingsFromString((__bridge CFStringRef)result, settings);

    return settings;
}

- (void)sendEqualizerSettings:(const EqualizerSettings &)settings {
    NSString *value = (__bridge_transfer NSString *)EqualizerSettingsCopyString(settings);
    AudioDeviceID proxyAudioBox = AudioDevice::audioDeviceIDForBoxUID(CFSTR(kBox_UID));
    AudioDevice::setObjectName(proxyAudioBox,
                               (__bridge CFStringRef)[NSString stringWithFormat:@"equalizer=%@", value]);
}

- (IBAction)equalizerChanged:(id)sender {
    #pragma unused(sender)
    EqualizerSettings settings = [self equalizerSettingsFromControls];
    [self updateEqualizerValueLabels];
    [self sendEqualizerSettings:settings];
}

- (IBAction)equalizerFlatPressed:(id)sender {
    #pragma unused(sender)
    for (NSSlider *slider in equalizerSliders) {
        slider.doubleValue = 0;
    }

    [self equalizerChanged:sender];
}

@end
