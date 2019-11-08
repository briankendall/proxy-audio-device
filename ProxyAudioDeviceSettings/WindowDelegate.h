#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface WindowDelegate : NSObject
@property(nonatomic, strong) IBOutlet NSTextField *deviceNameTextField;
@property(nonatomic, strong) IBOutlet NSComboBox *outputDeviceComboBox;
@property(nonatomic, strong) IBOutlet NSComboBox *bufferSizeComboBox;
@property(nonatomic, strong) IBOutlet NSButton *proxiedDeviceIsActiveRadioButton;
@property(nonatomic, strong) IBOutlet NSButton *userIsActiveRadioButton;
@property(nonatomic, strong) IBOutlet NSButton *alwaysRadioButton;

- (void)awakeFromNib;
- (IBAction)deviceNameEntered:(id)sender;
- (IBAction)outputDeviceSelected:(id)sender;
- (IBAction)outputDeviceBufferFrameSizeSelected:(id)sender;
- (IBAction)proxiedDeviceIsActiveConditionSelected:(id)sender;
- (IBAction)userIsActiveConditionSelected:(id)sender;
- (IBAction)alwaysConditionSelected:(id)sender;

@end

NS_ASSUME_NONNULL_END
