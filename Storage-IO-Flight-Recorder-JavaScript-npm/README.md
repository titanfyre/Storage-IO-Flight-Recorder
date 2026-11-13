# Storage-IO-Flight-Recorder Node.js / TypeScript SDK

`npm install storage-io-flight-recorder`

Windows x64 Node20+, prebuilt Node-API8 addon and product DLL.

```js
const {Client}=require("storage-io-flight-recorder");
const client=new Client();
try {console.log(client.call("status"));}finally{client.close();}
```

Call is synchronous; Calls use the native operation deadlines. Run client in a worker for responsive UI/event loops. NativeError exposes code. [Operations and schema](docs/API.md). Matching driver and original product licensing required separately.
