# Simple Mbed Cloud Client

A simple way of connecting Mbed OS 5 devices to Mbed Cloud. It's designed to:

* Enable Mbed Cloud Connect and mbed Cloud Update to applications in few lines of code.
* Run separate from your main application, it does not take over your main loop.
* Provide LWM2M resources, essentially variables that are automatically synced through Mbed Cloud Connect.
* Help users avoid doing blocking network operations in interrupt contexts, by automatically defering actions to a separate thread.
* Provide end to end Greentea tests for Mbed Cloud Client

This library is a simpler interface to Mbed Cloud Client, making it trivial to expose sensors, actuators and other variables to the cloud. For a full Mbed Cloud CLient API, check our [documentation](https://cloud.mbed.com/docs/current/mbed-cloud-client/index.html).

## Usage to Connect to Mbed Cloud

1. Add this library to your Mbed OS project:

    ```
    $ mbed add https://github.com/ARMmbed/simple-mbed-cloud-client
    ```
    
1. Add your Mbed Cloud developer certificate to your project (`mbed_cloud_dev_credentials.c` file).

1. Reference the library from your main.cpp file, add network and storage drivers; finally initialize the Simple Mbed Cloud Client library.

    ```cpp
    #include "simple-mbed-cloud-client.h"
    #include <Block device>
    #include <Filesystem>
    #include <Network>

    int main() {
    
        /* Initialize connectivity */
        <Network> net;
        net.connect();
    
        /* Initialize storage */
        <Block device> sd(...);
        <Filesystem> fs("sd", &sd);

        /* Initialize Simple Mbed Cloud Client */
        SimpleMbedCloudClient client(&net, &sd, &fs);
        client.init();

        /* Create resource */        
        MbedCloudClientResource *variable;
        variable = client.create_resource("3201/0/5853", "variable");
        variable->set_value("assign new value");
        variable->methods(M2MMethod::GET | M2MMethod::PUT);

    }
    ```
   
## Example applications
  
  There are a number of applications that make usage of the Simple Mbed Cloud Client library.
  
  The Mbed Cloud [Quick-Start](https://cloud.mbed.com/quick-start) is an initiative to support Mbed Partner's platforms while delivering a great User Experience to Mbed Developers.  

## Testing

Simple Mbed Cloud Client provides Greentea tests to test your Simple Mbed Cloud Client porting efforts. 

### Tests

| **Test Name** | **Description** |
| ------------- | ------------- |
| `simple-connect` | Tests that the device successfully registers to Mbed Cloud using the specified storage, SOTP, and connectivity configuration. Tests that SOTP and the RoT is preserved over a reset and the device connects with a consistent device ID.  |

### Requirements
 Simple Mbed Cloud Client tests rely on the Python SDK to test the end to end solution. 
 To install the Python SDK:
`pip install mbed-cloud-sdk`
 **Note:** The Python SDK requires Python 2.7.10+ / Python 3.4.3+, built with SSL support.
 
 ### Setup
 
 1. Include the `mbed_cloud_dev_credentials.c` developer certificate in your application. For detailed instructions [see the documentation](https://cloud.mbed.com/docs/current/connecting/provisioning-development-devices.html#creating-and-downloading-a-developer-certificate).
 
 2. Set an environment variable on the host machine called `MBED_CLOUD_API_KEY` which is valid for the account that your device will connect to. For instructions on how to generate an API key, please [see the documentation](https://cloud.mbed.com/docs/current/integrate-web-app/api-keys.html#generating-an-api-key).
 
 3. In your reference application, change the following parameters in `mbed_app.json` to the parameters specific to your platform:
 ```json      
"test-connect-header-file": {
    "help": "Name of socket interface for SMCC tests.",
    "value": "\"EthernetInterface.h\""
},
"test-socket-object": {
    "help": "Instantiation of network interface statement for SMCC tests. (variable name must be net)",
    "value": "EthernetInterface net;"
},
"test-socket-connect": {
    "help": "Network socket connect statement for SMCC tests.",
    "value": "net.connect();"
},
"test-block-device-header-file": {
    "help": "Name of block device for SMCC tests.",
    "value": "\"SDBlockDevice.h\""
},
"test-block-device-object": {
    "help": "Block device instantiation for SMCC tests. (variable name must be bd)",
    "value": "SDBlockDevice bd(MBED_CONF_APP_SPI_MOSI, MBED_CONF_APP_SPI_MISO, MBED_CONF_APP_SPI_CLK, MBED_CONF_APP_SPI_CS);"
}
```
For example, to run the Simple Mbed Cloud Client tests on a `UBLOX_EVK_ODIN_W2`, you would add the following configuration in `target_overrides`:
 ```json
"target_overrides": {
    "UBLOX_EVK_ODIN_W2": {
        "app.sotp-section-1-address"    : "(0x081C0000)",
        "app.sotp-section-1-size"       : "(128*1024)",
        "app.sotp-section-2-address"    : "(0x081E0000)",
        "app.sotp-section-2-size"       : "(128*1024)",
        "test-connect-header-file"      : "\"OdinWiFiInterface.h\"",
        "test-block-device-header-file" : "\"SDBlockDevice.h\"",
        "test-socket-object"            : "OdinWiFiInterface net;",
        "test-socket-connect"           : "net.connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);",
        "test-block-device-object"      : "SDBlockDevice bd(D11, D12, D13, D9);"
    }
}
```
For more examples and a template `mbed_app.json`, see [simple-mbed-cloud-client-template-restricted](https://github.com/ARMmbed/simple-mbed-cloud-client-template-restricted).

4. You may need to delete your `main.cpp`.

5. Run the Simple Mbed Cloud Client tests from the application directory: 
 ` mbed test -m <platform> -t <toolchain> --app-config mbed_app.json -n simple-mbed-cloud-client-tests-*`
 
### Troubleshooting
Below are a list of common issues and fixes for using Simple Mbed Cloud Client.

#### Autoformatting failed with error -5005
This is due to an issue with the storage block device. If using an SD card, ensure that the SD card is seated properly.

#### SYNC_FAILED during testing
Occasionally, if the test failed during a previous attempt, the SMCC Greentea tests will fail to sync. If this is the case, please replug your device to the host PC. Additionally, you may need to update your DAPLink or ST-Link interface firmware.

#### Device identity is inconsistent
If your device ID in Mbed Cloud is inconsistent over a device reset, it could be because it is failing to open the credentials on the storage held in the Enhanced Secure File System. Typically, this is because the device cannot access the Root of Trust stored in SOTP.

One way to verify this is to see if Simple Mbed Cloud Client autoformats the storage after a device reset when `format-storage-layer-on-error` is set to `1` in `mbed_app.json`.  It would appear on the serial terminal output from the device as the following:
```
[Simple Cloud Client] Initializing storage.
[Simple Cloud Client] Autoformatting the storage.
[Simple Cloud Client] Reset storage to an empty state.
[Simple Cloud Client] Starting developer flow
```

When this occurs, you should look at the SOTP sectors defined in `mbed_app.json`:
```
"sotp-section-1-address"           : "0xFE000",
"sotp-section-1-size"              : "0x1000",
"sotp-section-2-address"           : "0xFF000",
"sotp-section-2-size"              : "0x1000",
```
Ensure that the sectors are correct according to the flash layout of your device, and they are not being overwritten during the programming of the device. ST-Link devices will overwrite these sectors when using drag-and-drop of .bin files. Thus, moving the SOTP sectors to the end sectors of flash ensure that they will not be overwritten.

### Known issues

None
