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

#include "unity/unity.h"
#include "greentea-client/test_env.h"
#include <string>

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

const char part1[] = "GET ";
const char part2[] = " HTTP/1.1\nHost: ";
const char part3[] = "\n\n";

static volatile bool event_fired = false;

DigitalOut led1(LED1);

static void socket_event(void) {
    event_fired = true;
    led1 = true;
}

void download_test(size_t size, NetworkInterface* interface, bool tls_mode, uint32_t thread_id) {
    int result = -1;

    /* setup TCP socket */
    TCPSocket* tcpsocket = new TCPSocket(interface);
    TEST_ASSERT_NOT_NULL_MESSAGE(tcpsocket, "failed to instantiate TCPSocket");

    for (int tries = 0; tries < MAX_RETRIES; tries++) {
        result = tcpsocket->connect(dl_host, 80);
        if (result == 0) {
            break;
        }
        printf("[INFO-%d] Connection failed. Retry %d of %d\r\n", thread_id, tries, MAX_RETRIES);
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, result, "failed to connect");

    tcpsocket->set_blocking(false);
    printf("[INFO-%d] Non-blocking socket mode set\r\n", thread_id);

    tcpsocket->sigio(socket_event);
    printf("[INFO-%d] Registered socket callback function\r\n", thread_id);

    /* setup request */
    size_t request_size = strlen(part1) + strlen(dl_path) + strlen(part2) + strlen(dl_host) + strlen(part3) + 1;
    char *request = new char[request_size]();

    /* construct request */
    memcpy(&request[0], part1, strlen(part1));
    memcpy(&request[strlen(part1)], dl_path, strlen(dl_path));
    memcpy(&request[strlen(part1) + strlen(dl_path)], part2, strlen(part2));
    memcpy(&request[strlen(part1) + strlen(dl_path) + strlen(part2)], dl_host, strlen(dl_host));
    memcpy(&request[strlen(part1) + strlen(dl_path) + strlen(part2) + strlen(dl_host)], part3, strlen(dl_host));

    printf("[INFO-%d] Request header: %s\r\n", thread_id, request);

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
        led1 = false;

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

                printf("[INFO-%d] Received bytes: %u\r\n", thread_id, received_bytes);
            }
        }
        while ((result > 0) && (received_bytes < expected_bytes));
    }

    delete request;
    delete tcpsocket;
    delete[] receive_buffer;

    printf("[INFO-%d] Complete\r\n", thread_id);
}

void download_test(size_t size, NetworkInterface* interface, bool tls_mode) {
    return download_test(size, interface, tls_mode, 0);
}

void download_test(size_t size, NetworkInterface* interface) {
    return download_test(size, interface, false, 0);
}
