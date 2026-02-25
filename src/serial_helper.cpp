#include "serial_helper.hpp"
#include <chrono>
// #include <dirent.h>
// #include <unistd.h>
// #include <glob.h>
// #include <fstream>
#include <regex>
// #include <filesystem>
// #include <stdexcept>
#include <iomanip>
#include <iostream>
#include <set>
SerialHelper::SerialHelper(std::mutex &lock, std::string Port,
                           uint32_t BaudRate, uint8_t ByteSize, char Parity,
                           uint8_t Stopbits)
    : lock(lock), port(Port) {
  switch (BaudRate) {
  case 9600:
    baudrate = LibSerial::BaudRate::BAUD_9600;
    break;
  case 19200:
    baudrate = LibSerial::BaudRate::BAUD_19200;
    break;
  case 38400:
    baudrate = LibSerial::BaudRate::BAUD_38400;
    break;
  case 57600:
    baudrate = LibSerial::BaudRate::BAUD_57600;
    break;
  case 115200:
    baudrate = LibSerial::BaudRate::BAUD_115200;
    break;

  default:
    baudrate = LibSerial::BaudRate::BAUD_115200;
    break;
  }
  switch (ByteSize) {
  case 5:
    bytesize = LibSerial::CharacterSize::CHAR_SIZE_5;
    break;
  case 6:
    bytesize = LibSerial::CharacterSize::CHAR_SIZE_6;
    break;
  case 7:
    bytesize = LibSerial::CharacterSize::CHAR_SIZE_7;
    break;
  case 8:
    bytesize = LibSerial::CharacterSize::CHAR_SIZE_8;
    break;
  default:
    bytesize = LibSerial::CharacterSize::CHAR_SIZE_DEFAULT;
    break;
  }
  switch (Stopbits) {
  case 1:
    stopbits = LibSerial::StopBits::STOP_BITS_1;
    break;
  case 2:
    stopbits = LibSerial::StopBits::STOP_BITS_2;
    break;
  default:
    stopbits = LibSerial::StopBits::STOP_BITS_DEFAULT;
    break;
  }
  switch (Parity) {
  case 'N':
    parity = LibSerial::Parity::PARITY_NONE;
    break;
  case 'O':
    parity = LibSerial::Parity::PARITY_ODD;
    break;
  case 'E':
    parity = LibSerial::Parity::PARITY_EVEN;
    break;
  default:
    parity = LibSerial::Parity::PARITY_DEFAULT;
    break;
  }
}
void SerialHelper::connect(/* uint16_t timeout */) {
  std::lock_guard<std::mutex> guard(lock);
  try {
    _serial.Open(port);
    if (!_serial.IsOpen()) {
      throw std::runtime_error("串口打开失败：" + port);
      printf("串口打开失败!!!!");
    }
    _serial.SetBaudRate(baudrate);
    _serial.SetCharacterSize(bytesize);
    _serial.SetParity(parity);
    _serial.SetStopBits(stopbits);
  } catch (const std::exception &e) {
    std::cerr << "串口连接失败 (" << e.what() << "): " << std::endl;
    _is_connected = false;
    return;
  }
  _is_connected = true;
}
void SerialHelper::disconnect() {

  if (_is_connected) {
    std::lock_guard<std::mutex> gard(lock);
    _serial.Close();
  }
}

std::vector<uint8_t> SerialHelper::hex_to_bytes(const std::string &hex) {
  Data bytes;
  std::istringstream iss(hex);
  std::string byteStr;
  while (iss >> std::setw(2) >> byteStr) {
    uint8_t byte = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
    bytes.push_back(byte);
  }
  return bytes;
}

void SerialHelper::write(const Data &data /*  , bool isHex */) {
  std::lock_guard<std::mutex> gard(lock);
  if (_is_connected) {
    // 串口已连接，直接写
    _serial.Write(data);
  }
  /* else
  {
      std::cout << "port no open" << std::endl;

      Data outData = data;
      if (isHex)
      {
          // data 当作十六进制字符串处理
          std::string hexStr(data.begin(), data.end());
          outData = hex_to_bytes(hexStr);
      }

      _serial.Write(outData);
  } */
}

void SerialHelper::on_connected_changed(const ConnectedCallback &func) {
  std::thread connect_thread(
      [this, func]() { this->_on_connected_changed(func); });
  // 设置为分离线程（对应Python daemon=True）
  pthread_setname_np(connect_thread.native_handle(), "serial_connect_thread");
  connect_thread.detach();
}

