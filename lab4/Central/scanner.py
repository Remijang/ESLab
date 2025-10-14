from bluepy.btle import Scanner, DefaultDelegate, Peripheral
import time
from prompt_toolkit import prompt
from prompt_toolkit.patch_stdout import patch_stdout


class ScanDelegate(DefaultDelegate):
    def __init__(self):
        DefaultDelegate.__init__(self)

    def handleDiscovery(self, dev, isNewDev, isNewData):
        if isNewDev:
            print(f"Discovered device {dev.addr}\r", end="")
            return
        elif isNewData:
            return


class MyDelegate(DefaultDelegate):
    def __init__(self):
        DefaultDelegate.__init__(self)

    def handleNotification(self, cHandle, data):
        x = int.from_bytes(data[2:4], byteorder="little", signed=True)
        y = int.from_bytes(data[4:6], byteorder="little", signed=True)
        z = int.from_bytes(data[6:8], byteorder="little", signed=True)
        print(f"Get notification {cHandle}: {x}, {y}, {z}")
        return


def connect():
    print("-" * 10, "start of scan", "-" * 10)
    scanner = Scanner().withDelegate(ScanDelegate())
    while True:
        try:
            devices = scanner.scan(5.0)
            break
        except:
            pass

    print("\n\r", "-" * 10, "end of scan", "-" * 10)

    for dev in devices:
        complete_local_name = dev.getValueText(9)
        if type(complete_local_name) == str:
            print(complete_local_name)
        else:
            continue
        if complete_local_name != "BlueNRG":
            continue

        print("Device %s (%s), RSSI=%d dB" % (dev.addr, dev.addrType, dev.rssi))
        for adtype, desc, value in dev.getScanData():
            print(f"   {adtype}: {desc} = {value}")

        print("-" * 30)
        peripheral = Peripheral(dev)
        peripheral.setDelegate(MyDelegate())
        return peripheral
    raise AssertionError("Device not found")


def main():
    peripheral = connect()
    CCCD_UUID = "00002902-0000-1000-8000-00805f9b34fb"
    target_c_uuid = "11110000-1111-0000-1111-000011110000"
    services = peripheral.getServices()
    services = [s for s in services if s.uuid == "00000000-0001-11e1-9ab4-0002a5d5c51b"]
    service = services[0]
    characteristics = service.getCharacteristics()
    cccd_handle = None

    characteristics = [
        c for c in characteristics if c.uuid == "00e00000000111e1ac360002a5d5c51b"
    ]
    c = characteristics[0]
    prop = c.propertiesToString()
    handle = c.getHandle()
    print(f" {c}: {prop}, {handle}")
    descriptors = peripheral.getDescriptors(handle, 0xFFFF)
    for d in descriptors:
        # print("   ", str(d.uuid).lower())
        if str(d.uuid).lower() == CCCD_UUID.lower() and cccd_handle == None:
            cccd_handle = d.handle
        if str(d.uuid).lower() == target_c_uuid.lower():
            target_c_handle = d.handle
    if cccd_handle is not None:
        print(f"        Found CCCD handle: {cccd_handle}")
        peripheral.writeCharacteristic(cccd_handle, b"\x01\x00", False)
        print(f"        Notifications enabled")
        time.sleep(1)
        ret = peripheral.readCharacteristic(cccd_handle)
        print(f"current value: {ret}")

    print("-" * 30)

    while True:
        if peripheral.waitForNotifications(1.0):
            time.sleep(1)
            continue
        """
        with patch_stdout():
            freq = prompt("set the freq here > ")

            if freq.isdigit():
                freq = int(freq)
                freq_bytes = freq.to_bytes(4, byteorder='little')
                if target_c_handle:
                    peripheral.writeCharacteristic(target_c_handle, freq_bytes, withResponse=False)
            else:
                print("Wrong input format")
        """


if __name__ == "__main__":
    main()
