/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2023 katursis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "main.h"

StringView PluginComponent::componentName() const {
  return Plugin::Instance().Name();
}

SemanticVersion PluginComponent::componentVersion() const {
  auto [major, minor, patch] =
      Plugin::VersionToTuple(Plugin::Instance().Version());

  return SemanticVersion(0, major, minor, patch);
}

void PluginComponent::onLoad(ICore *c) {
  core_ = c;
  players_ = &c->getPlayers();

  getCore() = c;
}

void PluginComponent::onInit(IComponentList *components) {
  pawn_component_ = components->queryComponent<IPawnComponent>();
  if (!pawn_component_) {
    StringView name = componentName();

    core_->logLn(LogLevel::Error,
                 "Error loading component %.*s: Pawn component not loaded",
                 name.length(), name.data());

    return;
  }

  pawn_component_->getEventDispatcher().addEventHandler(this);
  players_->getPlayerTextDispatcher().addEventHandler(this);

  plugin_data_[PLUGIN_DATA_LOGPRINTF] =
      reinterpret_cast<void *>(&PluginLogprintf);
  plugin_data_[PLUGIN_DATA_AMX_EXPORTS] =
      const_cast<void **>(pawn_component_->getAmxFunctions().data());

  Plugin::DoLoad(plugin_data_);
}

void PluginComponent::onAmxLoad(IPawnScript &script) {
  Plugin::DoAmxLoad(static_cast<AMX *>(script.GetAMX()));
};

void PluginComponent::onAmxUnload(IPawnScript &script) {
  Plugin::DoAmxUnload(static_cast<AMX *>(script.GetAMX()));
};

void PluginComponent::onFree(IComponent *component) {
  if (component == pawn_component_ || component == this) {
    Plugin::DoUnload();

    if (pawn_component_) {
      pawn_component_->getEventDispatcher().removeEventHandler(this);
      players_->getPlayerTextDispatcher().removeEventHandler(this);
    }

    pawn_component_ = nullptr;
  }
}

void PluginComponent::reset() {}

void PluginComponent::free() {
  delete this;
}

bool PluginComponent::onPlayerCommandText(IPlayer &player, StringView message) {
  Plugin::ProcessCommand(player.getID(),
                         std::string(message.data(), message.length()).c_str());

  return true;
}

void PluginComponent::PluginLogprintf(const char *fmt, ...) {
  auto core = getCore();
  if (!core) {
    return;
  }

  va_list args{};

  va_start(args, fmt);

  core->vprintLn(fmt, args);

  va_end(args);
}

ICore *&PluginComponent::getCore() {
  static ICore *core{};

  return core;
}

COMPONENT_ENTRY_POINT() { return new PluginComponent(); }
