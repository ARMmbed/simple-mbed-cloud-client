"""
mbed SDK
Copyright (c) 2011-2013 ARM Limited

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

from mbed_host_tests import BaseHostTest
from mbed_cloud.device_directory import DeviceDirectoryAPI
import os

class SDKTests(BaseHostTest):
    
    __result = None
    deviceApi = None
    
    def _callback_device_api_registration(self, key, value, timestamp):

        try:
            # Check if device is in Mbed Cloud Device Directory
            device = self.deviceApi.get_device(value)
            
            # Send registraton status to device
            self.send_kv("registration_status", device.state)
            
        except:
            # SDK throws an exception if the device is not found (unsuccessful registration) or times out
            self.send_kv("registration_status", "error")

    def setup(self):
        
        # Register callbacks from GT tests
        self.register_callback('device_api_registration', self._callback_device_api_registration)
        
        # Setup API config
        api_key_val = os.environ.get("MBED_CLOUD_API_KEY")            
        api_config = {"api_key" : api_key_val, "host" : "https://api-os2.mbedcloudstaging.net"}
        
        # Instantiate Device API
        self.deviceApi = DeviceDirectoryAPI(api_config)
        
        
    def result(self):
        return self.__result

    def teardown(self):
        pass