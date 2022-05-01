# Irrigation IoT

Proof-of-concept project to control an internet connected irrigation system. This is broken up in to four parts:
   
* PlatformIO project which runs on an ESP8266 enables wifi config and connects to AWS IoT
* AWS infrastructure for registering new IoT devices and certificates
* An Alexa skill for controlling the irrigation system with voice commands
* A simple Labmda function for scheduling the irrigation

## Development

    pio run -t uploadfs
    platformio run --target upload
    platformio device monitor

## AWS Iot 

We need to deploy the IoT Certificate vending machine which is used when registering new IoT irrigation devices as AWS 
Things. This is based on another project with the README under `/infrastructure`. Walk thru https://devopstar.com/2019/02/07/aws-sumerian-magic-cube/

Some examples of updating the Thing shadow to turn the irrigation on and off. 

    aws iot-data publish --topic '$aws/things/si-03/shadow/update' --payload '{"state":{"desired":{"relay_on_timer":15000}}}' --cli-binary-format raw-in-base64-out --region ap-southeast-2
    aws iot-data publish --topic '$aws/things/si-03/shadow/update' --payload '{"state":{"desired":{"relay_state":1}}}' --cli-binary-format raw-in-base64-out --region ap-southeast-2

Example cronjob

    0 16 */3 * * /usr/local/bin/aws iot-data publish --topic '$aws/things/si-03/shadow/update' --payload '{"state":{"desired":{"relay_on_timer":1800000}}}' --cli-binary-format raw-in-base64-out --region ap-southeast-2

## Alexa Skill

There's an Alexa Skill which enables you to say "Hey Alexa, Ask irrigation to water the lawn" and it will publish to the IoT topic. 
This is implemented as an [ASK](https://developer.amazon.com/en-US/docs/alexa/smapi/ask-cli-command-reference.html) project under /alexa

## Update Firmware over Wifi

    platformio run && cp .pio/build/nodemcuv2/firmware.bin ~/Downloads/

Then open http://Irrigation.local/update in your browser and upload the firmware.bin file.

## Quick-n-dirty cronjob to run the irrigation

    aws cloudformation deploy --template-file irrigation/cron/cfn-cron.yaml --stack-name irrigation-cron --profile mclellan --region ap-southeast-2 --capabilities CAPABILITY_IAM
