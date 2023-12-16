//
//  CloudNotification.swift
//  HelloCloudApple
//
//  Created by Deling Ren on 12/15/23.
//

import Foundation
import FirebaseFunctions

class CloudNotification {
  static let shared = CloudNotification()

  lazy var functions = Functions.functions()

  init() {
  }

  func helloWorld() -> Void {
    functions.httpsCallable("helloWorld").call() { result, error in
      if let error = error as NSError? {
        if error.domain == FunctionsErrorDomain {
          let code = FunctionsErrorCode(rawValue: error.code)
          let message = error.localizedDescription
          let details = error.userInfo[FunctionsErrorDetailsKey]
        }
      }
      if let text = result?.data as? String {
        print(text)
      }
    }
  }
}
