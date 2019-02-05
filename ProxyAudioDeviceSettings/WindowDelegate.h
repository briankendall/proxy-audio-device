#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface WindowDelegate : NSObject
@property(nonatomic, strong) IBOutlet NSTextField *deviceNameTextField;
@property(nonatomic, strong) IBOutlet NSComboBox *outputDeviceComboBox;
@property(nonatomic, strong) IBOutlet NSComboBox *bufferSizeComboBox;

- (void)awakeFromNib;
- (IBAction)deviceNameEntered:(id)sender;
- (IBAction)outputDeviceSelected:(id)sender;
- (IBAction)outputDeviceBufferFrameSizeSelected:(id)sender;

@end

NS_ASSUME_NONNULL_END
