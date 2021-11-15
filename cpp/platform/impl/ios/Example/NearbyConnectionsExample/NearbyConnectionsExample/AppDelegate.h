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
//  AppDelegate.h
//  NearbyConnectionsExample
//

#import <UIKit/UIKit.h>

/**
 * A delegate for NSApplication to handle notifications about app launch and
 * shutdown. Owned by the application object.
 */
@interface AppDelegate : UIResponder <UIApplicationDelegate>

/**
 * Main screen window displayed to the user which contains any active view
 * hierarchy.
 */
@property(nonatomic) UIWindow *window;

@end
