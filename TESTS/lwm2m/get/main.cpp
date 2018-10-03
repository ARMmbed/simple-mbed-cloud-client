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

void smcc_lwm2m_get(void) {

    int timeout = 0;
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
    MbedCloudClientResource *res_get_test;
    res_get_test = client.create_resource("5000/0/1", "get_resource");
    res_get_test->observable(true);
    res_get_test->methods(M2MMethod::GET);
    res_get_test->set_value("test0");

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
    printf("[INFO] Beginning LwM2M resource GET test.\r\n");

    // Read original value of /5000/0/1
    greentea_send_kv("device_lwm2m_get_test", endpointInfo->internal_endpoint_name.c_str());
    greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));
    TEST_ASSERT_EQUAL_STRING("test0", _value);
    if (strcmp(_value, "test0") != 0) {
        printf("[ERROR] Wrong value reported in Pelion DM.\r\n");
        test_result = false;
        greentea_send_kv("fail_test", 0);
    } else {
        printf("[INFO] Original value of LwM2M resource /5000/0/1 is read correctly. \r\n");
    }

    // Update resource /5000/0/1 from client and observe value
    greentea_send_kv("device_lwm2m_get_test", endpointInfo->internal_endpoint_name.c_str());
    res_get_test->set_value("test1");
    greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));
    TEST_ASSERT_EQUAL_STRING("test1", _value);
    if (strcmp(_value, "test1") != 0) {
        printf("[ERROR] Wrong value observed from Pelion DM.\r\n");
        test_result = false;
        greentea_send_kv("fail_test", 0);
    } else {
        printf("[INFO] Changed value of LwM2M resource /5000/0/1 is observed correctly. \r\n");
    }

    GREENTEA_TESTSUITE_RESULT(test_result);

}

int main(void) {
    GREENTEA_SETUP(150, "sdk_host_tests");
    smcc_lwm2m_get();

    return 0;
}
