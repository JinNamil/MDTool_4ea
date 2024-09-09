# Raspberry Pi Based MCU Firmware Download Tool

This project is a tool for downloading firmware to an MCU (Microcontroller Unit) using a Raspberry Pi. It facilitates uploading firmware via communication between the Raspberry Pi and the MCU, enabling easy firmware version management and updates.

## Requirements

To run this project, the following requirements must be met:

- Raspberry Pi (Model 3 or higher recommended)
- Python 3.x or higher
- MCU (e.g., STM32, ESP32, etc.)
- Interface for communication between the MCU and Raspberry Pi (e.g., UART, SPI, I2C)
- Firmware file (.bin or .hex)

## Installation and Setup

1. Install the necessary packages on the Raspberry Pi.

    ```bash
    sudo apt update
    sudo apt install python3 python3-pip git
    ```

2. Clone this repository.

    ```bash
    git clone https://github.com/JinNamil/MDTool_4ea
    ```

3. Install the required Python dependencies.

    ```bash
    pip3 install -r requirements.txt
    ```

4. Set up the connection between the MCU and Raspberry Pi. Complete the connection using an interface such as UART, SPI, or I2C.

5. Edit the configuration file to update the settings for your specific MCU.

    ```bash
    nano config.yaml
    ```

    In the `config.yaml` file, you can configure options such as the communication port, baud rate, and other settings.

## Usage

1. Prepare the firmware file.
   - The firmware file should be in `.bin` or `.hex` format and stored in the `firmware/` directory.

2. Execute the firmware download command.

    ```bash
    python3 mcu_firmware_tool.py --file firmware/your_firmware_file.bin
    ```

    The above command will download the specified firmware file to the MCU.

3. Once the download is complete, a success message will be displayed.

## Options

- `--file`: Path to the firmware file to be downloaded
- `--port`: Port connected to the MCU (default: `/dev/ttyS0`)
- `--baudrate`: Communication speed (default: `115200`)
- `--verify`: Option to verify after download (default: disabled)

## Contribution

1. Fork this project.
2. Create a new branch (`git checkout -b feature/new-feature`).
3. Commit your changes (`git commit -am 'Add new feature'`).
4. Push to the branch (`git push origin feature/new-feature`).
5. Create a Pull Request.

## Troubleshooting

If you encounter issues, check the following:

1. The connection between the Raspberry Pi and the MCU.
2. The communication port and baud rate values in the configuration file (`config.yaml`).
3. Whether the correct firmware file for the MCU is being used.

## License

This project is licensed under the MIT License. For more details, refer to the [LICENSE](LICENSE) file.
