// AYG Cool Bed™
// https://aygarage.com/cool-bed/
//
// Copyright (c) 2026 AY Garage Ltd. All rights reserved.
// SPDX-License-Identifier: MIT

#include "Utils.h"

const char *resetReasonToString(esp_reset_reason_t reason)
{
	switch (reason)
	{
	case ESP_RST_UNKNOWN:
		return "Unknown";
	case ESP_RST_POWERON:
		return "Power-on";
	case ESP_RST_EXT:
		return "External reset";
	case ESP_RST_SW:
		return "Software reset";
	case ESP_RST_PANIC:
		return "Exception/Panic";
	case ESP_RST_INT_WDT:
		return "Interrupt watchdog";
	case ESP_RST_TASK_WDT:
		return "Task watchdog";
	case ESP_RST_WDT:
		return "Other watchdog";
	case ESP_RST_DEEPSLEEP:
		return "Wake from deep sleep";
	case ESP_RST_BROWNOUT:
		return "Brownout";
	case ESP_RST_SDIO:
		return "SDIO reset";
#if defined(ESP_RST_USB)
	case ESP_RST_USB:
		return "USB reset";
#endif
#if defined(ESP_RST_JTAG)
	case ESP_RST_JTAG:
		return "JTAG reset";
#endif
	default:
		return "Invalid";
	}
}
