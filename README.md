pio run -t uploadfs
platformio run --target upload
platformio device monitor


aws --profile mclellan iot-data publish --topic '$aws/things/si-03/shadow/update' --payload '{"state":{"desired":{"relay_state":1}}}'

aws --profile mclellan iot-data publish --topic '$aws/things/si-03/shadow/update' --payload '{"state":{"desired":{"relay_on_timer":15000}}}'



Walk thru https://devopstar.com/2019/02/07/aws-sumerian-magic-cube/ 


Alexa Skill to publish to IoT topic is an [ASK](https://developer.amazon.com/en-US/docs/alexa/smapi/ask-cli-command-reference.html) project under /alexa

## Update Firmware over Wifi

    platformio run && cp .pio/build/nodemcuv2/firmware.bin ~/Downloads/

Then open http://Irrigation.local/update in your browser and upload the firmware.bin file.
