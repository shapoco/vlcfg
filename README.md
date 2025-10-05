# [WIP] VLCFG

Visible Light Configuration Interface

## memo

|Pico2W|Connection|
|:--|:--|
|GPIO16 (I2C0 SDA)|EEPROM|
|GPIO17 (I2C0 SCL)|EEPROM|
|GPIO18|User Switch 0|
|GPIO19|User Switch 1|
|GPIO26 (I2C1 SDA)|SSD1306|
|GPIO27 (I2C1 SCL)|SSD1306|
|GPIO28 (ADC2)|Optical Sensor|

```
ADC2 (GP28): opt-sensor (NJL7502L)

  ↑
  │
|／ NJL7502L
|＼
  ├────┬── ADC2
  │        │
  [] 10k    ＝ 0.47u
  │        │
  ▽        ▽
```
