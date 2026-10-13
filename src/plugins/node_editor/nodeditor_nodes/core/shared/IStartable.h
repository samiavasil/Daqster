#pragma once

// Forwarding shim: the canonical definition of Daqster::IStartable lives in
// src/plugins/common/capabilities/IStartable.h. See the note in the sibling
// IStoppable.h shim (REQ-SW-PL-050 "Оставащ дълг" #1).
#include <capabilities/IStartable.h>
