# Storage-IO-Flight-Recorder PowerShell SDK

PowerShell7.4+ Windows x64. `Install-Module StorageIOFlightRecorder -Scope CurrentUser`.

```powershell
$client=Connect-StorageIOFlightRecorderSDK
try { Invoke-StorageIOFlightRecorderSDK -Client $client -Operation status } finally { Disconnect-StorageIOFlightRecorderSDK -Client $client }
```

Use -Arguments with a hashtable matching [schema](docs/API.md). Native errors retain their Windows code through the inner exception. Matching driver installed separately.
