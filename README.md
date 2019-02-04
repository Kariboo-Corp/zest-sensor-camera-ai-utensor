# Zest_Sensor_Camera

A basic example to demonstrate how to use the Zest_Sensor_Camera.

## Requirements

*List Zest_Sensor_Camera requirements here*

## Usage

To clone **and** deploy the project in one command, use `mbed import` and skip to the
target and toolchain definition:

```shell
mbed import https://gitlab.com/catie_6tron/zest_sensor_camera.git zest_sensor_camera
```

Alternatively:

- Clone to "zest_sensor_camera" and enter it:

  ```shell
  git clone https://gitlab.com/catie_6tron/zest_sensor_camera.git zest_sensor_camera
  cd zest_sensor_camera
  ```

- Create an empty Mbed CLI configuration file:

  - On Linux/macOS:
    ```shell
    touch .mbed
    ```

  - Or on Windows:
    ```shell
    echo.> .mbed
    ```

- Deploy software requirements with:

  ```shell
  mbed deploy
  ```

Define your target (eg. `ZEST_CORE_STM32L496RG`) and toolchain:

```shell
mbed target ZEST_CORE_STM32L496RG
mbed toolchain GCC_ARM
```

Export to Eclipse IDE with:

```shell
mbed export -i eclipse_6tron
```

## Working from command line

Compile the project:

```shell
mbed compile
```

Program the target device (eg. `STM32L496RG` for the Zest_Core_STM32L496RG) with a J-Link
debug probe:

```shell
python dist/program.py STM32L496RG BUILD/ZEST_CORE_STM32L496RG/GCC_ARM/zest_sensor_camera.elf
```

Debug on the target device (eg. `STM32L496RG` for the Zest_Core_STM32L496RG) with a
J-Link debug probe.

- First, start the GDB server:

  ```shell
  JLinkGDBServer -device STM32L496RG
  ```

- Then, in another terminal, start the debugger:

  ```shell
  arm-none-eabi-gdb BUILD/ZEST_CORE_STM32L496RG/GCC_ARM/zest_sensor_camera.elf
  ```

*Note:* You may have to adjust your [GDB auto-loading safe path](https://sourceware.org/gdb/onlinedocs/gdb/Auto_002dloading-safe-path.html#Auto_002dloading-safe-path)
or disable it completely by adding a .gdbinit file in your $HOME folder containing:

```conf
set autoload safe-path /
```
