import time
import threading

from prompt_toolkit import Application
from prompt_toolkit.layout import Layout, HSplit
from prompt_toolkit.widgets import TextArea
from prompt_toolkit.key_binding import KeyBindings
from prompt_toolkit.patch_stdout import patch_stdout

from bluepy.btle import DefaultDelegate, Peripheral, Scanner

CCCD_UUID = "00002902-0000-1000-8000-00805f9b34fb"
TARGET_UUID = "11110000-1111-0000-1111-000011110000"
TARGET_SERVICE_UUID = "00000000-0001-11e1-9ab4-0002a5d5c51b"
TARGET_CHAR_UUID = "00e00000000111e1ac360002a5d5c51b"
DEVICE_NAME = "Lab4OWO"


class ScanDelegate(DefaultDelegate):
    def __init__(self):
        DefaultDelegate.__init__(self)

    def handleDiscovery(self, dev, isNewDev, isNewData):
        if isNewDev:
            print(f"Discovered device {dev.addr}\r", end="")
            return
        elif isNewData:
            return


def connect():
    print("-" * 10 + " start of scan " + "-" * 10)
    scanner = Scanner().withDelegate(ScanDelegate())
    while True:
        try:
            devices = scanner.scan(5.0)
            break
        except:
            pass

    print("\n" + "-" * 11 + " end of scan " + "-" * 11)

    for dev in devices:
        complete_local_name = dev.getValueText(9)
        if type(complete_local_name) == str:
            print(complete_local_name)
        else:
            continue
        if complete_local_name != DEVICE_NAME:
            continue

        print("Device %s (%s), RSSI=%d dB" % (dev.addr, dev.addrType, dev.rssi))
        for adtype, desc, value in dev.getScanData():
            print(f"   {adtype}: {desc} = {value}")

        print("-" * 30)
        peripheral = Peripheral(dev)

        return peripheral

    raise AssertionError("Device not found")


def scan(peripheral):
    services = peripheral.getServices()
    services = [s for s in services if s.uuid == TARGET_SERVICE_UUID]
    service = services[0]

    characteristics = service.getCharacteristics()
    characteristics = [c for c in characteristics if c.uuid == TARGET_CHAR_UUID]
    c = characteristics[0]

    prop = c.propertiesToString()
    handle = c.getHandle()
    print(f" {c}: {prop}, {handle}")
    print("-" * 33)

    return handle


def get_handle(peripheral, char_handle, UUID):
    descriptors = peripheral.getDescriptors(char_handle, 0xFFFF)
    for d in descriptors:
        if str(d.uuid).lower() == UUID.lower():
            return d.handle


def main():
    peripheral = connect()
    char_handle = scan(peripheral)
    cccd_handle = get_handle(peripheral, char_handle, CCCD_UUID)
    target_handle = get_handle(peripheral, char_handle, TARGET_UUID)

    print("-" * 4 + " turning on notification " + "-" * 4)
    if cccd_handle is not None:
        print(f"        Found CCCD handle: {cccd_handle}")
        peripheral.writeCharacteristic(cccd_handle, b"\x01\x00", False)
        print(f"        Notifications enabled")
        time.sleep(1)
        ret = peripheral.readCharacteristic(cccd_handle)
        print(f"current value: {ret}")
    print("-" * 33)

    def ble_notification_simulator():
        while True:
            if peripheral.waitForNotifications(1.0):
                time.sleep(0.05)

    log_area = TextArea(style="class:output-field", scrollbar=True, focusable=False)
    input_field = TextArea(
        height=1, prompt="freq > ", style="class:input-field", multiline=False
    )

    root_container = HSplit([log_area, input_field])
    layout = Layout(root_container, focused_element=input_field)

    kb = KeyBindings()

    @kb.add("enter")
    def _handle_standard_input(event):
        user_input = input_field.text.strip()
        input_field.text = ""
        if user_input == "" or not user_input.isdigit():
            log_area.buffer.insert_text(f"[USER] Invalid Input\n")
            event.app.invalidate()
            return

        freq = int(user_input)
        freq_bytes = freq.to_bytes(4, byteorder="little")
        if freq > 20:
            log_area.buffer.insert_text(f"[USER] Frequency Too Large, cap to {20}\n")
            freq = 20
        else:
            log_area.buffer.insert_text(f"[USER] Set Frequency to {freq}\n")
        peripheral.writeCharacteristic(target_handle, freq_bytes, withResponse=False)
        event.app.invalidate()

    @kb.add("c-c")
    def _keyboard_interrupt(event):
        event.app.exit(exception=KeyboardInterrupt, style="class:aborting")

    class MyDelegate(DefaultDelegate):
        def __init__(self):
            DefaultDelegate.__init__(self)

        def handleNotification(self, cHandle, data):
            x = int.from_bytes(data[2:4], byteorder="little", signed=True)
            y = int.from_bytes(data[4:6], byteorder="little", signed=True)
            z = int.from_bytes(data[6:8], byteorder="little", signed=True)
            if app.is_running:
                log_area.buffer.insert_text(
                    f"[BLE] Get notification {cHandle}: {x}, {y}, {z}\n"
                )
                app.invalidate()
            return

    app = Application(layout=layout, key_bindings=kb, full_screen=True)
    peripheral.setDelegate(MyDelegate())
    threading.Thread(target=ble_notification_simulator, daemon=True).start()

    with patch_stdout():
        app.run()


if __name__ == "__main__":
    main()
