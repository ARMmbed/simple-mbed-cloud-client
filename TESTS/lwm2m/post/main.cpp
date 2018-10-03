#include "mbed.h"
#include "FATFileSystem.h"
#include "simple-mbed-cloud-client.h"

#include "utest/utest.h"
#include "unity/unity.h"
#include "greentea-client/test_env.h"

using namespace utest::v1;

// Default storage definition.
BlockDevice* bd = BlockDevice::get_default_instance();
FATFileSystem fs("sd", bd);

static const ConnectorClientEndpointInfo* endpointInfo;
void registered(const ConnectorClientEndpointInfo *endpoint) {
    printf("[INFO] Connected to Pelion Device Management. Device ID: %s\n",
            endpoint->internal_endpoint_name.c_str());
    endpointInfo = endpoint;
}

void post_test_callback(MbedCloudClientResource *resource, const uint8_t *buffer, uint16_t size) {
    printf("[INFO] POST test callback executed. \r\n");
    greentea_send_kv("device_lwm2m_post_test_result", 0);
}

void smcc_lwm2m_post(void) {

    int timeout = 0;
    int result = -1;
    bool test_result = true;
    char _key[20] = { };
    char _value[128] = { };

    // Connection definition.
#if MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE == ETHERNET
    NetworkInterface *net = NetworkInterface::get_default_instance();
    nsapi_error_t status = net->connect();
#elif MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE == WIFI
    WiFiInterface *net = WiFiInterface::get_default_instance();
    nsapi_error_t status = net->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
#elif MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE == CELLULAR
    CellularBase *net = CellularBase::get_default_instance();
    net->set_sim_pin(MBED_CONF_NSAPI_DEFAULT_CELLULAR_SIM_PIN);
    net->set_credentials(MBED_CONF_NSAPI_DEFAULT_CELLULAR_APN, MBED_CONF_NSAPI_DEFAULT_CELLULAR_USERNAME, MBED_CONF_NSAPI_DEFAULT_CELLULAR_PASSWORD);
    nsapi_error_t status = net->connect();
#elif MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE == MESH
    MeshInterface *net = MeshInterface::get_default_instance();
    nsapi_error_t status = net->connect();
#else
    #error "Default network interface not defined"
#endif

    // Must have IP address.
    TEST_ASSERT_NOT_EQUAL(net->get_ip_address(), NULL);
    if (net->get_ip_address() == NULL) {
        printf("[ERROR] No IP address obtained from network.\r\n");
        test_result = false;
        greentea_send_kv("fail_test", 0);
    }

    // Connection must be successful.
    TEST_ASSERT_EQUAL(status, 0);
    if (status == 0 && net->get_ip_address() != NULL) {
        printf("[INFO] Connected to network successfully. IP address: %s\n", net->get_ip_address());
    } else {
        printf("[ERROR] Failed to connect to network.\r\n");
        test_result = false;
        greentea_send_kv("fail_test", 0);
    }

    SimpleMbedCloudClient client(net, bd, &fs);

    // SimpleMbedCloudClient initialization must be successful.
    int client_status = client.init();
    TEST_ASSERT_EQUAL(client_status, 0);
    if (client_status == 0) {
        printf("[INFO] Simple Mbed Cloud Client initialization successful. \r\n");
    } else {
        printf("[ERROR] Simple Mbed Cloud Client failed to initialize.\r\n");
        test_result = false;
        greentea_send_kv("fail_test", 0);
    }

    //Create LwM2M resources
    MbedCloudClientResource *res_post_test;
    res_post_test = client.create_resource("5000/0/3", "post_resource");
    res_post_test->methods(M2MMethod::POST);
    res_post_test->attach_post_callback(post_test_callback);

    client.on_registered(&registered);
    client.register_and_connect();

    timeout = 5000;
    while (timeout && !client.is_client_registered()) {
        timeout--;
        wait_ms(1);
    }

    // Registration to Mbed Cloud must be successful.
    TEST_ASSERT_TRUE(client.is_client_registered());
    if (!client.is_client_registered()) {
        printf("[ERROR] Device failed to register.\r\n");
        test_result = false;
        greentea_send_kv("fail_test", 0);
    } else {
        printf("[INFO] Simple Mbed Cloud Client successfully registered to Mbed Cloud.\r\n");
    }

    // Allow 500ms for Mbed Cloud to update the device directory.
    timeout = 500;
    while (timeout) {
        timeout--;
        wait_ms(1);
    }

    // LwM2M tests
    printf("[INFO] Beginning LwM2M resource POST test.\r\n");

    printf("[INFO] Executing POST on /5000/0/3 and waiting for callback function.");
    greentea_send_kv("device_lwm2m_post_test", endpointInfo->internal_endpoint_name.c_str());
    greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));

    // Ensure that callback is executed. If not, test fails.
    result = atoi(_value);
    TEST_ASSERT_EQUAL(0, result);
    if (result != 0) {
        printf("[ERROR] POST callback execution failed on resource /5000/0/3. \r\n");
        test_result = false;
        greentea_send_kv("fail_test", 0);
    } else {
        printf("[INFO] POST callback executed successfully /5000/0/3 \r\n");
    }

    GREENTEA_TESTSUITE_RESULT(test_result);
}

int main(void) {
    GREENTEA_SETUP(150, "sdk_host_tests");
    smcc_lwm2m_post();

    return 0;
}
