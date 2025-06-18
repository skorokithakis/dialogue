/**
 * @file keyboard.h
 * @author @rktrlng
 * @brief create USB HID keycodes from pin inputs
 * @see https://github.com/rktrlng/pico_tusb_keyboard
 */

#ifndef KEYS_H
#define KEYS_H

#include "hardware/gpio.h" // gpio_*
#include "class/hid/hid.h" // HID_KEY_*
#include <cstring>          // for memset()
#include <array>            // <─ add this

struct PinKey
{
	const uint8_t pin; // pico pin number
	const uint8_t key; // HID_KEY_*
};

class KeyBoard
{
private:
	// we no longer expose any GPIOs as keyboard keys
	static constexpr size_t num_pins = 0;
	std::array<PinKey, num_pins> pin_keys{};   // empty PinKey list
	uint32_t last_state = 0;      // 1-bit per PinKey entry

public:
	uint8_t key_codes[6] = {0}; // we can send max 6 keycodes per hid-report

	KeyBoard()
	{
		// set all pins to pulled up inputs
		for (size_t i = 0; i < num_pins; i++)
		{
			gpio_init(pin_keys[i].pin);
			gpio_pull_up(pin_keys[i].pin);
			gpio_set_dir(pin_keys[i].pin, GPIO_IN);
		}
	}

	virtual ~KeyBoard() {}

	bool update()
	{
		uint32_t cur_state = 0;
		for (size_t i = 0; i < num_pins; ++i)
		{
			if (gpio_get(pin_keys[i].pin) == 0)        // pressed ⇒ 0
				cur_state |= (1u << i);
		}

		if (cur_state == last_state) return false;     // no change since last call

		// clear & rebuild key code array from new state
		memset(key_codes, 0, sizeof(key_codes));
		uint8_t idx = 0;
		for (size_t i = 0; i < num_pins && idx < 6; ++i)
		{
			if (cur_state & (1u << i))
				key_codes[idx++] = pin_keys[i].key;
		}

		last_state = cur_state;
		return true;                                   // something changed
	}
};

#endif /* KEYS_H */
