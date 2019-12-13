pio run -t uploadfs
platformio run --target upload
platformio device monitor


aws --profile mclellan iot-data publish --topic '$aws/things/si-03/shadow/update' --payload '{"state":{"desired":{"relay_state":1}}}'

aws --profile mclellan iot-data publish --topic '$aws/things/si-03/shadow/update' --payload '{"state":{"desired":{"relay_on_timer":3000}}}'





Walk thru https://devopstar.com/2019/02/07/aws-sumerian-magic-cube/ 