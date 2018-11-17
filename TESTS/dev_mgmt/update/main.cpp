#include "mbed.h"
#include "FATFileSystem.h"
#include "simple-mbed-cloud-client.h"
#include "greentea-client/test_env.h"

DigitalOut led1(LED1);
DigitalOut led2(LED2);
void led_thread() {
    led1 = 1;
    led2 = 0;
    while (true) {
        led1 = !led1;
        led2 = !led2;
        wait(0.5);
    }
}
RawSerial pc(USBTX, USBRX);

// Default storage definition.
BlockDevice* bd = BlockDevice::get_default_instance();
FATFileSystem fs("fs", bd);

void wait_nb(uint16_t ms) {
    while (ms > 0) {
        ms--;
        wait_ms(1);
    }
}

void logger(const char* message, const char* decor) {
    wait_nb(10);
    pc.printf(message, decor);
    wait_nb(10);
}
void logger(const char* message) {
    wait_nb(10);
    pc.printf(message);
    wait_nb(10);
}

static const ConnectorClientEndpointInfo* endpointInfo;
void registered(const ConnectorClientEndpointInfo *endpoint) {
    logger("[INFO] Connected to Pelion Device Management. Device ID: %s\n",
            endpoint->internal_endpoint_name.c_str());
    endpointInfo = endpoint;
}

