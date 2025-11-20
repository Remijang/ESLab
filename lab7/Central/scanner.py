import struct
import time
import threading
import io

import matplotlib.pyplot as plt
from prompt_toolkit import Application
from prompt_toolkit.layout import Layout, HSplit
from prompt_toolkit.widgets import TextArea, Box, Frame
from prompt_toolkit.key_binding import KeyBindings
from prompt_toolkit.patch_stdout import patch_stdout

from bluepy.btle import DefaultDelegate, Peripheral, Scanner

CCCD_UUID = "00002902-0000-1000-8000-00805f9b34fb"
TARGET_UUID = "11110000-1111-0000-1111-000011110000"
TARGET_SERVICE_UUID = "00000000-0001-11e1-9ab4-0002a5d5c51b"
TARGET_CHAR_UUID = "00e00000-0001-11e1-ac36-0002a5d5c51b"
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


def make_plot(x, y):
    """Return PNG bytes of a matplotlib plot."""
    fig, ax = plt.subplots()
    ax.plot(x, label="Before")
    ax.plot(y, label="After")
    ax.legend()

    # Save to in-memory buffer
    # buf = io.BytesIO()
    fig.savefig("output.png", format="png", dpi=120, bbox_inches="tight")
    # plt.show()
    plt.close(fig)
    exit(0)
    return


def main():
    peripheral = connect()
    char_handle = scan(peripheral)
    cccd_handle = get_handle(peripheral, char_handle, CCCD_UUID)
    # target_handle = get_handle(peripheral, char_handle, TARGET_UUID)

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
            peripheral.waitForNotifications(1.0)

    log_area = TextArea(style="class:output-field", scrollbar=True, focusable=False)
    input_field = TextArea(
        height=1, prompt="freq > ", style="class:input-field", multiline=False
    )

    root_container = HSplit([log_area, input_field])
    layout = Layout(root_container, focused_element=input_field)

    kb = KeyBindings()

    @kb.add("c-c")
    def _keyboard_interrupt(event):
        event.app.exit(exception=KeyboardInterrupt, style="class:aborting")

    class MyDelegate(DefaultDelegate):
        def __init__(self):
            DefaultDelegate.__init__(self)
            self.before = [0] * 32
            self.after = [0] * 32

        def handleNotification(self, cHandle, data):
            for i in range(4):
                [x] = struct.unpack("<f", data[4 * i + 1 : 4 * i + 5])
                log_area.buffer.insert_text(
                    f"    {data[0]} {x:2f} {data[4 * i + 1]} {data[4 * i + 2]} {data[4 * i + 3]} {data[4 * i + 4]}\n"
                )
                if data[0] >= 8:
                    self.after[4 * data[0] - 32 + i] = x
                else:
                    self.before[4 * data[0] + i] = x
            if data[0] == 15:
                with open("result.txt", "w") as f:
                    print(self.before, file=f)
                    print(self.after, file=f)

            if app.is_running:
                app.invalidate()
            return

    app = Application(layout=layout, key_bindings=kb, full_screen=True)
    peripheral.setDelegate(MyDelegate())
    threading.Thread(target=ble_notification_simulator, daemon=True).start()

    with patch_stdout():
        app.run()


if __name__ == "__main__":
    main()
