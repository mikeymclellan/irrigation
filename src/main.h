#ifndef MAIN_H

const String thing_id = "si-03";

// AWS MQTT Details
const char* aws_mqtt_server = "a2uuawoq84cbx8-ats.iot.ap-southeast-2.amazonaws.com";
const char* aws_mqtt_client_id = "si-03";

// Use `$aws/things/[thing-name]/shadow/update` if working with shadows
const char* aws_mqtt_thing_topic_pub = "$aws/things/si-03/shadow/update";
const char* aws_mqtt_thing_topic_sub = "$aws/things/si-03/shadow/update";

#endif
