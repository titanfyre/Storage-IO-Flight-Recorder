# Storage-IO-Flight-Recorder Python SDK

`pip install storage-io-flight-recorder`

Windows x64 Python3.10+. No driver is installed by this package.

```python
from storage_io_flight_recorder import Client
with Client() as client:
    info = client.call("status")
```

[Operation schema and API](docs/API.md). Check errors and the product licensing state before device creation.
