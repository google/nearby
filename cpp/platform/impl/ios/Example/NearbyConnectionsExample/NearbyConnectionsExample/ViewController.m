//
// Copyright (c) 2021 Google Inc.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
//  ViewController.m
//  NearbyConnectionsExample
//

#import "ViewController.h"

#import <NearbyConnections/NearbyConnections.h>

NS_ASSUME_NONNULL_BEGIN

static NSString *kServiceId = @"com.google.NearbyConnectionsExample";
static NSString *kCellIdentifier = @"endpointCell";

// Simplified version of dispatch_after.
void delay(NSTimeInterval delay, dispatch_block_t block) {
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), block);
}

// This class contains info about a discovered endpoint.
@interface EndpointInfo : NSObject
@property(nonatomic, readonly) id<GNCDiscoveredEndpointInfo> discInfo;
@property(nonatomic, nullable) id<GNCConnection> connection;
@end

@implementation EndpointInfo
- (instancetype)initWithDiscoveredInfo:(id<GNCDiscoveredEndpointInfo>)discInfo {
  self = [super init];
  if (self) {
    _discInfo = discInfo;
  }
  return self;
}
@end

@interface ViewController () <UITableViewDataSource, UITableViewDelegate>
@property(nonatomic) GNCAdvertiser *advertiser;
@property(nonatomic) GNCDiscoverer *discoverer;
@property(nonatomic) NSMutableDictionary<GNCEndpointId, EndpointInfo *> *endpoints;
@property(nonatomic, readonly) NSData *ping;
@property(nonatomic, readonly) NSData *pong;
@property(nonatomic) UITableView *tableView;
@property(nonatomic) UITextView *statusView;
@property(nonatomic) NSMutableDictionary<GNCEndpointId, id<GNCConnection> > *incomingConnections;
@end

@implementation ViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  [self makeViews];

  // Enable "info" log messages in the release build.
  [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"GTMVerboseLogging"];

  self.title = [[UIDevice currentDevice] name];
  NSData *endpointInfo = [self.title dataUsingEncoding:NSUTF8StringEncoding];
  _endpoints = [NSMutableDictionary dictionary];
  _ping = [@"ping" dataUsingEncoding:NSUTF8StringEncoding];
  _pong = [@"pong" dataUsingEncoding:NSUTF8StringEncoding];
  _incomingConnections = [NSMutableDictionary dictionary];

  // The advertiser.
  _advertiser = [GNCAdvertiser
       advertiserWithEndpointInfo:endpointInfo
                        serviceId:kServiceId
                         strategy:GNCStrategyCluster
      connectionInitiationHandler:^(GNCEndpointId endpointId,
                                    id<GNCAdvertiserConnectionInfo> advConnInfo,
                                    GNCConnectionResponseHandler responseHandler) {
        // Show a status that a discoverer has requested a connection.
        [self logStatus:@"Accepting connection request" final:NO];

        // Accept the connection request.
        responseHandler(GNCConnectionResponseAccept);
        return [GNCConnectionResultHandlers
            successHandler:^(id<GNCConnection> connection) {
              // Save the connection until the ping-pong sequence is done.
              self.incomingConnections[endpointId] = connection;
              __block BOOL receivedPong = NO;

              // Send a ping, expecting the remote endpoint to send a pong.
              [self logStatus:@"Connection established; sending ping" final:NO];
              [connection sendBytesPayload:[GNCBytesPayload payloadWithBytes:self.ping]
                                completion:^(GNCPayloadResult result) {
                                  if (result == GNCPayloadResultSuccess) {
                                    [self logStatus:@"Sent ping; waiting for pong" final:NO];

                                    // Show an error if the pong isn't received in the expected
                                    // timeframe.
                                    delay(3.0, ^{
                                      if (receivedPong) {
                                        [self logStatus:@"Error: Didn't receive pong" final:YES];
                                        [self.incomingConnections
                                            removeObjectForKey:endpointId];  // close the connection
                                      }
                                    });
                                  } else {
                                    [self logStatus:@"Error: Failed to send ping" final:YES];
                                  }
                                }];

              // Return handlers for incoming payloads.
              return [GNCConnectionHandlers handlersWithBuilder:^(GNCConnectionHandlers *handlers) {
                handlers.bytesPayloadHandler = ^(GNCBytesPayload *payload) {
                  receivedPong = NO;
                  [self.incomingConnections removeObjectForKey:endpointId];  // close the connection

                  // Show a status of whether the pong was received.
                  [self logStatus:[payload.bytes isEqual:self.pong] ? @"Received pong"
                                                                    : @"Error: Payload is not pong"
                            final:YES];
                };
              }];
            }
            failureHandler:^(GNCConnectionFailure result) {
              [self
                  logStatus:(result == GNCConnectionFailureRejected) ? @"Error: Connection rejected"
                                                                     : @"Error: Connection failed"
                      final:YES];
            }];
      }];

  // The discoverer.
  __weak typeof(self) weakSelf = self;
  _discoverer = [GNCDiscoverer
      discovererWithServiceId:kServiceId
                     strategy:GNCStrategyCluster
         endpointFoundHandler:^(GNCEndpointId endpointId,
                                id<GNCDiscoveredEndpointInfo> endpointInfo) {
           typeof(self) self = weakSelf;

           // An endpoint was discovered; add it to the endpoint list and UI.
           self.endpoints[endpointId] = [[EndpointInfo alloc] initWithDiscoveredInfo:endpointInfo];
           [self.tableView reloadData];

           // Return the lost handler for this endpoint.
           return ^{
             typeof(self) self = weakSelf;  // shadow

             // Endpoint disappeared; remove it from the endpoint list and UI.
             [self.endpoints removeObjectForKey:endpointId];
             [self.tableView reloadData];
           };
         }];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
  // The user tapped on a cell; request a connection with it.
  EndpointInfo *info = _endpoints[_endpoints.allKeys[indexPath.row]];
  if (!info) return;

  void (^deselectRow)(void) = ^{
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
  };

  [self logStatus:@"Requesting connection" final:NO];
  info.discInfo.requestConnection(
      self.title,
      ^(id<GNCDiscovererConnectionInfo> discConnInfo,
        GNCConnectionResponseHandler responseHandler) {
        // Accept the auth token.
        [self logStatus:@"Accepting auth token" final:NO];
        responseHandler(GNCConnectionResponseAccept);
        return ^(id<GNCConnection> connection) {
          // Save the connection until the ping-pong sequence is done.
          info.connection = connection;
          __block BOOL receivedPing = NO;

          [self logStatus:@"Connection established; waiting for ping" final:NO];

          // Show an error if the ping isn't received in the expected timeframe.
          delay(3.0, ^{
            if (!receivedPing) {
              deselectRow();
              [self logStatus:@"Error: Didn't receive ping" final:YES];
              info.connection = nil;  // close the connection
            }
          });

          // Return handlers for incoming payloads.
          __weak id<GNCConnection> weakConnection = connection;  // avoid a retain cycle
          return [GNCConnectionHandlers handlersWithBuilder:^(GNCConnectionHandlers *handlers) {
            handlers.bytesPayloadHandler = ^(GNCBytesPayload *payload) {
              receivedPing = YES;

              // If a ping was received, send a pong back to the advertiser.
              if ([payload.bytes isEqual:self.ping]) {
                [self logStatus:@"Received ping; sending pong" final:NO];
                [weakConnection sendBytesPayload:[GNCBytesPayload payloadWithBytes:self.pong]
                                      completion:^(GNCPayloadResult result) {
                                        deselectRow();
                                        [self logStatus:(result == GNCPayloadResultSuccess)
                                                            ? @"Sent pong"
                                                            : @"Error: Failed to send pong"
                                                  final:YES];

                                        // Pong was sent, so close the connection.
                                        info.connection = nil;
                                      }];
              } else {
                deselectRow();
                [self logStatus:@"Error: Payload is not ping" final:YES];
              }
            };
          }];
        };
      },
      ^(GNCConnectionFailure result) {
        // Connection failed.
        deselectRow();
        [self logStatus:(result == GNCConnectionFailureRejected) ? @"Connection rejected"
                                                                 : @"Connection failed"
                  final:YES];
      });
}

