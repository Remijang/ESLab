from bluepy.btle import Scanner, DefaultDelegate, Peripheral
import time


class ScanDelegate(DefaultDelegate):
    def __init__(self):
        DefaultDelegate.__init__(self)

    def handleDiscovery(self, dev, isNewDev, isNewData):
        if isNewDev:
            print("Discovered device", dev.addr)
            return
        elif isNewData:
            return


class MyDelegate(DefaultDelegate):
    def __init__(self):
        DefaultDelegate.__init__(self)

    def handleNotification(self, cHandle, data):
        print(f"Get notification {cHandle}: {data}")
        return


def main():
    scanner = Scanner().withDelegate(ScanDelegate())
    while True:
        try:
            devices = scanner.scan(5.0)
            break
        except:
            pass

    print("-" * 10, "end of scan", "-" * 10)

    CCCD_UUID = "00002902-0000-1000-8000-00805f9b34fb"
    for dev in devices:
        complete_local_name = dev.getValueText(9)
        print(complete_local_name)
        if complete_local_name != "owo_ble":
            continue

        print("Device %s (%s), RSSI=%d dB" % (dev.addr, dev.addrType, dev.rssi))
        for adtype, desc, value in dev.getScanData():
            print(f"   {adtype}: {desc} = {value}")

        print("-" * 30)
        peripheral = Peripheral(dev)
        peripheral.setDelegate(MyDelegate())
        services = peripheral.getServices()

        for service in services:
            if service.uuid == "00001111-0000-1111-0000-111100001111":
                print(f"{service}")
                characteristics = service.getCharacteristics()
                for c in characteristics:
                    prop = c.propertiesToString()
                    handle = c.getHandle()
                    can_read = c.supportsRead()

                    cccd = None
                    cccd_handle = None
                    descriptors = peripheral.getDescriptors(handle, 0xFFFF)
                    for d in descriptors:
                        print("   ", str(d.uuid).lower())
                        if str(d.uuid).lower() == CCCD_UUID.lower():
                            cccd_handle = d.handle
                            cccd = d
                            break

                    if can_read == True:
                        text = c.read()
                    else:
                        text = ""
                    if prop.strip() == "WRITE":
                        c.write(b"owo_test_write")

                    if cccd_handle is not None:
                        print(f"Found CCCD handle: {cccd_handle}")
                        peripheral.writeCharacteristic(cccd_handle, b"\x01\x00", False)
                        print(f"Notifications enabled")
                        time.sleep(10)
                        peripheral.writeCharacteristic(cccd_handle, b"\x00\x00", False)
                        print(f"Notifications disabled")
                        time.sleep(1)
                        ret = peripheral.readCharacteristic(cccd_handle)
                        print(f"current value: {ret}")
                        print(f" {c}: {prop}, {handle}, {text}")

        print("-" * 30)

        while True:
            if peripheral.waitForNotifications(1.0):
                time.sleep(1)
                continue

            print("waiting...")


if __name__ == "__main__":
    main()
