# smartletterbox
Attach a device to an existing letterbox that will send a push notification when a letter or package is inserted. It will also report the temperature, humidity and heat index.

### Configuration

lib/SensitiveConfig/SensitiveConfig_smartletterbox.h contains the WiFi and MQTT Broker configuration settings.

Rename lib/SensitiveConfig/SensitiveConfig_template.h to lib/SensitiveConfig/SensitiveConfig_smartletterbox.h and edit the placeholders with your own settings.

**Ensure .gitignore has an entry for the SensitiveConfig_smartletterbox.h file to avoid pushing it up to a public repository.**
