#include "stdafx.h"
#include "compositor.h"

#error lol

#include "libovr_wrapper.h"

Compositor::~Compositor() {
	if (chain) {
		ovr_DestroyTextureSwapChain(*ovr::session, chain);
		chain = NULL;
	}
}
