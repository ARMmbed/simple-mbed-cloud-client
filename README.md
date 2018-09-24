# Simple Pelion Device Management Client

(aka Simple Mbed Cloud Client)

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

2. Add your Mbed Cloud developer certificate to your project (`mbed_cloud_dev_credentials.c` file).

3. Reference the library from your main.cpp file, add network and storage drivers; finally initialize the Simple Pelion Client library. The is the architecture of a generic Simple Pelion Client application:

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

        /* Initialize Simple Pelion Client */
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

  There are a number of applications that make usage of the Simple Pelion Client library.

  The Mbed Cloud [Quick-Start](https://cloud.mbed.com/quick-start) is an initiative to support Mbed Partner's platforms while delivering a great User Experience to Mbed Developers.

## Testing

Simple Pelion Client provides Greentea tests to test your porting efforts.

### Tests

| **Test Name** | **Description** |
| ------------- | ------------- |
| `simple-connect` | Tests that the device successfully registers to Mbed Cloud using the specified storage, SOTP, and connectivity configuration. Tests that SOTP and the RoT is preserved over a reset and the device connects with a consistent device ID.  |

### Requirements
 Simple Pelion Client tests rely on the Python SDK to test the end to end solution.
 To install the Python SDK:
`pip install mbed-cloud-sdk`
 **Note:** The Python SDK requires Python 2.7.10+ / Python 3.4.3+, built with SSL support.

 ### Setup

 1. Include the `mbed_cloud_dev_credentials.c` developer certificate in your application. For detailed instructions [see the documentation](https://cloud.mbed.com/docs/current/connecting/provisioning-development-devices.html#creating-and-downloading-a-developer-certificate).

 2. Set an environment variable on the host machine called `MBED_CLOUD_API_KEY` which is valid for the account that your device will connect to. For instructions on how to generate an API key, please [see the documentation](https://cloud.mbed.com/docs/current/integrate-web-app/api-keys.html#generating-an-api-key).

 3. If your platform does not have a default network interface in `targets.json`, you can define this in `mbed_app.json`, for example:
 ```json
"target.network-default-interface-type": "ETHERNET",
```

1. If your platform does not have a default storage component in `targets.json`, you can define this in `mbed_app.json`. Aditionally, you may want to double-check the storage pinout configuration for your platform in the `mbed_lib.json` file in `mbed-os/components/storage/blockdevice/`.
   
For examples of platform configuration, see the applications available in the [Quick-start](https://cloud.mbed.com/quick-start).

1. You may need to delete your `main.cpp`.

2. Run the Simple Pelion Client tests from the application directory:
 ` mbed test -t <toolchain> -m <platform> --app-config mbed_app.json -n simple-mbed-cloud-client-tests-*`

### Troubleshooting
Below are a list of common issues and fixes for using Simple Pelion Client.

#### Autoformatting failed with error -5005
This is due to an issue with the storage block device. If using an SD card, ensure that the SD card is seated properly.

#### SYNC_FAILED during testing
Occasionally, if the test failed during a previous attempt, the SMCC Greentea tests will fail to sync. If this is the case, please replug your device to the host PC. Additionally, you may need to update your DAPLink or ST-Link interface firmware.

#### Device identity is inconsistent
If your device ID in Mbed Cloud is inconsistent over a device reset, it could be because it is failing to open the credentials on the storage held in the Enhanced Secure File System. Typically, this is because the device cannot access the Root of Trust stored in SOTP.

One way to verify this is to see if Simple Pelion Client autoformats the storage after a device reset when `format-storage-layer-on-error` is set to `1` in `mbed_app.json`.  It would appear on the serial terminal output from the device as the following:
```
[SMCC] Initializing storage.
[SMCC] Autoformatting the storage.
[SMCC] Reset storage to an empty state.
[SMCC] Starting developer flow
```

When this occurs, you should look at the SOTP sectors defined in `mbed_app.json`:
```
"sotp-section-1-address"           : "0xFE000",
"sotp-section-1-size"              : "0x1000",
"sotp-section-2-address"           : "0xFF000",
"sotp-section-2-size"              : "0x1000",
```
Ensure that the sectors are correct according to the flash layout of your device, and they are not being overwritten during the programming of the device. ST-Link devices will overwrite these sectors when using drag-and-drop of .bin files. Thus, moving the SOTP sectors to the end sectors of flash ensure that they will not be overwritten.

#### Stack Overflow
If you receive a stack overflow error, increase the Mbed OS main stack size to at least 6000. This can be done by modifying the following parameter in `mbed_app.json`:
```
 "MBED_CONF_APP_MAIN_STACK_SIZE=6000",
```

#### Device failed to register
Check the device allocation on your Mbed Cloud account to see if you are allowed additional devices to connect. You can delete development devices, after being deleted they will not count towards your allocation.


### Known issues

None