void spdmc_testsuite_connect(void) {
    int iteration = 0;
    char _key[20] = { };
    char _value[128] = { };

    greentea_send_kv("spdmc_ready_chk", true);
    while (1) {
        greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));

        if (strcmp(_key, "iteration") == 0) {
            iteration = atoi(_value);
            break;
        }
    }

    // provide manifest to greentea so it can correct show skipped and failed tests
    if (iteration == 0) {
        greentea_send_kv(GREENTEA_TEST_ENV_TESTCASE_COUNT, 10);
        greentea_send_kv(GREENTEA_TEST_ENV_TESTCASE_NAME, "Connect to Network");
        greentea_send_kv(GREENTEA_TEST_ENV_TESTCASE_NAME, "Format Storage");
        greentea_send_kv(GREENTEA_TEST_ENV_TESTCASE_NAME, "Simple PDMC Initialization");
        greentea_send_kv(GREENTEA_TEST_ENV_TESTCASE_NAME, "Pelion DM Bootstrap & Reg.");
        greentea_send_kv(GREENTEA_TEST_ENV_TESTCASE_NAME, "Pelion DM Re-register");
        greentea_send_kv(GREENTEA_TEST_ENV_TESTCASE_NAME, "Pelion DM Directory");
        greentea_send_kv(GREENTEA_TEST_ENV_TESTCASE_NAME, "Prepare Firmware");
        greentea_send_kv(GREENTEA_TEST_ENV_TESTCASE_NAME, "Download Firmware");
        greentea_send_kv(GREENTEA_TEST_ENV_TESTCASE_NAME, "Consistent Identity");
    }

    // Start network connection test.
    GREENTEA_TESTCASE_START("Connect to Network");
    logger("[INFO] Attempting to connect to network.\r\n");

    // Connection definition.
    NetworkInterface *net = NetworkInterface::get_default_instance();
    nsapi_error_t net_status = net->connect();

    // Report status to console.
    if (net_status != 0) {
        logger("[ERROR] Device failed to connect to Network.\r\n");
        greentea_send_kv("test_failed", 0);
    } else {
        logger("[INFO] Connected to network successfully. IP address: %s\n", net->get_ip_address());
    }

    GREENTEA_TESTCASE_FINISH("Connect to Network", (net_status == 0), (net_status != 0));

    // Instantiate SimpleMbedCloudClient.
    SimpleMbedCloudClient client(net, bd, &fs);

    // This must be done on the first iteration to ensure that we can test writing of new credentials. It may
    // happen twice if the reset storage flag is set to 1.
    if (iteration == 0) {
        logger("[INFO] Resetting storage to a clean state for test.\n");

        GREENTEA_TESTCASE_START("Format Storage");
        //int storage_status = 0;
        int storage_status = client.reformat_storage();

        // Report status to console.
        if (storage_status == 0) {
            logger("[INFO] Storage format successful.\r\n");
        } else {
            logger("[ERROR] Storage format failed.\r\n");
            greentea_send_kv("test_failed", 0);
        }

        GREENTEA_TESTCASE_FINISH("Format Storage", (storage_status == 0), (storage_status != 0));
    }

    // SimpleMbedCloudClient initialization must be successful.
    GREENTEA_TESTCASE_START("Simple PDMC Initialization");
    int client_status = client.init();

    // Report status to console.
    if (client_status == 0) {
        logger("[INFO] Simple Mbed Cloud Client initialization successful.\r\n");
    } else {
        logger("[ERROR] Simple Mbed Cloud Client failed to initialize.\r\n");
        // End the test early, cannot continue without successful cloud client initialization.
        greentea_send_kv("test_failed", 0);
    }

    GREENTEA_TESTCASE_FINISH("Simple PDMC Initialization", (client_status == 0), (client_status != 0));

    //Create LwM2M resources
    MbedCloudClientResource *res_get_test;
    res_get_test = client.create_resource("5000/0/1", "get_resource");
    res_get_test->observable(true);
    res_get_test->methods(M2MMethod::GET);
    res_get_test->set_value("test0");

    // Set client callback to report endpoint name.
    client.on_registered(&registered);

    // Register to Pelion Device Management.
    if (iteration == 0) {
        GREENTEA_TESTCASE_START("Pelion DM Bootstrap & Reg.");
    } else {
        GREENTEA_TESTCASE_START("Pelion DM Re-register");
    }
    client.register_and_connect();

    int timeout = 30000;
    while (timeout && !client.is_client_registered()) {
        timeout--;
        wait_ms(1);
    }

    // Get registration status.
    bool client_registered = client.is_client_registered();
    if (client_registered) {
        client_status = 0;
        wait_nb(100);
        logger("[INFO] Device successfully registered to Pelion DM.\r\n");
    } else {
        client_status = -1;
        logger("[ERROR] Device failed to register.\r\n");
        greentea_send_kv("test_failed", 0);
    }
    if (iteration == 0) {
        GREENTEA_TESTCASE_FINISH("Pelion DM Bootstrap & Reg.", (client_status == 0), (client_status != 0));
    } else {
        GREENTEA_TESTCASE_FINISH("Pelion DM Re-register", (client_status == 0), (client_status != 0));
    }

    if (iteration == 0) {
        //Start registration status test
        GREENTEA_TESTCASE_START("Pelion DM Directory");
        int reg_status;

        logger("[INFO] Wait 5 seconds for Device Directory to update after initial registration.\r\n");
        wait_nb(5000);

        // Start host tests with device id
        logger("[INFO] Starting Pelion DM verification using Python SDK...\r\n");
        greentea_send_kv("verify_registration", endpointInfo->internal_endpoint_name.c_str());
        while (1) {
            greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));

            if (strcmp(_key, "registered") == 0) {
                if (atoi(_value)) {
                    reg_status = 0;
                    logger("[INFO] Device is registered in the Device Directory.\r\n");
                } else {
                    reg_status = -1;
                    logger("[ERROR] Device could not be verified as registered in Device Directory.\r\n");
                    greentea_send_kv("test_failed", 0);
                }
                break;
            }
        }

        GREENTEA_TESTCASE_FINISH("Pelion DM Directory", (reg_status == 0), (reg_status != 0));

        if (reg_status == 0) {
            GREENTEA_TESTCASE_START("Prepare Firmware");
            wait_nb(500);
            int fw_status;
            greentea_send_kv("firmware_prepare", 1);
            while (1) {
                greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));
                if (strcmp(_key, "firmware_sent") == 0) {
                    if (atoi(_value)) {
                        fw_status = 0;
                    } else {
                        fw_status = -1;
                        logger("[ERROR] While preparing firmware.\r\n");
                    }
                    break;
                }
            }
            GREENTEA_TESTCASE_FINISH("Prepare Firmware", (fw_status == 0), (fw_status != 0));

            GREENTEA_TESTCASE_START("Download Firmware");
            logger("[INFO] Update campaign has started.\r\n");
            // The device should download firmware and reset at this stage
        }

        while (1) {
            wait_nb(1000);
        }
    } else {
        //Start consistent identity test.
        GREENTEA_TESTCASE_FINISH("Download Firmware", true, false);

        GREENTEA_TESTCASE_START("Consistent Identity");
        int identity_status;

        logger("[INFO] Wait 2 seconds for Device Directory to update after reboot.\r\n");
        wait_nb(2000);

        // Wait for Host Test to verify consistent device ID (blocking here)
        logger("[INFO] Verifying consistent Device ID...\r\n");
        greentea_send_kv("verify_identity", endpointInfo->internal_endpoint_name.c_str());
        while (1) {
            greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));

            if (strcmp(_key, "verified") == 0) {
                if (atoi(_value)) {
                    identity_status = 0;
                    logger("[INFO] Device ID consistent, SOTP and Secure Storage is preserved correctly.\r\n");
                } else {
                    identity_status = -1;
                    logger("[ERROR] Device ID is inconsistent. SOTP and Secure Storage was not preserved.\r\n");
                }
                break;
            }
        }

        GREENTEA_TESTCASE_FINISH("Consistent Identity", (identity_status == 0), (identity_status != 0));

        GREENTEA_TESTSUITE_RESULT(identity_status == 0);

        while (1) {
            wait_nb(1000);
        }
    }
}

int main(void) {
    //Create a thread to blink an LED and signal that the device is alive
    Thread thread;
    thread.start(led_thread);

    greentea_send_kv("device_booted", 1);

    GREENTEA_SETUP(300, "sdk_host_tests");
    spdmc_testsuite_connect();

    return 0;
}