- (UITableViewCell *)tableView:(UITableView *)tableView
         cellForRowAtIndexPath:(NSIndexPath *)indexPath {
  UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:kCellIdentifier
                                                          forIndexPath:indexPath];
  cell.textLabel.text = _endpoints[_endpoints.allKeys[indexPath.row]].discInfo.name;
  return cell;
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
  return [_endpoints.allKeys count];
}

#pragma mark - Private

- (void)makeViews {
  _tableView = [[UITableView alloc] initWithFrame:self.view.frame style:UITableViewStylePlain];
  [_tableView registerClass:[UITableViewCell class] forCellReuseIdentifier:kCellIdentifier];
  _tableView.delegate = self;
  _tableView.dataSource = self;
  _tableView.rowHeight = 48;
  _tableView.scrollEnabled = YES;
  _tableView.showsVerticalScrollIndicator = YES;
  _tableView.userInteractionEnabled = YES;
  _tableView.bounces = YES;
  _tableView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [self.view addSubview:_tableView];

  // Make the status view.
  UITextView * (^newTextView)(CGRect) = ^(CGRect frame) {
    UITextView *textView = [[UITextView alloc] initWithFrame:frame];
    textView.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleTopMargin;
    textView.layer.borderColor = [UIColor blackColor].CGColor;
    textView.layer.borderWidth = 1;
    textView.editable = NO;
    textView.textContainerInset = UIEdgeInsetsZero;
    return textView;
  };
  CGRect selfFrame = self.view.frame;
  static const int kStatusHeight = 144;
  CGRect statusFrame = (CGRect){{selfFrame.origin.x + 4, selfFrame.size.height - kStatusHeight},
                                {selfFrame.size.width - 8, kStatusHeight - 4}};
  _statusView = newTextView(statusFrame);
  [self.view addSubview:_statusView];
}

- (void)logStatus:(NSString *)status final:(BOOL)final {
  _statusView.text = [NSString stringWithFormat:@"%@\n%@%@", _statusView.text, status,
                                                final ? @"\n–––––––––––––––––––––––––" : @""];
  [_statusView scrollRangeToVisible:NSMakeRange(_statusView.text.length - 1, 1)];
}

@end

NS_ASSUME_NONNULL_END
