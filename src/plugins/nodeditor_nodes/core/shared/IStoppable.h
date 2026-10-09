#pragma once

// Forwarding shim: the canonical definition of Daqster::IStoppable lives in
// src/plugins/common/capabilities/IStoppable.h. This used to be a byte-identical
// copy (REQ-SW-PL-050 "Оставащ дълг" #1): the shutdown path cast to the common
// interface while the 19 node models inherited the copy — safe only as long as
// both layouts stayed identical, and silently corrupting stop() offsets if they
// ever diverged. Keeping one definition removes the hazard.
//
// Kept as a stable include path for the node models and FrameworkGuiPlugin,
// which include "shared/IStoppable.h".
#include <capabilities/IStoppable.h>
