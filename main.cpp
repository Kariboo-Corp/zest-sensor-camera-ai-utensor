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
#include "swo.h"
#include "FlashIAPBlockDevice.h"
#include "USBMSD.h"
#include "FATFileSystem.h"
#include "BlockDevice.h"
#include <cmath>
#include <iostream>

#include "models/my_model/my_model.hpp"
#include "models/samples/input_image.h"
#include "uTensor.h"

#include "zest-sensor-camera/zest-sensor-camera.h"

using namespace sixtron;
using namespace uTensor;

namespace {
#define PERIOD_MS          500
#define TIMEOUT_MS         1000 // timeout capture sequence in milliseconds
#define BOARD_VERSION      "v2.1.0"
#define START_PROMPT       "\r\n*** Zest Sensor Camera Demo ***\r\n"\
                           "camera version board: "\
                           BOARD_VERSION
#define PROMPT             "\r\n> "
#define CAPTURE_COUNT      1 // capture image count
#define INTERVAL_TIME      500 // delay between each capture, used if the CAPTURE_COUNT is bigger than one
#define FLASH_ENABLE       1 // state of led flash during the capture
#define FLASHIAP_ADDRESS   0x08055000 // 0x0800000 + 350 kB
#define FLASHIAP_SIZE      0x70000 //0x61A80    // 460 kB
}

// Prototypes
void application_setup(void);
void application(void);
uint32_t jpeg_processing(uint8_t *data);

// Peripherals
SWO pc;
static DigitalOut led1(LED1);
static InterruptIn button(BUTTON1);
ZestSensorCamera camera_device;

// RTOS
static Thread queue_thread;
static EventQueue queue;

// Create flash IAP block device
BlockDevice *bd = new FlashIAPBlockDevice(FLASHIAP_ADDRESS, FLASHIAP_SIZE);
//FlashIAPBlockDevice bd(FLASHIAP_ADDRESS, FLASHIAP_SIZE);
FATFileSystem fs("fs");

// Variables
int jpeg_id = 0;

static void camera_frame_handler(void)
{
    queue.call(application);
}

static void button_handler(void)
{
    queue.call(&camera_device, &ZestSensorCamera::take_snapshot, (bool)(FLASH_ENABLE));
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

    // Try to record jpeg in flash storage mounted on USB
    //fs.mount(bd);

    FILE *f;
    fflush(stdout);
    char filename[20];
    sprintf(filename, "/fs/jpeg_%d.jpg", jpeg_id);
    f = fopen(filename, "w");
    pc.printf("%s\n", (!f ? "Fail :(" : "OK"));
    if (!f) {
        pc.printf("error: %s (%d)\n", strerror(errno), -errno);
    }
    int err = fwrite(data, sizeof(uint8_t), length, f);
    pc.printf("Octets écrits : %d\n", err);
    if (err < 0) {
        pc.printf("Fail :(\n");
        pc.printf("error: %s (%d)\n", strerror(errno), -errno);
    }

    fclose(f);

    // Display the root directory
    printf("Opening the root directory... ");
    fflush(stdout);
    DIR *d = opendir("/fs/");
    printf("%s\n", (!d ? "Fail :(" : "OK"));
    if (!d) {
        error("error: %s (%d)\n", strerror(errno), -errno);
    }

    printf("root directory:\n");
    while (true) {
        struct dirent *e = readdir(d);
        if (!e) {
            break;
        }

        printf("    %s\n", e->d_name);
    }

    //fs.unmount();

    return length;
}

void application_setup(void)
{
    // setup power
    camera_device.power_up();
    // set user button handler
    button.fall(button_handler);
    // re-init jpeg id
    jpeg_id = 0;

    // Initialize the flash IAP block device and print the memory layout
	bd->init();
	pc.printf("Flash block device size: %llu\n",         bd->size());
	pc.printf("Flash block device read size: %llu\n",    bd->get_read_size());
	pc.printf("Flash block device program size: %llu\n", bd->get_program_size());
	pc.printf("Flash block device erase size: %llu\n",   bd->get_erase_size());

    pc.printf("Mounting the filesystem... ");
    fflush(stdout);
    int err = fs.mount(bd);

    //fs.format(bd);

    pc.printf("%s\n", (err ? "Fail :(" : "OK"));
    if (err) {
        // Reformat if we can't mount the filesystem
        // this should only happen on the first boot
        pc.printf("No filesystem found, formatting... ");
        fflush(stdout);
        err = fs.reformat(bd);
        pc.printf("%s\n", (err ? "Fail :(" : "OK"));
        if (err) {
            error("error: %s (%d)\n", strerror(-err), err);
        }
    }

    //fs.unmount();
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
        queue_thread.start(callback(&queue, &EventQueue::dispatch_forever));
        pc.printf(PROMPT);
        pc.printf("Press the button to start the snapshot capture...");
    } else {
        pc.printf(PROMPT);
        pc.printf("Error: omnivision sensor ov5640 initialization failed");
        return -1;
    }

    USBMSD usb(bd);

    My_model model;

    Tensor input_image = new RomTensor({1, 28, 28, 1}, flt, arr_input_image);
    Tensor logits = new RamTensor({1, 10}, flt);

    model.set_inputs({{My_model::input_0, input_image}}).set_outputs({{My_model::output_0, logits}}).eval();

    float max_value = static_cast<float>(logits(0));
    int max_index = 0;
    for (int i = 1; i < 10; ++i) {
        float value = static_cast<float>(logits(i));
        if (value >= max_value) {
        max_value = value;
        max_index = i;
        }
    }

    pc.printf("label : %d", max_index);

    while (true) {
        usb.process();
    }
}
