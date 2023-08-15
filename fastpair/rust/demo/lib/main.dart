// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import 'package:flutter/material.dart';
import 'package:demo/rust.dart';

void main() {
  api.init();
  runApp(const FastPairApp());
}

class FastPairApp extends StatelessWidget {
  const FastPairApp({super.key});
  @override
  Widget build(BuildContext context) => MaterialApp(
        title: 'Fast Pair',
        theme: ThemeData(
          primarySwatch: Colors.blue,
        ),
        home: const HomePage(),
      );
}

class HomePage extends StatelessWidget {
  const HomePage({super.key});

  @override
  Widget build(BuildContext context) => Scaffold(
        appBar: AppBar(
          title: const Text("Fast Pair"),
        ),
        body: Center(
          child: StreamBuilder(
            // Retrieve device info stream from Rust side.
            stream: api.eventStream(),
            builder: (context, deviceInfo) {
              var deviceName = deviceInfo.data?[0];
              var deviceImageUrl = deviceInfo.data?[1];

              if (deviceInfo.hasData &&
                  deviceName != null &&
                  deviceImageUrl != null) {
                return Column(
                    mainAxisAlignment: MainAxisAlignment.center,
                    crossAxisAlignment: CrossAxisAlignment.center,
                    children: [
                      Expanded(
                          child: Image.network(deviceImageUrl,
                              fit: BoxFit.contain)),
                      Text(deviceName),
                      // Spacing between device name text and buttons.
                      const SizedBox(height: 20),
                      Row(
                        mainAxisAlignment: MainAxisAlignment.start,
                        children: [
                          // Spacing between left edge of screen and first button.
                          const SizedBox(width: 20),
                          OutlinedButton(
                            // Invoke pairing dialog.
                            onPressed: () => pairing(context),
                            child: const Text('Pair'),
                          ),
                          // Spacing between buttons.
                          const SizedBox(width: 20),
                          OutlinedButton(
                              onPressed: () => api.dismiss(),
                              child: const Text('Dismiss'))
                        ],
                      ),
                      // Spacing between buttons and bottom of screen.
                      const SizedBox(height: 20),
                    ]);
              }
              return const Center(
                child: CircularProgressIndicator(),
              );
            },
          ),
        ),
      );
}

// Displays pairing dialog box.
Future<String?> pairing(BuildContext context) => showDialog<String>(
    context: context,
    // Rust functions are invoked as futures.
    builder: (context) => FutureBuilder(
        future: api.pair(),
        builder: (context, pairResult) {
          var pairResultValue = pairResult.data;

          return pairResult.hasData && pairResultValue != null
              ? AlertDialog(
                  title: const Text('Pairing result'),
                  content: Text(pairResultValue),
                  actions: <Widget>[
                    TextButton(
                      onPressed: () => Navigator.pop(context, 'OK'),
                      child: const Text('OK'),
                    )
                  ],
                )
              : const AlertDialog(
                  title: Text('Pairing...'),
                  // Ensures the progress indicator has sensible dimensions,
                  // otherwise it follows the height/width of the alert dialog.
                  content: Column(
                    mainAxisAlignment: MainAxisAlignment.center,
                    mainAxisSize: MainAxisSize.min,
                    children: <Widget>[
                      SizedBox(
                        width: 50,
                        height: 50,
                        child: CircularProgressIndicator(),
                      ),
                    ],
                  ),
                );
        }));
