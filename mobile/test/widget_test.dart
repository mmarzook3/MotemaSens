import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:motemasens_mobile/main.dart';

void main() {
  testWidgets('shows the ESP32 control dashboard', (tester) async {
    tester.view.physicalSize = const Size(1080, 2340);
    tester.view.devicePixelRatio = 3;
    await tester.pumpWidget(const MotemaSensApp());

    expect(find.text('MotemaSens Control'), findsOneWidget);
    expect(find.text('Device connection'), findsOneWidget);
    await tester.drag(find.byType(ListView).first, const Offset(0, -450));
    await tester.pumpAndSettle();
    expect(find.text('Green LED'), findsWidgets);
    expect(find.text('Blue LED'), findsWidgets);
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
  });

  testWidgets('can toggle demo LED control', (tester) async {
    tester.view.physicalSize = const Size(1080, 2340);
    tester.view.devicePixelRatio = 3;
    await tester.pumpWidget(const MotemaSensApp());

    await tester.drag(find.byType(ListView).first, const Offset(0, -450));
    await tester.pumpAndSettle();
    expect(tester.widget<Switch>(find.byType(Switch).first).value, isFalse);

    await tester.tap(find.byType(Switch).first);
    await tester.pumpAndSettle();

    expect(tester.widget<Switch>(find.byType(Switch).first).value, isTrue);
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
  });
}
