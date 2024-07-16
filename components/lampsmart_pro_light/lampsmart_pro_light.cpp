#include "lampsmart_utils.h"
#include "lampsmart_pro_light.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>

namespace esphome
{
  namespace lampsmartpro
  {

    void LampSmartProLight::setup()
    {
      register_service(&LampSmartProLight::on_pair, this->get_object_id());
      register_service(&LampSmartProLight::on_unpair, this->get_object_id());
    }

    light::LightTraits LampSmartProLight::get_traits()
    {
      auto traits = light::LightTraits();
      traits.set_supported_color_modes({light::ColorMode::COLD_WARM_WHITE});
      traits.set_min_mireds(this->cold_white_temperature_);
      traits.set_max_mireds(this->warm_white_temperature_);
      return traits;
    }

    char *LampSmartProLight::getHostDeviceIdentifier()
    {
      uint32_t hash = this->get_object_id_hash();
      int crc = CRC16((char *)&hash, 4, 0);

      static char hostId[2];
      hostId[0] = (crc >> 8) & 255;
      hostId[1] = crc & 255;
      return hostId;
    }

    void LampSmartProLight::write_state(light::LightState *state)
    {
      float cwf, wwf;
      state->current_values_as_cwww(&cwf, &wwf, this->constant_brightness_);

      if (!cwf && !wwf)
      {
        send_packet(CMD_TURN_OFF, 0, 0);
        _is_off = true;

        return;
      }

      uint8_t cwi = (uint8_t)(0xff * cwf);
      uint8_t wwi = (uint8_t)(0xff * wwf);

      if ((cwi < min_brightness_) && (wwi < min_brightness_))
      {
        if (cwf > 0.000001)
        {
          cwi = min_brightness_;
        }

        if (wwf > 0.000001)
        {
          wwi = min_brightness_;
        }
      }

      ESP_LOGD(TAG, "LampSmartProLight::write_state called! Requested cw: %d, ww: %d", cwi, wwi);

      if (_is_off)
      {
        send_packet(CMD_TURN_ON, 0, 0);
        _is_off = false;
      }

      send_packet(CMD_DIM, cwi, wwi);
    }

    void LampSmartProLight::dump_config()
    {
      ESP_LOGCONFIG(TAG, "LampSmartProLight '%s'", light_state_ ? light_state_->get_name().c_str() : "");
      ESP_LOGCONFIG(TAG, "  Cold White Temperature: %f mireds", cold_white_temperature_);
      ESP_LOGCONFIG(TAG, "  Warm White Temperature: %f mireds", warm_white_temperature_);
      ESP_LOGCONFIG(TAG, "  Constant Brightness: %s", constant_brightness_ ? "true" : "false");
      ESP_LOGCONFIG(TAG, "  Minimum Brightness: %d", min_brightness_);
      ESP_LOGCONFIG(TAG, "  Transmission Duration: %d millis", tx_duration_);
    }

    void LampSmartProLight::on_pair()
    {
      ESP_LOGD(TAG, "LampSmartProLight::on_pair called!");

      char *hostId = getHostDeviceIdentifier();
      send_packet(CMD_PAIR, hostId[0], hostId[1]);
    }

    void LampSmartProLight::on_unpair()
    {
      ESP_LOGD(TAG, "LampSmartProLight::on_unpair called!");

      char *hostId = getHostDeviceIdentifier();
      send_packet(CMD_UNPAIR, hostId[0], hostId[1]);
    }

    void LampSmartProLight::send_packet(uint16_t cmd, uint8_t arg1, uint8_t arg2)
    {
      char *hostId = getHostDeviceIdentifier();
      uint8_t *packet = (uint8_t *)buildPacket(cmd, hostId[0], hostId[1], arg1, arg2, group_id_);

      // Skip first byte (BLE packet size indicator)
      ESP_ERROR_CHECK_WITHOUT_ABORT(esp_ble_gap_config_adv_data_raw(&packet[1], 31));
      ESP_ERROR_CHECK_WITHOUT_ABORT(esp_ble_gap_start_advertising(&ADVERTISING_PARAMS));
      delay(tx_duration_);
      ESP_ERROR_CHECK_WITHOUT_ABORT(esp_ble_gap_stop_advertising());
    }

  } // namespace lampsmartpro
} // namespace esphome

#endif
