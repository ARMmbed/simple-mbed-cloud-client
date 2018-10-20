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

#define MAX_RETRIES 3

#ifdef MBED_CONF_DOWNLOAD_TEST_URL_HOST
  const char dl_host[] = MBED_CONF_DOWNLOAD_TEST_URL_HOST;
#else
  const char dl_host[] = "armmbed.github.io";
#endif
#ifdef MBED_CONF_DOWNLOAD_TEST_URL_PATH
  const char dl_path[] = MBED_CONF_DOWNLOAD_TEST_URL_PATH;
#else
  const char dl_path[] = "/mbed-test-files/alice.txt";
#endif
#ifdef MBED_CONF_DOWNLOAD_TEST_FILENAME
  #include MBED_CONF_DOWNLOAD_TEST_FILENAME
#else
  #include "alice.h"
#endif

const char part1[] = "GET ";
const char part2[] = " HTTP/1.1\nHost: ";
const char part3[] = "\n\n";

static volatile bool event_fired[5] = { };

DigitalOut led1(LED1);

static void socket_event_0(void) { event_fired[0] = true; }
static void socket_event_1(void) { event_fired[1] = true; }
static void socket_event_2(void) { event_fired[2] = true; }
static void socket_event_3(void) { event_fired[3] = true; }
static void socket_event_4(void) { event_fired[4] = true; }

size_t download_test(size_t buff_size, NetworkInterface* interface, uint32_t thread_id) {
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

    if (thread_id == 0) {
        // technically this is non-threaded mode
        tcpsocket->sigio(socket_event_0);
    } else if (thread_id == 1) {
        tcpsocket->sigio(socket_event_1);
    } else if (thread_id == 2) {
        tcpsocket->sigio(socket_event_2);
    } else if (thread_id == 3) {
        tcpsocket->sigio(socket_event_3);
    } else if (thread_id == 4) {
        tcpsocket->sigio(socket_event_4);
    } else {
        TEST_ASSERT_MESSAGE(0, "wrong thread id");
    }
    printf("[INFO-%d] Registered socket callback function\r\n", thread_id);
    event_fired[thread_id] = false;

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
    char* receive_buffer = new char[buff_size];
    TEST_ASSERT_NOT_NULL_MESSAGE(receive_buffer, "failed to allocate receive buffer");

    size_t expected_bytes = sizeof(story);
    size_t received_bytes = 0;
    int body_index = -1;

    Timer timer;
    timer.start();

    /* loop until all expected bytes have been received */
    while (received_bytes < expected_bytes) {
        /* wait for async event */
        while(!event_fired[thread_id]) {
            if (thread_id > 0) {
                Thread::yield();
            }
        }
        event_fired[thread_id] = false;
        led1 = true;

        /* loop until all data has been read from socket */
        do {
            result = tcpsocket->recv(receive_buffer, buff_size);
            TEST_ASSERT_MESSAGE((result == NSAPI_ERROR_WOULD_BLOCK) || (result >= 0), "failed to read socket");

            if (result > 0) {
                /* skip HTTP header */
                if (body_index < 0) {
                    /* note that there are no required Response headers and their length may greatly vary */
                    std::string header(receive_buffer, result);
                    body_index = header.find("\r\n\r\n");
                    if (body_index < 0) {
                        continue;
                    } else {
                        printf("[INFO-%d] Found body index: %d\r\n", thread_id, body_index);

                        /* remove header before comparison */
                        memmove(receive_buffer, &receive_buffer[body_index + 4], result - body_index - 4);

                        TEST_ASSERT_EQUAL_STRING_LEN_MESSAGE(story, receive_buffer, result - body_index - 4, "character mismatch in header");

                        received_bytes += (result - body_index - 4);
                    }
                } else {
                    TEST_ASSERT_EQUAL_STRING_LEN_MESSAGE(&story[received_bytes], receive_buffer, result, "character mismatch in body");

                    received_bytes += result;
                }

                printf("[INFO-%d] Received bytes: %u\r\n", thread_id, received_bytes);
            }
        }
        while ((result > 0) && (received_bytes < expected_bytes));

        led1 = false;
    }

    TEST_ASSERT_MESSAGE(body_index >= 0, "failed to find body");

    delete request;
    delete tcpsocket;
    delete[] receive_buffer;

    timer.stop();

    printf("[INFO-%d] Downloaded %.2fKB in %f seconds (%.2fKB/s)\r\n", thread_id, float(received_bytes) / 1024, timer.read(), float(received_bytes) / timer.read() / 1024);

    return received_bytes;
}

size_t download_test(size_t buff_size, NetworkInterface* interface) {
    return download_test(buff_size, interface, 0);
}
