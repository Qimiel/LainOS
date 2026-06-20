#ifndef __OS__GUI__SCREEN_H
#define __OS__GUI__SCREEN_H

#include <common/types.h>

namespace os {
	namespace gui {

		static const common::uint16_t GRAPHICS_LOGICAL_WIDTH = 320;
		static const common::uint16_t GRAPHICS_LOGICAL_HEIGHT = 200;
		static const common::uint32_t GRAPHICS_LOGICAL_SIZE =
			GRAPHICS_LOGICAL_WIDTH * GRAPHICS_LOGICAL_HEIGHT;

		static const common::uint16_t GRAPHICS_SCREEN_WIDTH = 1600;
		static const common::uint16_t GRAPHICS_SCREEN_HEIGHT = 900;

		static const common::uint8_t TEXT_MODE_WIDTH = 80;
		static const common::uint8_t TEXT_MODE_HEIGHT = 25;

		static inline common::uint32_t LogicalIndex(common::uint32_t x,
								 common::uint32_t y) {
			return GRAPHICS_LOGICAL_WIDTH * y + x;
		}

		static inline common::uint32_t ScreenToLogicalX(common::uint32_t x) {
			return (x * GRAPHICS_LOGICAL_WIDTH) / GRAPHICS_SCREEN_WIDTH;
		}

		static inline common::uint32_t ScreenToLogicalY(common::uint32_t y) {
			return (y * GRAPHICS_LOGICAL_HEIGHT) / GRAPHICS_SCREEN_HEIGHT;
		}

	}
}

#endif
