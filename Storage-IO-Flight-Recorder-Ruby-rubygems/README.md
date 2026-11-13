# Storage-IO-Flight-Recorder Ruby SDK

`gem install storage-io-flight-recorder`

Windows x64 Ruby3.1+.

```ruby
require "storage_io_flight_recorder"
StorageIOFlightRecorderSDK::Client.open { |client| p client.call("status") }
```

[API and operation schema](docs/API.md). Matching driver installed separately.
