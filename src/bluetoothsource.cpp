#include "bluetoothsource.h"

#include "logging.h"
#include "leds.h"
#include "app.h"
#include "mediaplayer.h"

BluetoothSource *globaSource = NULL;
App *globalApp;

int32_t readDataCallback(uint8_t *data, int32_t len)
{
  return globaSource->readData(data, len);
}

void avrccallback(uint8_t key, bool isReleased)
{
  globaSource->handleAVRC(key, isReleased);
}

BluetoothSource::BluetoothSource(I2SStream *output, Leds *leds, App *app, MediaPlayer *player)
{
  globaSource = this;
  globalApp = app;

  this->out = output;
  this->leds = leds;
  this->app = app;
  this->player = player;

  INFO("Bluetooth initializing buffers. Free HEAP is %d", ESP.getFreeHeap());
  this->buffer = new BufferRTOS<uint8_t>(0);
  this->buffer->resize(5 * 1024);
  this->bluetoothout = new QueueStream<uint8_t>(*buffer);

  INFO("Bluetooth initializing A2DP source. Free HEAP is %d", ESP.getFreeHeap());
  this->source = new BluetoothA2DPSource();
  this->source->set_local_name(app->computeTechnicalName().c_str());
  this->source->clean_last_connection();
  this->source->set_reset_ble(true);
  this->source->set_auto_reconnect(false);
  this->source->set_ssid_callback([](const char *ssid, esp_bd_addr_t address, int rrsi)
                                  {
    INFO("bluetooth() - Found SSID %s with RRSI %d", ssid, rrsi);
    if (globalApp->isValidDeviceToPairForBluetooth(String(ssid))) 
    {
      INFO("bluetooth() - Candidate for pairing found!");
      return true;
    }
    return false; });

  this->source->set_discovery_mode_callback([](esp_bt_gap_discovery_state_t discoveryMode)
                                            {
    switch (discoveryMode)
    {
    case ESP_BT_GAP_DISCOVERY_STARTED:
    INFO("bluetooth() - Discovery started");
    break;
    case ESP_BT_GAP_DISCOVERY_STOPPED:
    INFO("bluetooth() - Discovery stopped");
    break;
    } });

  this->source->set_data_callback(readDataCallback);

  this->source->set_on_connection_state_changed([](esp_a2d_connection_state_t state, void *x)
                                                {
    BluetoothSource *obj = (BluetoothSource*) x; 
    switch (state)
    {
    case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
      INFO("bluetooth() - DISCONNECTED");
      break;
    case ESP_A2D_CONNECTION_STATE_CONNECTING:
      INFO("bluetooth() - CONNECTING");
      break;
    case ESP_A2D_CONNECTION_STATE_CONNECTED:
      INFO("bluetooth() - CONNECTED. Sending player output to Bluetooth device");
      obj->leds->setState(BTCONNECTED);
      obj->player->setOutput(*(obj->bluetoothout));
      // Bluetooth Playback is always 70%
      obj->player->setVolume(0.7);
      obj->app->setBluetoothSpeakerConnected();
      obj->leds->setBluetoothSpeakerConnected();
      break;
    case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
      INFO("bluetooth() - DISCONNECTING. Sending player output to I2S");
      obj->player->setOutput(*(obj->out));
      break;
    } }, this);

  this->source->set_avrc_passthru_command_callback(avrccallback);
}

void BluetoothSource::start()
{
  // Start output when buffer is 95% full
  this->bluetoothout->begin(95);

  this->source->start();
}

int32_t BluetoothSource::readData(uint8_t *data, int32_t len)
{
  if (this->buffer->available() < len)
  {
    // Need more data
    return 0;
  }
  DEBUG("Requesting %d bytes for Bluetooth", len);
  int32_t result_bytes = this->buffer->readArray(data, len);
  DEBUG("Transfering %d bytes over Bluetooth, requested were %d", result_bytes, len);
  return result_bytes;
}

void BluetoothSource::handleAVRC(uint8_t key, bool isReleased)
{
  if (isReleased)
  {
    INFO("bluetooth() - AVRC button %d released!", key);
  }
  else
  {
    INFO("bluetooth() - AVRC button %d pressed!", key);
    if (key == 0x44) // PLAY
    {
      INFO("bluetooth() - AVRC PLAY pressed");
      this->app->toggleActiveState();
    }
    if (key == 0x45) // STOP
    {
      INFO("bluetooth() - AVRC STOP pressed");
      this->app->toggleActiveState();
    }
    if (key == 0x46) // PAUSE
    {
      INFO("bluetooth() - AVRC PAUSE pressed");
      this->app->toggleActiveState();
    }
    if (key == 0x4b) // NEXT
    {
      INFO("bluetooth() - AVRC NEXT pressed");
      this->app->next();
    }
    if (key == 0x4c) // PREVIOUS
    {
      INFO("bluetooth() - AVRC PREVIOUS pressed");
      this->app->previous();
    }
  }
}
