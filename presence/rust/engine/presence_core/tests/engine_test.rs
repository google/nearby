use std::sync::mpsc;
use std::thread;

use presence_core::ble_scan_provider::{BleScanner, PresenceScanResult, ScanRequest};
use presence_core::client_provider::{
    DiscoveryCallback, DiscoveryResult, PresenceDiscoveryCondition, PresenceDiscoveryRequest,
    PresenceIdentityType, PresenceMeasurementAccuracy, PresenceMedium,
};
use presence_core::PresenceEngine;

const EXPECTED_PRIORITY: i32 = 99;
const EXPECTED_ACTION: i32 = 100;

struct MockDiscoveryCallback {
    pub discovery_result_tx: mpsc::Sender<DiscoveryResult>,
}

impl DiscoveryCallback for MockDiscoveryCallback {
    fn on_device_update(&self, result: DiscoveryResult) {
        println!("DiscoveryCallback on device update.");
        self.discovery_result_tx.send(result).unwrap();
    }
}

struct MockBleScanner {
    pub scan_request_tx: mpsc::Sender<ScanRequest>,
}

impl BleScanner for MockBleScanner {
    fn start_ble_scan(&self, request: ScanRequest) {
        println!("BleScanner start ble scan.");
        self.scan_request_tx.send(request).unwrap();
    }
}

#[test]
fn test_engine() {
    println!("test engine starts.");
    let (scan_request_tx, scan_request_rx) = mpsc::channel();
    let (discovery_result_tx, discovery_result_rx) = mpsc::channel();
    let mut presence_engine = PresenceEngine::new(
        Box::new(MockDiscoveryCallback {
            discovery_result_tx,
        }),
        Box::new(MockBleScanner { scan_request_tx }),
    );

    thread::scope(|scope| {
        let engine_thread = scope.spawn(|| presence_engine.engine.run());

        let condition = PresenceDiscoveryCondition {
            action: EXPECTED_ACTION,
            identity_type: PresenceIdentityType::Private,
            measurement_accuracy: PresenceMeasurementAccuracy::Unknown,
        };
        let request = PresenceDiscoveryRequest::new(
            EXPECTED_PRIORITY, /* priority */
            Vec::from([condition]),
        );
        println!("test engine: set discovery request.");
        presence_engine
            .client_provider
            .set_discovery_request(request);

        let request = scan_request_rx.recv().unwrap();
        println!("received request {:?}", request);
        assert_eq!(request.priority, EXPECTED_PRIORITY);
        assert_eq!(request.actions.len(), 1);
        assert_eq!(request.actions[0], EXPECTED_ACTION);

        let scan_result =
            PresenceScanResult::new(PresenceMedium::BLE, Vec::from([EXPECTED_ACTION]));
        presence_engine
            .ble_scan_callback
            .on_scan_result(scan_result);

        let discovery_result = discovery_result_rx.recv().unwrap();
        println!("received discovery result {:?}", discovery_result);
        assert_eq!(discovery_result.medium, PresenceMedium::BLE);
        assert_eq!(discovery_result.device.actions.len(), 1);
        assert_eq!(discovery_result.device.actions[0], EXPECTED_ACTION);

        presence_engine.client_provider.stop();
        engine_thread
            .join()
            .expect("Engine integration test crashed.");
    });
}
