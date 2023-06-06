
### Nearby SDK for embedded systems release notes

## v1.1.0-embedded-RC1
Support GFPS 3.2 specification per [specification](https://developers.google.com/nearby/fast-pair/specifications/introduction).

New features:
* Smart Audio Source Switching (SASS)
* Two byte salt for improved security

Other changes:
* Added optional encryption modules based on mbedtls.
* Fixed build issues.
* More verbose and readable logs.

Supported features include:
* Initial pairing with discoverable advertisement
* Subsequent pairing with non-discoverable advertisement
* Retroactive Active Key for provisioning after manual pairing
* Battery Notification
* Personalized Name
* RFCOMM message stream
* Unit-tests on a simulated gLinux platform implementation 

## v1.0.0-embedded
Initial release supporting GFPS 3.1 per [specification](https://developers.google.com/nearby/fast-pair/specifications/introduction).

Supported features include:
* Initial pairing with discoverable advertisement
* Subsequent pairing with non-discoverable advertisement
* Retroactive Active Key for provisioning after manual pairing
* Battery Notification
* Personalized Name
* RFCOMM message stream
* Unit-tests on a simulated gLinux platform implementation 
