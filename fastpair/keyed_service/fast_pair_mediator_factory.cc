// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "fastpair/keyed_service/fast_pair_mediator_factory.h"

#include <memory>

#include "fastpair/internal/mediums/mediums.h"
#include "fastpair/server_access/fast_pair_repository_impl.h"
#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"
#include "fastpair/ui/ui_broker_impl.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {

MediatorFactory* MediatorFactory::GetInstance() {
  static MediatorFactory* instance = new MediatorFactory();
  return instance;
}

Mediator* MediatorFactory::CreateMediator() {
  mediator_ = std::make_unique<Mediator>(
      std::make_unique<Mediums>(), std::make_unique<UIBrokerImpl>(),
      std::make_unique<FastPairNotificationController>(),
      std::make_unique<FastPairRepositoryImpl>(),
      std::make_unique<SingleThreadExecutor>());
  return mediator_.get();
}

}  // namespace fastpair
}  // namespace nearby
