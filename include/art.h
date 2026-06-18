#ifndef __OS__ART_H
#define __OS__ART_H

#include <common/types.h>

namespace os {

	class Funny {

		public:
			Funny();
			~Funny();

			void LainFace();
			void LainSpiral();
			void LainNavi();
	
			void cat();
			void god();
			
			void LainAscii();
			
			void cubeAscii(os::common::uint16_t cubeCount);
	};
}

#endif
