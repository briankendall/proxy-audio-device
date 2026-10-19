#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface WindowDelegate : NSObject <NSTableViewDataSource, NSTableViewDelegate>
@property(nonatomic, strong) IBOutlet NSTextField *deviceNameTextField;
// Legacy single-device combo box. Kept so the existing XIB connection stays
// valid, but hidden at runtime and replaced by the priority list table below.
@property(nonatomic, strong) IBOutlet NSComboBox *outputDeviceComboBox;
@property(nonatomic, strong) IBOutlet NSComboBox *bufferSizeComboBox;
@property(nonatomic, strong) IBOutlet NSButton *proxiedDeviceIsActiveRadioButton;
@property(nonatomic, strong) IBOutlet NSButton *userIsActiveRadioButton;
@property(nonatomic, strong) IBOutlet NSButton *alwaysRadioButton;
@property(nonatomic, strong) IBOutlet NSButton *hideWhenUnavailableCheckbox;

// Priority list UI, built programmatically (see setupPriorityListUI).
@property(nonatomic, strong) NSTableView *priorityTableView;
@property(nonatomic, strong) NSButton *addDeviceButton;
@property(nonatomic, strong) NSButton *removeDeviceButton;
@property(nonatomic, strong) NSButton *moveUpButton;
@property(nonatomic, strong) NSButton *moveDownButton;
@property(nonatomic, strong) NSButton *autoFailbackCheckbox;

- (void)awakeFromNib;
- (IBAction)deviceNameEntered:(id)sender;
- (IBAction)outputDeviceSelected:(id)sender;
- (IBAction)outputDeviceBufferFrameSizeSelected:(id)sender;
- (IBAction)proxiedDeviceIsActiveConditionSelected:(id)sender;
- (IBAction)userIsActiveConditionSelected:(id)sender;
- (IBAction)alwaysConditionSelected:(id)sender;
- (IBAction)hideWhenUnavailableToggled:(id)sender;

// Priority list actions.
- (void)addDeviceClicked:(id)sender;
- (void)removeDeviceClicked:(id)sender;
- (void)moveDeviceUpClicked:(id)sender;
- (void)moveDeviceDownClicked:(id)sender;
- (void)autoFailbackToggled:(id)sender;

@end

NS_ASSUME_NONNULL_END
