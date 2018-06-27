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

using namespace utest::v1;

void smcc_register(void){

	printf("Test is run.");
    TEST_ASSERT_TRUE(true);
}

utest::v1::status_t greentea_setup(const size_t number_of_cases)
{

    GREENTEA_SETUP(30*60, "default_auto");
    return greentea_test_setup_handler(number_of_cases);
}

Case cases[] = {
    Case("Simple Cloud Client Register", smcc_register),
};

Specification specification(greentea_setup, cases);

int main()
{
    return !Harness::run(specification);
}
