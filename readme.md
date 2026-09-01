# 2.8 Oled AI Speech Recognition  

## Description  

This small project demonstrate how to use ESP Speech Recognition Engine  
with new generation of "Cheap Yellow Display" - "2.8inch ESP32-S3 Display".  

lcd wiki [wiki]  (https://www.lcdwiki.com/2.8inch_ESP32-S3_Display)  
Espressif sr [ESP-SR] (https://github.com/espressif/esp-sr)  

## Why need one more example ?  

This display based on ESP32S3 + ILI9341 + FT6336G +  ES8311 + SD CARD + RGB LED.  
I can't find any ready to use esp-sr pure idf examples for this device, and spend some  
time to develope some simpliest working example.  
Project based on IDF >=6.0 and LVGL >=9.0 mn5_en as sr model.  
Nothing needs to migrate.  

This module has only ES8311 as input and output audio codec.  
Many other devices has es7210 as ADC. 2 channel ADC + 1 channel DAC.  
Hardware configurations using es7210 has many public examples.  
But single es8311 (1 ADC + 1 DAC) has no "easy to find" examples.  

## Steps to go:  

-1 Switch on ES8311 as mic(ADC) input device .  
-2 Tune ESP32-SR to work with mono i2s channel.  
-3 Communication between different tasks in FreeRTOS.  

Mono configuration is not default data format for I2S.  
We need 16 bit per sample and single channel only.  
ESP-SR by default use second mic as reference for AEC(Automatic Echo Canceling).  
We have no two mic, only single mic.  

## Solution:  

Analog Front End(AFE) config:  
afe_config_t *afe_config = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_LOW_COST);  
"M" - singe adc no reference.  

And I2S configuration:  
i2s_std_config_t std_cfg_default = BSP_I2S_DUPLEX_MONO_CFG(16000);  

Now we have only one channel from adc (16 bit).  
Dont forget Set input mic gain to 42.0 (Not 30.0 or less as for es7210).  

## Getting Started:  

Git clone project to folder.  
git clone https://github.com/vso2004/oled_28_ai_sr  
 
-Open folder in VsCode( IDF extension must be active).  
-Wait IDF extension become ready.  
-Set IDF extension to version idf 6.0 and target to esp32s3.  
-Run "Build" or  "Build flash monitor".  
-All part of firmware have to download to device:  bootloader, spiffs, firmware.  
-You can send command manually by idf terminal:  

idf.py Build flash monitor  
   
## Important notes !!!!:  

Build process generate error when movemodel.py running (part of esp-sr component)  
"print('в”Ђ' * 40) generate error.  
Comment "print" command on error's lines.  
you can move  
./managed_components/espressif__esp-sr folder to ./component/espressif__esp-sr after successful build, to prevent overwrite movemodel.py  
after clearing.  

By the way, I move all components from "managed_components" to "components" and rename ./main/idf_component.yml to idf_component.yml_tmp (make it not useabled).
    
## Use example:  

Speak wake word ALEXA to build in mic of display.  
When wake word will be recognized message "WAIT" appear on screen .  
Speak command, for example "Sing me a song" or "tell me a joke" or up to 200 preprogrammed commands.  
 
## How to modify commands phrase.  

There is default method to create command list for multinet5: put commands to sdkconfig.  
I just a little fork default method:  

-1 Find folder "prepare_command_set_python" inside project.  
-2 Go to "prepare_command_set_python" and find and edit "commands_en_in.txt".  
-3 Add up to 200 commands in english.  
-4 Run gogogo.ps1 (powershell script).  
-5 File commands_en_v5.txt will be created.  
-6 Open sdkconfig.default.  
-7 Find string "# Add English speech commands".  
-8 Replace old command to new in sdkconfig.default (or/and sdkconfig.default.esp32s3).  
-9 Clean project (IDF.py clean  or delete ./build folder).  
-10 idf.py Build flash monitor.  
   
