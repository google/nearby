//
//  Copyright 2023 Google LLC
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

import SwiftUI
import FirebaseCore
import FirebaseMessaging

class AppDelegate: NSObject, UIApplicationDelegate, UNUserNotificationCenterDelegate, MessagingDelegate {
  static private(set) var shared: AppDelegate! = nil
  var notificationToken: String? = nil

  func application(_ application: UIApplication,
                   didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey : Any]? = nil)
  -> Bool {
    // Set up singleton
    AppDelegate.shared = self

    // Initialize Firebase
    FirebaseApp.configure()

    // Set up FCM MessagingDelegate
    Messaging.messaging().delegate = self

    // Register for remote notifications
    UNUserNotificationCenter.current().delegate = self
    let authOptions: UNAuthorizationOptions = [.badge, .alert, .sound]
    UNUserNotificationCenter.current().requestAuthorization(
      options: authOptions,
      completionHandler: { _, _ in }
    )
    application.registerForRemoteNotifications()

    return true
  }

  func application(_ application: UIApplication,
                   didRegisterForRemoteNotificationsWithDeviceToken deviceToken: Data) {
    Messaging.messaging().apnsToken = deviceToken
  }

  func messaging(_ messaging: Messaging, didReceiveRegistrationToken fcmToken: String?) {
    AppDelegate.shared.notificationToken = fcmToken
    print("Firebase registration token: \(String(describing: fcmToken))")

    let dataDict: [String: String] = ["token": fcmToken ?? ""]
    NotificationCenter.default.post(
      name: Notification.Name("FCMToken"),
      object: nil,
      userInfo: dataDict
    )
  }

  // Executed when the notification is received
  func userNotificationCenter(_ center: UNUserNotificationCenter,
                              willPresent notification: UNNotification) async
  -> UNNotificationPresentationOptions {
    let userInfo = notification.request.content.userInfo
    let id = UUID(uuidString: userInfo["packetId"] as? String ?? "")
    if id != nil {
      Main.shared.onPacketUploaded(id: id!)
    }

    return [[.banner, .sound]]
  }

  // Executed when the user clicks on the notification.
  func userNotificationCenter(_ center: UNUserNotificationCenter,
                              didReceive response: UNNotificationResponse) async {
    let userInfo = response.notification.request.content.userInfo
    let id = UUID(uuidString: userInfo["packetId"] as? String ?? "")
    if id != nil {
      Main.shared.showInboxAndHilight(packet: id!)
    }
  }
}

@main
struct HelloCloudApp: App {
  // register app delegate for Firebase setup
  @UIApplicationDelegateAdaptor(AppDelegate.self) var delegate

  private let mainModel: Main
  private let mainView: MainView

  init() {
    mainModel = Main.create();
    mainView = MainView(model: mainModel)
  }

  var body: some Scene {
    WindowGroup {
      mainView.environment(mainModel)
    }
  }
}
