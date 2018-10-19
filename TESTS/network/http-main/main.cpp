/*
 * mbed Microcontroller Library
 * Copyright (c) 2006-2016 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if !defined(MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE)
#error [NOT_SUPPORTED] No network interface found on this target.
#endif

#if MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE != ETHERNET && \
    MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE != WIFI && \
    MBED_CONF_TARGET_NETWORK_DEFAULT_INTERFACE_TYPE != CELLULAR
#error [NOT_SUPPORTED] Either WiFi, Ethernet or Cellular network interface need to be enabled
#endif

#include "mbed.h"

#include "utest/utest.h"
#include "unity/unity.h"
#include "greentea-client/test_env.h"
using namespace utest::v1;
#include "download_test.h"
#include <string>

DigitalOut led2(LED2);
NetworkInterface* interface = NULL;

void led_heartbeat() {
    led2 = 0;
    while (true) {
        led2 = !led2;
        wait(0.5);
    }
}

static control_t setup_network(const size_t call_count) {
    interface = NetworkInterface::get_default_instance();
    TEST_ASSERT_NOT_NULL_MESSAGE(interface, "failed to initialize network");

    nsapi_error_t err = interface->connect();
    TEST_ASSERT_EQUAL(NSAPI_ERROR_OK, err);
    printf("[INFO] IP address is '%s'\n", interface->get_ip_address());
    printf("[INFO] MAC address is '%s'\n", interface->get_mac_address());
    return CaseNext;
}

static control_t download_1k(const size_t call_count) {
    download_test(1024, interface);
    return CaseNext;
}

static control_t download_2k(const size_t call_count) {
    download_test(2*1024, interface);
    return CaseNext;
}

static control_t download_4k(const size_t call_count) {
    download_test(4*1024, interface);
    return CaseNext;
}

static control_t download_8k(const size_t call_count) {
    download_test(8*1024, interface);
    return CaseNext;
}

static control_t download_16k(const size_t call_count) {
    download_test(16*1024, interface);
    return CaseNext;
}

static control_t download_32k(const size_t call_count) {
    download_test(32*1024, interface);
    return CaseNext;
}

utest::v1::status_t greentea_setup(const size_t number_of_cases) {
    GREENTEA_SETUP(5*60, "default_auto");
    return greentea_test_setup_handler(number_of_cases);
}

Case cases[] = {
    Case("Setup network", setup_network),
    Case("Download  1k recv buffer", download_1k),
    Case("Download  2k recv buffer", download_2k),
    Case("Download  4k recv buffer", download_4k),
    Case("Download  8k recv buffer", download_8k),
    Case("Download 16k recv buffer", download_16k),
    Case("Download 32k recv buffer", download_32k),
};

Specification specification(greentea_setup, cases);

int main() {
    Thread thread;
    thread.start(led_heartbeat);
    return !Harness::run(specification);
}
