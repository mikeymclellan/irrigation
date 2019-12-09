# Magic Cube - AWS Sumerian

https://github.com/t04glovern/aws-sumerian-magic-cube

This repo is a small end to end proof of concept on using AWS Sumerian with AWS IoT data

## Hardware

### BOM

***Note**: most of this stuff you can find extremely cheap off aliexpress if you shop around*

- 1x - [NodeMCU ESP8266](https://amzn.to/2BgZhnF) : $8.39
- 1x - [Adafruit Micro Lipo w/MicroUSB Jack](https://amzn.to/2Da2yp0) : $10.11
- 1x - [Lithium Ion Battery 1000mAh](https://www.sparkfun.com/products/13813) : $9.95
- 1x - [MMA8451 Accelerometer](https://amzn.to/2t5DV8d) : $9.44

### Schematic

![ESP8266 Schematic](aws-iot-esp8266/design/esp8266-circuit-design.png)

### Dependencies

We need to deploy the IoT Certificate vending machine lambda code somewhere accessible for CloudFormation.

```bash
## The command I use to deploy the IoT CVM code to my bucket
aws --profile=mclellan s3 cp aws-iot-cvm/iot-cvm.zip s3://smart-irrigation/resources/aws-iot-vpn-spawner/iot-cvm.zip
```

This reference must be updated in the `aws-iot-cvm/iot-cvm-params.json` parameters file else it will default to the version in my bucket. This is only applicable if you'd prefer to deploy your own.

```json
{
    "ParameterKey": "LambdaCodeBucket",
    "ParameterValue": "smart-irrigation" # Bucket Name
},
{
    "ParameterKey": "LambdaCodeS3Key",
    "ParameterValue": "resources/aws-iot-vpn-spawner/iot-cvm.zip" # Code Location
}
```

Deploys a IoT Vending machine instances that can be used to generate certificates for new devices

```bash
aws --profile mclellan cloudformation create-stack --stack-name "iot-cvm" \
    --template-body file://aws-iot-cvm/iot-cvm.yaml \
    --parameters file://aws-iot-cvm/iot-cvm-params.json \
    --capabilities CAPABILITY_IAM
```

Get details, including your API Endpoint for adding new IoT devices

```bash
aws --profile mclellan cloudformation describe-stacks --stack-name "iot-cvm" \
    --query 'Stacks[0].Outputs[?OutputKey==`RequestUrlExample`].OutputValue' \
    --output text

# https://pruqsmozo8.execute-api.ap-southeast-2.amazonaws.com/LATEST/getcert?serialNumber=value1&deviceToken=value2
```

Create a new Item in DynamoDB for your device by replacing:

- **si-01**: With your desired name for the device
- **fe14de2e-47bf-4786-9c66-c3f2943b96c7**: Something secret :)

```bash
aws --profile mclellan dynamodb put-item \
    --table-name iot-cvm-device-info \
    --item '{"deviceToken":{"S":"fe14de2e-47bf-4786-9c66-c3f2943b96c7"},"serialNumber":{"S":"si-02"}}'
```

Now make a request with the URL you got from the API gateway. Save the results to a file `config/iot-key.json`

```bash
curl https://pruqsmozo8.execute-api.ap-southeast-2.amazonaws.com/LATEST/getcert?serialNumber=si-01&deviceToken=fe14de2e-47bf-4786-9c66-c3f2943b96c7
```

You'll be returned a json response:

```json
{
    "certificateArn": "arn:aws:iot:us-east-1:<account-id>:cert/009ff6ee0.........",
    "certificateId": "009ff6ee092e......",
    "certificatePem": "-----BEGIN CERTIFICATE-----\nMIIDWTCCAkGgAwIBAgIUZiIgLi......-----END CERTIFICATE-----\n",
    "keyPair": {
        "PublicKey": "-----BEGIN PUBLIC KEY-----\nMIIBIjANBgkqhkiG9w0BAQEFAAO.......-----END PUBLIC KEY-----\n",
        "PrivateKey": "-----BEGIN RSA PRIVATE KEY-----\nMIIEpAIBAAKCAQ........-----END RSA PRIVATE KEY-----\n"
    },
    "RootCA": "-----BEGIN CERTIFICATE-----\r\nMIIE0zCCA7ugAwIBAgIQGNrRniZ96Lt........-----END CERTIFICATE-----"
}
```

Place the outputs for each of the two fields below into new files in [../certs](../certs)

- **../certs/certificate.pem.crt**: certificatePem
- **../certs/private.pem.key**: keyPair.PrivateKey

*Annoyingly you'll have to remove the newline delimiters with actual newlines. I usually use a \\n -> \n regular expression find and replace in VSCode*

## Uploading Certificates

You will also need to create the cert files based on the output from the CloudFormation deploy of the vending machine

```bash
openssl x509 -in certs/certificate.pem.crt -out data/cert.der -outform DER
openssl rsa -in certs/private.pem.key -out data/private.der -outform DER
openssl x509 -in certs/root-CA.pem -out data/ca.der -outform DER
```

Then upload the certificates using SPIFFS

```bash
pio run -t uploadfs
```

