/*
 * Copyright (c) 2019, CATIE
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "mbed.h"

#include "zest-sensor-camera/zest-sensor-camera.h"

using namespace sixtron;

namespace {
#define PERIOD_MS      500
#define TIMEOUT_MS     1000 // timeout capture sequence in milliseconds
#define BOARD_VERSION  "v2.1.0"
#define START_PROMPT   "\r\n*** Zest Sensor Camera Demo ***\r\n"\
                       "camera version board: "\
                       BOARD_VERSION
#define PROMPT         "\r\n> "
#define CAPTURE_COUNT  1 // capture image count
#define INTERVAL_TIME  500 // delay between each capture, used if the CAPTURE_COUNT is bigger than one
#define FLASH_ENABLE   1 // state of led flash during the capture
}

// Prototypes
void application_setup(void);
void application(void);
void jpeg_processing(int jpeg_index, uint8_t *data);
void capture_process(void);

// Peripherals
RawSerial pc(SERIAL_TX, SERIAL_RX);
static DigitalOut led1(LED1);
static InterruptIn button(BUTTON1);
ZestSensorCamera camera_device;

// RTOS
static Thread thread_application;
static EventQueue queue;

// Variables
int jpeg_id = 0;

static void camera_frame_handler(void)
{
    queue.call(application);
}

static void button_handler(void)
{
    queue.call(capture_process);
}

uint32_t jpeg_processing(uint8_t *data)
{
    size_t i = 0;
    uint32_t jpgstart = 0;
    bool  head = false;
    uint8_t  *base_address = NULL;
    uint32_t length = 0;

    for (i = 0; i < OV5640_JPEG_BUFFER_SIZE; i++) {
        //search for 0XFF 0XD8 0XFF and 0XFF 0XD9, get size of JPG
        if ((data[i] == 0xFF) && (data[i + 1] == 0xD8) && (data[i + 2] == 0xFF)) {
            base_address = &data[i];
            jpgstart = i;
            head = 1; // Already found  FF D8
        }
        if ((data[i] == 0xFF) && (data[i + 1] == 0xD9) && head) {
            // set jpeg length
            length = i - jpgstart + 2;
            break;
        }
    }

    //end of traitment: back to base address of jpeg
    data = base_address;

    return length;
}

void application_setup(void)
{
    // setup power
    camera_device.power_up();
    // set user button handler
    button.fall(button_handler);
}

void application(void)
{
    uint32_t jpeg_size = 0;

    jpeg_id = jpeg_id + 1;
    // check if the jpeg picture is enable
    if (camera_device.jpeg_picture()) {
        jpeg_size = jpeg_processing(ov5640_camera_data());
        // print data to serial port
        pc.printf(PROMPT);
        pc.printf("JPEG %d stored in RAM: %ld bytes", jpeg_id, jpeg_size);
    }

    pc.printf(PROMPT);
    pc.printf("Complete camera acquisition");

}

void capture_process(void)
{
    camera_device.take_snapshot(FLASH_ENABLE);
}

// main() runs in its own thread in the OS
// (note the calls to Thread::wait below for delays)
int main()
{
    pc.printf(START_PROMPT);

    // application setup
    application_setup();

    // init ov5640 sensor: 15fps VGA resolution, jpeg compression enable and capture mode configured in snapshot mode
    if (camera_device.initialize(OV5640::Resolution::VGA_640x480, OV5640::FrameRate::_15_FPS,
                    OV5640::JpegMode::ENABLE, OV5640::CameraMode::SNAPSHOT)) {
        pc.printf(PROMPT);
        pc.printf("Omnivision sensor ov5640 initialized");
        // attach frame complete callback
        camera_device.attach_callback(camera_frame_handler);
        // start thread
        thread_application.start(callback(&queue, &EventQueue::dispatch_forever));
        pc.printf(PROMPT);
        pc.printf("Press the button to start the snapshot capture...");
    } else {
        pc.printf(PROMPT);
        pc.printf("Error: omnivision sensor ov5640 initialization failed");
        return -1;
    }

    while (true) {
        ThisThread::sleep_for(PERIOD_MS);
        led1 = !led1;
    }
}
