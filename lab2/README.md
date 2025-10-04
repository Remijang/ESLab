# Lab2 

## 環境設定
在 `Projects/B-L475E-IOT01A/Applications/WiFi/WiFi_Client_Server/Inc` 目錄底下加入
`env.h` 檔如下：
```
#define SSID "<your SSID>"
#define PASSWORD "<your AP password>"

uint8_t RemoteIP[] = {172, 20, 10, 2}; // your remote IP
#define RemotePORT 8002                // your remote port
```