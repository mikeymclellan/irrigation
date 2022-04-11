/*
 * Copyright 2018-2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

//
// Alexa Fact Skill - Sample for Beginners
//

// sets up dependencies
const Alexa = require('ask-sdk-core');
const i18n = require('i18next');

//AWS SDK will be sued to invoke device shadow from lambda function
var AWS = require('aws-sdk');

//This should be the device name mentioned in AWs IoT Core.
//In real life scenario, this should be captured as part of registraion
const topic = '$aws/things/si-03/shadow/update'

// const ioT_EndPoint = 'data.iot.us-east-1.amazonaws.com' // Ensure you are using the right endpoint
const ioT_EndPoint = 'data.iot.ap-southeast-2.amazonaws.com' // Ensure you are using the right endpoint

//Device Shadow message
var shadowMessage = {
  "state": {
    "desired": {}
  }
}

function updateDeviceShadow(obj, command) {
  return new Promise((resolve, reject) => {
    shadowMessage.state.desired[obj] = command;
    var iotdata = new AWS.IotData({ endpoint: ioT_EndPoint });
    var params = {
      payload: JSON.stringify(shadowMessage),
      topic: topic
    };

    iotdata.publish(params, function(err, data) {
      if (err)
        console.log(err, err.stack); // an error occurred
      else
        console.log(data);
      //reset the shadow
      shadowMessage.state.desired = {}
      resolve();
    });
  });
}

// core functionality for irrigation skill
const WaterLawnHandler = {
  canHandle(handlerInput) {
    const request = handlerInput.requestEnvelope.request;
    // checks request type
    return request.type === 'LaunchRequest'
        || (request.type === 'IntentRequest'
            && request.intent.name === 'WaterLawnIntent');
  },
  async handle(handlerInput) {
    console.log(handlerInput.requestEnvelope.request);
    var d_action_value = handlerInput.requestEnvelope.request.intent.slots.d_action.value;
    console.log(handlerInput.requestEnvelope.request.intent.slots.d_action);
    const requestAttributes = handlerInput.attributesManager.getRequestAttributes();

    var speakOutput;
    if (d_action_value === "sprinkle") {
      await updateDeviceShadow('relay_on_timer', 10000);
      speakOutput = requestAttributes.t('SPRINKLING_LAWN');
    }
    else if (d_action_value === "water") {
        await updateDeviceShadow('relay_on_timer', 1800000);
        speakOutput = requestAttributes.t('WATERING_LAWN');
    } else {
      await updateDeviceShadow('relay_on_timer', 1);
      speakOutput = requestAttributes.t('STOP_MESSAGE');
    }
    console.log(speakOutput);
    return handlerInput.responseBuilder
        .speak(speakOutput)
        //.reprompt('add a reprompt if you want to keep the session open for the user to respond')
        .getResponse();
  }
};

const HelpHandler = {
  canHandle(handlerInput) {
    const request = handlerInput.requestEnvelope.request;
    return request.type === 'IntentRequest'
      && request.intent.name === 'AMAZON.HelpIntent';
  },
  handle(handlerInput) {
    const requestAttributes = handlerInput.attributesManager.getRequestAttributes();
    return handlerInput.responseBuilder
      .speak(requestAttributes.t('HELP_MESSAGE'))
      .reprompt(requestAttributes.t('HELP_REPROMPT'))
      .getResponse();
  },
};

const FallbackHandler = {
  // The FallbackIntent can only be sent in those locales which support it,
  // so this handler will always be skipped in locales where it is not supported.
  canHandle(handlerInput) {
    const request = handlerInput.requestEnvelope.request;
    return request.type === 'IntentRequest'
      && request.intent.name === 'AMAZON.FallbackIntent';
  },
  handle(handlerInput) {
    const requestAttributes = handlerInput.attributesManager.getRequestAttributes();
    return handlerInput.responseBuilder
      .speak(requestAttributes.t('FALLBACK_MESSAGE'))
      .reprompt(requestAttributes.t('FALLBACK_REPROMPT'))
      .getResponse();
  },
};

const ExitHandler = {
  canHandle(handlerInput) {
    const request = handlerInput.requestEnvelope.request;
    return request.type === 'IntentRequest'
      && (request.intent.name === 'AMAZON.CancelIntent'
        || request.intent.name === 'AMAZON.StopIntent');
  },
  handle(handlerInput) {
    const requestAttributes = handlerInput.attributesManager.getRequestAttributes();
    updateDeviceShadow('relay_on_timer', 1);
    return handlerInput.responseBuilder
      .speak(requestAttributes.t('STOP_MESSAGE'))
      .getResponse();
  },
};

const SessionEndedRequestHandler = {
  canHandle(handlerInput) {
    const request = handlerInput.requestEnvelope.request;
    return request.type === 'SessionEndedRequest';
  },
  handle(handlerInput) {
    console.log(`Session ended with reason: ${handlerInput.requestEnvelope.request.reason}`);
    return handlerInput.responseBuilder.getResponse();
  },
};

const ErrorHandler = {
  canHandle() {
    return true;
  },
  handle(handlerInput, error) {
    console.log(`Error handled: ${error.message}`);
    console.log(`Error stack: ${error.stack}`);
    const requestAttributes = handlerInput.attributesManager.getRequestAttributes();
    return handlerInput.responseBuilder
      .speak(requestAttributes.t('ERROR_MESSAGE'))
      .reprompt(requestAttributes.t('ERROR_MESSAGE'))
      .getResponse();
  },
};

const LocalizationInterceptor = {
  process(handlerInput) {
    // Gets the locale from the request and initializes i18next.
    const localizationClient = i18n.init({
      lng: handlerInput.requestEnvelope.request.locale,
      resources: languageStrings,
      returnObjects: true
    });
    // Creates a localize function to support arguments.
    localizationClient.localize = function localize() {
      // gets arguments through and passes them to
      // i18next using sprintf to replace string placeholders
      // with arguments.
      const args = arguments;
      const value = i18n.t(...args);
      // If an array is used then a random value is selected
      if (Array.isArray(value)) {
        return value[Math.floor(Math.random() * value.length)];
      }
      return value;
    };
    // this gets the request attributes and save the localize function inside
    // it to be used in a handler by calling requestAttributes.t(STRING_ID, [args...])
    const attributes = handlerInput.attributesManager.getRequestAttributes();
    attributes.t = function translate(...args) {
      return localizationClient.localize(...args);
    }
  }
};

const skillBuilder = Alexa.SkillBuilders.custom();

exports.handler = skillBuilder
  .addRequestHandlers(
    WaterLawnHandler,
    HelpHandler,
    ExitHandler,
    FallbackHandler,
    SessionEndedRequestHandler,
  )
  .addRequestInterceptors(LocalizationInterceptor)
  .addErrorHandlers(ErrorHandler)
  .withCustomUserAgent('sample/basic-fact/v2')
  .lambda();

// It is organized by language/locale.  You can safely ignore the locales you aren't using.
// Update the name and messages to align with the theme of your skill

const enData = {
  translation: {
    SKILL_NAME: 'Irrigation',
    GET_FACT_MESSAGE: 'Here\'s your fact: ',
    HELP_MESSAGE: 'You can say tell me a space fact, or, you can say exit... What can I help you with?',
    HELP_REPROMPT: 'What can I help you with?',
    FALLBACK_MESSAGE: 'The Space Facts skill can\'t help you with that.  It can help you discover facts about space if you say tell me a space fact. What can I help you with?',
    FALLBACK_REPROMPT: 'What can I help you with?',
    ERROR_MESSAGE: 'Sorry, an error occurred.',
    STOP_MESSAGE: 'Okay!',
    WATERING_LAWN: 'Watering the lawn',
    SPRINKLING_LAWN: 'Sprinkling the lawn',
    FACTS:
      [
        'A year on Mercury is just 88 days long.',
        'Despite being farther from the Sun, Venus experiences higher temperatures than Mercury.',
        'On Mars, the Sun appears about half the size as it does on Earth.',
        'Jupiter has the shortest day of all the planets.',
        'The Sun is an almost perfect sphere.',
      ],
  },
};

const enauData = {
  translation: {
    SKILL_NAME: 'Irrigation',
  },
};

// constructs i18n and l10n data structure
const languageStrings = {
  'en': enData,
  'en-US': enauData,
};
