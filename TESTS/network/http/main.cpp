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

/** @file fopen.cpp Test cases to POSIX file fopen() interface.
 *
 * Please consult the documentation under the test-case functions for
 * a description of the individual test case.
 */

#include "mbed.h"

#include "utest/utest.h"
#include "unity/unity.h"
#include "greentea-client/test_env.h"

#include <string>

using namespace utest::v1;

//#include "certificate_aws_s3.h"

#define MAX_RETRIES 3

#ifdef MBED_CONF_DOWNLOAD_TEST_URL_HOST
  const char dl_host[] = MBED_CONF_DOWNLOAD_TEST_URL_HOST;
#else
  const char dl_host[] = "armmbed.github.io";
#endif
#ifdef MBED_CONF_DOWNLOAD_TEST_URL_PATH
  const char dl_path[] = MBED_CONF_DOWNLOAD_TEST_URL_PATH;
#else
  const char dl_path[] = "/mbed-test-files/elizabeth.txt";
#endif
#ifdef MBED_CONF_DOWNLOAD_TEST_FILENAME
  #include MBED_CONF_DOWNLOAD_TEST_FILENAME
#else
  #include "elizabeth.h"
#endif

static volatile bool event_fired = false;

NetworkInterface* interface = NULL;

const char part1[] = "GET ";
const char part2[] = " HTTP/1.1\nHost: ";
const char part3[] = "\n\n";

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

static void socket_event(void) {
    event_fired = true;
}

void download(size_t size) {
    int result = -1;

    /* setup TCP socket */
    TCPSocket* tcpsocket = new TCPSocket(interface);
    TEST_ASSERT_NOT_NULL_MESSAGE(tcpsocket, "failed to instantiate TCPSocket");

    for (int tries = 0; tries < MAX_RETRIES; tries++) {
        result = tcpsocket->connect(dl_host, 80);
        if (result == 0) {
            break;
        }
        printf("[INFO] Connection failed. Retry %d of %d\r\n", tries, MAX_RETRIES);
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, result, "failed to connect");

    tcpsocket->set_blocking(false);
    printf("[INFO] Non-blocking socket mode set\r\n");

    tcpsocket->sigio(socket_event);
    printf("[INFO] Registered socket callback function\r\n");

    /* setup request */
    size_t request_size = strlen(part1) + strlen(dl_path) + strlen(part2) + strlen(dl_host) + strlen(part3) + 1;
    char *request = new char[request_size]();

    /* construct request */
    memcpy(&request[0], part1, strlen(part1));
    memcpy(&request[strlen(part1)], dl_path, strlen(dl_path));
    memcpy(&request[strlen(part1) + strlen(dl_path)], part2, strlen(part2));
    memcpy(&request[strlen(part1) + strlen(dl_path) + strlen(part2)], dl_host, strlen(dl_host));
    memcpy(&request[strlen(part1) + strlen(dl_path) + strlen(part2) + strlen(dl_host)], part3, strlen(dl_host));

    printf("[INFO] Request header: %s\r\n", request);

    /* send request to server */
    result = tcpsocket->send(request, request_size);
    TEST_ASSERT_EQUAL_INT_MESSAGE(request_size, result, "failed to send");

    /* read response */
    char* receive_buffer = new char[size];
    TEST_ASSERT_NOT_NULL_MESSAGE(receive_buffer, "failed to allocate receive buffer");

    size_t expected_bytes = sizeof(story);
    size_t received_bytes = 0;
    uint32_t body_index = 0;

    /* loop until all expected bytes have been received */
    while (received_bytes < expected_bytes) {
        /* wait for async event */
        while(!event_fired);
        event_fired = false;

        /* loop until all data has been read from socket */
        do {
            result = tcpsocket->recv(receive_buffer, size);
            TEST_ASSERT_MESSAGE((result == NSAPI_ERROR_WOULD_BLOCK) || (result >= 0), "failed to read socket");

            if (result > 0) {
                /* skip HTTP header */
                if (body_index == 0) {
                    std::string header(receive_buffer, result);
                    body_index = header.find("\r\n\r\n");
                    TEST_ASSERT_MESSAGE(body_index != std::string::npos, "failed to find body");

                    /* remove header before comparison */
                    memmove(receive_buffer, &receive_buffer[body_index + 4], result - body_index - 4);

                    TEST_ASSERT_EQUAL_STRING_LEN_MESSAGE(story,
                                                         receive_buffer,
                                                         result - body_index - 4,
                                                         "character mismatch");

                    received_bytes += (result - body_index - 4);
                } else {
                    TEST_ASSERT_EQUAL_STRING_LEN_MESSAGE(&story[received_bytes],
                                                         receive_buffer,
                                                         result,
                                                         "character mismatch");

                    received_bytes += result;
                }

                printf("[INFO] Received bytes: %u\r\n", received_bytes);
            }
        }
        while ((result > 0) && (received_bytes < expected_bytes));
    }

    delete request;
    delete tcpsocket;
    delete[] receive_buffer;

    printf("[INFO] Complete\r\n");
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
    download(1024);

    return CaseNext;
}

static control_t download_2k(const size_t call_count) {
    download(2*1024);

    return CaseNext;
}

static control_t download_4k(const size_t call_count) {
    download(4*1024);

    return CaseNext;
}

static control_t download_8k(const size_t call_count) {
    download(8*1024);

    return CaseNext;
}

static control_t download_16k(const size_t call_count) {
    download(16*1024);

    return CaseNext;
}

static control_t download_32k(const size_t call_count) {
    download(32*1024);

    return CaseNext;
}

utest::v1::status_t greentea_setup(const size_t number_of_cases) {
    GREENTEA_SETUP(30*60, "default_auto");
    return greentea_test_setup_handler(number_of_cases);
}

Case cases[] = {
    Case("Setup network", setup_network),
    Case("Download  1k", download_1k),
    Case("Download  2k", download_2k),
    Case("Download  4k", download_4k),
    Case("Download  8k", download_8k),
    Case("Download 16k", download_16k),
    Case("Download 32k", download_32k),
};

Specification specification(greentea_setup, cases);

int main() {
    Thread thread;
    thread.start(led_thread);
    return !Harness::run(specification);
}
