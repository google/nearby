#include <signal.h>
#include <cstdlib>
#include <ctime>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

#include <alsa/asoundlib.h>

#include "absl/synchronization/notification.h"
#include "absl/log/log.h"
#include "connections/implementation/service_controller_router.h"
#include "connections/walkietalkie/walkietalkie.h"

namespace {
static absl::Notification notification_;

void quit_handler(int sig, siginfo_t *siginfo, void *ignore) {
  if (!notification_.HasBeenNotified()) notification_.Notify();
}
}  // namespace

int main() {
  std::srand(std::time(nullptr));

  struct sigaction action {};
  action.sa_sigaction = quit_handler;
  action.sa_flags = SA_SIGINFO;

  sigaction(SIGINT, &action, nullptr);
  sigaction(SIGTERM, &action, nullptr);  

  auto client = std::make_unique<WalkieTalkie>(
      std::make_unique<nearby::connections::ServiceControllerRouter>());
  client->start();

  notification_.WaitForNotification();
  std::cerr << "received signal, shutting down" << std::endl;
  client.reset();
}
