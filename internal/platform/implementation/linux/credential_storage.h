#ifndef PLATFORM_IMPL_LINUX_CREDENTIAL_STORAGE_H_
#define PLATFORM_IMPL_LINUX_CREDENTIAL_STORAGE_H_
#include <memory>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/sdbus-c++.h>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/credential_storage.h"

namespace nearby {
namespace linux {
class CredentialStorage : public api::CredentialStorage {
  using LocalCredential = ::nearby::internal::LocalCredential;
  using SharedCredential = ::nearby::internal::SharedCredential;
  using PublicCredentialType = ::nearby::presence::PublicCredentialType;
  using SaveCredentialsResultCallback =
      ::nearby::presence::SaveCredentialsResultCallback;
  using CredentialSelector = ::nearby::presence::CredentialSelector;
  using GetLocalCredentialsResultCallback =
      ::nearby::presence::GetLocalCredentialsResultCallback;
  using GetPublicCredentialsResultCallback =
      ::nearby::presence::GetPublicCredentialsResultCallback;

  CredentialStorage(sdbus::IConnection &connection);
  ~CredentialStorage() override = default;

  void SaveCredentials(absl::string_view manager_app_id,
                       absl::string_view account_name,
                       const std::vector<LocalCredential> &Local_credentials,
                       const std::vector<SharedCredential> &Shared_credentials,
                       PublicCredentialType public_credential_type,
                       SaveCredentialsResultCallback callback) override;
  void UpdateLocalCredential(absl::string_view manager_app_id,
                             absl::string_view account_name,
                             nearby::internal::LocalCredential credential,
                             SaveCredentialsResultCallback callback) override;
  void
  GetPublicCredentials(const CredentialSelector &credential_selector,
                       PublicCredentialType public_credential_type,
                       GetPublicCredentialsResultCallback callback) override;

private:
  std::unique_ptr<sdbus::IProxy> proxy;
};
} // namespace linux
} // namespace nearby

#endif
