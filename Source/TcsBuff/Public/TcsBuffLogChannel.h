// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"



// Buff 模块独立日志通道（D0-6 v2：独立通道文件——使用日志只 include 本头，不 include Module.h）
DECLARE_LOG_CATEGORY_EXTERN(LogTcsBuff, Log, All);