void SerialHelper::_on_connected_changed(const ConnectedCallback &func) {
  _is_connected_temp = false;

  for (;;) {
    {
      std::lock_guard<std::mutex> gard(lock);
      _is_connected = false;

      // Linux 平台：检查端口是否存在
      std::vector<std::string> ttys = _serial.GetAvailableSerialPorts();
      if (std::find(ttys.begin(), ttys.end(), port) != ttys.end()) {
        _is_connected = true;
      } else {
        printf("未找到指定串口 : %s\n", port.c_str());
        printf("可用串口\n");
        for (auto &s : ttys) {
          printf("串口名称 : %s\n", s.c_str());
        }
      }

      // 状态变化才回调
      if (_is_connected_temp != _is_connected) {
        func(_is_connected);
      }

      _is_connected_temp = _is_connected.load();
    }

    // 0.5 秒
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}
/*
std::vector<std::string> SerialHelper::find_usb_tty(uint16_t vendor_id, uint16_t
product_id)
{
    std::vector<std::string> tty_devs;
    std::regex tty_regex("^ttyUSB[0-9]+$");

    const char *usb_path = "/sys/bus/usb/devices";
    DIR *dir = opendir(usb_path);
    if (!dir)
        return tty_devs;

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (entry->d_name[0] == '.')
            continue;

        std::string dev_path = std::string(usb_path) + "/" + entry->d_name;

        std::string vid_path = dev_path + "/idVendor";
        std::string pid_path = dev_path + "/idProduct";

        std::ifstream vid_file(vid_path.c_str());
        std::ifstream pid_file(pid_path.c_str());

        if (!vid_file.is_open() || !pid_file.is_open())
            continue;

        int vid = 0, pid = 0;
        vid_file >> std::hex >> vid;
        pid_file >> std::hex >> pid;

        if ((vendor_id != 0 && vid != vendor_id) ||
            (product_id != 0 && pid != product_id))
            continue;

        // 遍历 dev_path 下的子目录
        DIR *subdir = opendir(dev_path.c_str());
        if (!subdir)
            continue;

        struct dirent *subentry;
        while ((subentry = readdir(subdir)) != nullptr)
        {
            if (subentry->d_name[0] == '.')
                continue;

            std::string sub_path = dev_path + "/" + subentry->d_name;

            DIR *subsubdir = opendir(sub_path.c_str());
            if (!subsubdir)
                continue;

            struct dirent *file;
            while ((file = readdir(subsubdir)) != nullptr)
            {
                if (std::regex_match(file->d_name, tty_regex))
                {
                    tty_devs.push_back("/dev/" + std::string(file->d_name));
                }
            }
            closedir(subsubdir);
        }
        closedir(subdir);
    }

    closedir(dir);
    return tty_devs;
} */

void SerialHelper::on_data_received(const DataReceived &func) {
  std::thread tDataReceived([this, &func]() { this->_on_data_received(func); });
  pthread_setname_np(tDataReceived.native_handle(), "serial_data_recv_thread");
  tDataReceived.detach(); // 后台线程
}

// 串口数据接收线程
void SerialHelper::_on_data_received(const DataReceived &func) {
  for (;;) {
    {
      std::lock_guard<std::mutex> gard(lock);
      if (_is_connected) {
        try {
          // 检查数据是否可读（非阻塞）
          if (_serial.IsDataAvailable()) {
            Data data;

            // 读取所有可用字节
            while (_serial.IsDataAvailable()) {
              char byte;
              _serial.ReadByte(
                  byte, 20); // ReadByte 会阻塞，如果未设置 timeout，请设置
              data.push_back(static_cast<uint8_t>(byte));
            }

            if (!data.empty() && data.size() >= 9) {
              uint32_t height =
                  (static_cast<uint32_t>(data[5]) & 0xFF) |
                  ((static_cast<uint32_t>(data[6]) & 0xFF) << 8) |
                  ((static_cast<uint32_t>(data[7]) & 0xFF) << 16) |
                  ((static_cast<uint32_t>(data[8]) & 0xFF) << 24);
              current_height = height;
              func(data);
            }
          }
        } catch (const LibSerial::ReadTimeout &timeout) {
          printf("Serial read timeout: %s\n", timeout.what());
        } catch (const std::exception &e) {
          _is_connected = false;
          _serial.Close();
          break;
        }
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}