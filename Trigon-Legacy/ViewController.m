// ViewController.m
// Trigon-Legacy, 2025

#import "ViewController.h"
#include "trigon-legacy.h"

@interface ViewController ()

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    [UIApplication sharedApplication].idleTimerDisabled = YES;
}

- (IBAction)go:(UIButton*)sender {
    [sender setEnabled:NO];
    // for (int i = 0; i < 5000; i++) {
        trigon_legacy();
    //     NSLog(@"%d", i);
    //     usleep(500000);
    // }
}

@end
