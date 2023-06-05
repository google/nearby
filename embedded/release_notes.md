### Nearby SDK for embedded systems release notes

## v2.2.0-embedded
Support GFPS 3.2 specification per [specification](https://developers.google.com/nearby/fast-pair/specifications/introduction).

New features:
* Find My Device Network added
* Added micro-ecc component

Other changes:
* Merged all feature to main.
* Fixed all the build issues.
* Guarded the features with Macros.

## v1.1.0-embedded RC1
Initial release supporting GFPS 3.2 per [specification](https://developers.google.com/nearby/fast-pair/specifications/introduction).

Supported features include:
* Initial pairing with discoverable advertisement
* Subsequent pairing with non-discoverable advertisement
* Retroactive Active Key for provisioning after manual pairing
* Battery Notification
* Personalized Name
* RFCOMM message stream
* Unit-tests on a simulated gLinux platform implementation 
* Smart Audio Source Switching (SASS)
* Two byte salt for improved security

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
