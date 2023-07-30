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
      appBar: AppBar(title: const Text("Fast Pair")),
      body: Center(
        child: StreamBuilder(
          // All Rust functions are called as Future's
          stream: api.eventStream(), // The Rust function we are calling.
          builder: (context, data) {
            if (data.hasData) {
              return Text(data.data!); // The string to display
            }
            return const Center(
              child: CircularProgressIndicator(),
            );
          },
        ),
      ));
}
