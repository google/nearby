// WARNING: Please don't edit this file. It was generated by C++/WinRT v2.0.250303.1

#pragma once
#ifndef WINRT_Windows_Management_Deployment_2_H
#define WINRT_Windows_Management_Deployment_2_H
#include "winrt/impl/Windows.ApplicationModel.2.h"
#include "winrt/impl/Windows.Management.Deployment.1.h"
WINRT_EXPORT namespace winrt::Windows::Management::Deployment
{
    struct DeploymentProgress
    {
        winrt::Windows::Management::Deployment::DeploymentProgressState state {};
        uint32_t percentage {};
    };
    inline bool operator==(DeploymentProgress const& left, DeploymentProgress const& right) noexcept
    {
        return left.state == right.state && left.percentage == right.percentage;
    }
    inline bool operator!=(DeploymentProgress const& left, DeploymentProgress const& right) noexcept
    {
        return !(left == right);
    }
    struct WINRT_IMPL_EMPTY_BASES AddPackageOptions : winrt::Windows::Management::Deployment::IAddPackageOptions,
        impl::require<AddPackageOptions, winrt::Windows::Management::Deployment::IAddPackageOptions2>
    {
        AddPackageOptions(std::nullptr_t) noexcept {}
        AddPackageOptions(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Windows::Management::Deployment::IAddPackageOptions(ptr, take_ownership_from_abi) {}
        AddPackageOptions();
    };
    struct WINRT_IMPL_EMPTY_BASES AppInstallerManager : winrt::Windows::Management::Deployment::IAppInstallerManager
    {
        AppInstallerManager(std::nullptr_t) noexcept {}
        AppInstallerManager(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Windows::Management::Deployment::IAppInstallerManager(ptr, take_ownership_from_abi) {}
        static auto GetDefault();
        static auto GetForSystem();
    };
    struct WINRT_IMPL_EMPTY_BASES AutoUpdateSettingsOptions : winrt::Windows::Management::Deployment::IAutoUpdateSettingsOptions
    {
        AutoUpdateSettingsOptions(std::nullptr_t) noexcept {}
        AutoUpdateSettingsOptions(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Windows::Management::Deployment::IAutoUpdateSettingsOptions(ptr, take_ownership_from_abi) {}
        AutoUpdateSettingsOptions();
        static auto CreateFromAppInstallerInfo(winrt::Windows::ApplicationModel::AppInstallerInfo const& appInstallerInfo);
    };
    struct WINRT_IMPL_EMPTY_BASES CreateSharedPackageContainerOptions : winrt::Windows::Management::Deployment::ICreateSharedPackageContainerOptions
    {
        CreateSharedPackageContainerOptions(std::nullptr_t) noexcept {}
        CreateSharedPackageContainerOptions(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Windows::Management::Deployment::ICreateSharedPackageContainerOptions(ptr, take_ownership_from_abi) {}
        CreateSharedPackageContainerOptions();
    };
    struct WINRT_IMPL_EMPTY_BASES CreateSharedPackageContainerResult : winrt::Windows::Management::Deployment::ICreateSharedPackageContainerResult
    {
        CreateSharedPackageContainerResult(std::nullptr_t) noexcept {}
        CreateSharedPackageContainerResult(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Windows::Management::Deployment::ICreateSharedPackageContainerResult(ptr, take_ownership_from_abi) {}
    };
    struct WINRT_IMPL_EMPTY_BASES DeleteSharedPackageContainerOptions : winrt::Windows::Management::Deployment::IDeleteSharedPackageContainerOptions
    {
        DeleteSharedPackageContainerOptions(std::nullptr_t) noexcept {}
        DeleteSharedPackageContainerOptions(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Windows::Management::Deployment::IDeleteSharedPackageContainerOptions(ptr, take_ownership_from_abi) {}
        DeleteSharedPackageContainerOptions();
    };
    struct WINRT_IMPL_EMPTY_BASES DeleteSharedPackageContainerResult : winrt::Windows::Management::Deployment::IDeleteSharedPackageContainerResult
    {
        DeleteSharedPackageContainerResult(std::nullptr_t) noexcept {}
        DeleteSharedPackageContainerResult(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Windows::Management::Deployment::IDeleteSharedPackageContainerResult(ptr, take_ownership_from_abi) {}
    };
    struct WINRT_IMPL_EMPTY_BASES DeploymentResult : winrt::Windows::Management::Deployment::IDeploymentResult,
        impl::require<DeploymentResult, winrt::Windows::Management::Deployment::IDeploymentResult2>
    {
        DeploymentResult(std::nullptr_t) noexcept {}
        DeploymentResult(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Windows::Management::Deployment::IDeploymentResult(ptr, take_ownership_from_abi) {}
    };
    struct WINRT_IMPL_EMPTY_BASES FindSharedPackageContainerOptions : winrt::Windows::Management::Deployment::IFindSharedPackageContainerOptions
    {
        FindSharedPackageContainerOptions(std::nullptr_t) noexcept {}
        FindSharedPackageContainerOptions(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Windows::Management::Deployment::IFindSharedPackageContainerOptions(ptr, take_ownership_from_abi) {}
        FindSharedPackageContainerOptions();
    };
    struct WINRT_IMPL_EMPTY_BASES PackageAllUserProvisioningOptions : winrt::Windows::Management::Deployment::IPackageAllUserProvisioningOptions,
        impl::require<PackageAllUserProvisioningOptions, winrt::Windows::Management::Deployment::IPackageAllUserProvisioningOptions2>
    {
        PackageAllUserProvisioningOptions(std::nullptr_t) noexcept {}
        PackageAllUserProvisioningOptions(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Windows::Management::Deployment::IPackageAllUserProvisioningOptions(ptr, take_ownership_from_abi) {}
        PackageAllUserProvisioningOptions();
    };
    struct WINRT_IMPL_EMPTY_BASES PackageManager : winrt::Windows::Management::Deployment::IPackageManager,
        impl::require<PackageManager, winrt::Windows::Management::Deployment::IPackageManager2, winrt::Windows::Management::Deployment::IPackageManager3, winrt::Windows::Management::Deployment::IPackageManager4, winrt::Windows::Management::Deployment::IPackageManager5, winrt::Windows::Management::Deployment::IPackageManager6, winrt::Windows::Management::Deployment::IPackageManager7, winrt::Windows::Management::Deployment::IPackageManager8, winrt::Windows::Management::Deployment::IPackageManager9, winrt::Windows::Management::Deployment::IPackageManager10, winrt::Windows::Management::Deployment::IPackageManager11>
    {
        PackageManager(std::nullptr_t) noexcept {}
        PackageManager(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Windows::Management::Deployment::IPackageManager(ptr, take_ownership_from_abi) {}
        PackageManager();
        using winrt::Windows::Management::Deployment::IPackageManager::AddPackageAsync;
        using impl::consume_t<PackageManager, winrt::Windows::Management::Deployment::IPackageManager3>::AddPackageAsync;
        using impl::consume_t<PackageManager, winrt::Windows::Management::Deployment::IPackageManager5>::AddPackageAsync;
        using impl::consume_t<PackageManager, winrt::Windows::Management::Deployment::IPackageManager6>::AddPackageAsync;
        using impl::consume_t<PackageManager, winrt::Windows::Management::Deployment::IPackageManager10>::ProvisionPackageForAllUsersAsync;
        using impl::consume_t<PackageManager, winrt::Windows::Management::Deployment::IPackageManager6>::ProvisionPackageForAllUsersAsync;
        using winrt::Windows::Management::Deployment::IPackageManager::RegisterPackageAsync;
        using impl::consume_t<PackageManager, winrt::Windows::Management::Deployment::IPackageManager3>::RegisterPackageAsync;
        using winrt::Windows::Management::Deployment::IPackageManager::RemovePackageAsync;
        using impl::consume_t<PackageManager, winrt::Windows::Management::Deployment::IPackageManager2>::RemovePackageAsync;
        using impl::consume_t<PackageManager, winrt::Windows::Management::Deployment::IPackageManager6>::RequestAddPackageAsync;
        using impl::consume_t<PackageManager, winrt::Windows::Management::Deployment::IPackageManager7>::RequestAddPackageAsync;
        using winrt::Windows::Management::Deployment::IPackageManager::StagePackageAsync;
        using impl::consume_t<PackageManager, winrt::Windows::Management::Deployment::IPackageManager2>::StagePackageAsync;
        using impl::consume_t<PackageManager, winrt::Windows::Management::Deployment::IPackageManager3>::StagePackageAsync;
        using impl::consume_t<PackageManager, winrt::Windows::Management::Deployment::IPackageManager5>::StagePackageAsync;
        using impl::consume_t<PackageManager, winrt::Windows::Management::Deployment::IPackageManager6>::StagePackageAsync;
        using impl::consume_t<PackageManager, winrt::Windows::Management::Deployment::IPackageManager2>::StageUserDataAsync;
        using impl::consume_t<PackageManager, winrt::Windows::Management::Deployment::IPackageManager3>::StageUserDataAsync;
    };
    struct WINRT_IMPL_EMPTY_BASES PackageManagerDebugSettings : winrt::Windows::Management::Deployment::IPackageManagerDebugSettings
    {
        PackageManagerDebugSettings(std::nullptr_t) noexcept {}
        PackageManagerDebugSettings(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Windows::Management::Deployment::IPackageManagerDebugSettings(ptr, take_ownership_from_abi) {}
    };
    struct WINRT_IMPL_EMPTY_BASES PackageUserInformation : winrt::Windows::Management::Deployment::IPackageUserInformation
    {
        PackageUserInformation(std::nullptr_t) noexcept {}
        PackageUserInformation(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Windows::Management::Deployment::IPackageUserInformation(ptr, take_ownership_from_abi) {}
    };
    struct WINRT_IMPL_EMPTY_BASES PackageVolume : winrt::Windows::Management::Deployment::IPackageVolume,
        impl::require<PackageVolume, winrt::Windows::Management::Deployment::IPackageVolume2>
    {
        PackageVolume(std::nullptr_t) noexcept {}
        PackageVolume(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Windows::Management::Deployment::IPackageVolume(ptr, take_ownership_from_abi) {}
    };
    struct WINRT_IMPL_EMPTY_BASES RegisterPackageOptions : winrt::Windows::Management::Deployment::IRegisterPackageOptions,
        impl::require<RegisterPackageOptions, winrt::Windows::Management::Deployment::IRegisterPackageOptions2>
    {
        RegisterPackageOptions(std::nullptr_t) noexcept {}
        RegisterPackageOptions(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Windows::Management::Deployment::IRegisterPackageOptions(ptr, take_ownership_from_abi) {}
        RegisterPackageOptions();
    };
    struct WINRT_IMPL_EMPTY_BASES RemovePackageOptions : winrt::Windows::Management::Deployment::IRemovePackageOptions
    {
        RemovePackageOptions(std::nullptr_t) noexcept {}
        RemovePackageOptions(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Windows::Management::Deployment::IRemovePackageOptions(ptr, take_ownership_from_abi) {}
        RemovePackageOptions();
    };
    struct WINRT_IMPL_EMPTY_BASES SharedPackageContainer : winrt::Windows::Management::Deployment::ISharedPackageContainer
    {
        SharedPackageContainer(std::nullptr_t) noexcept {}
        SharedPackageContainer(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Windows::Management::Deployment::ISharedPackageContainer(ptr, take_ownership_from_abi) {}
    };
    struct WINRT_IMPL_EMPTY_BASES SharedPackageContainerManager : winrt::Windows::Management::Deployment::ISharedPackageContainerManager
    {
        SharedPackageContainerManager(std::nullptr_t) noexcept {}
        SharedPackageContainerManager(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Windows::Management::Deployment::ISharedPackageContainerManager(ptr, take_ownership_from_abi) {}
        static auto GetDefault();
        static auto GetForUser(param::hstring const& userSid);
        static auto GetForProvisioning();
    };
    struct WINRT_IMPL_EMPTY_BASES SharedPackageContainerMember : winrt::Windows::Management::Deployment::ISharedPackageContainerMember
    {
        SharedPackageContainerMember(std::nullptr_t) noexcept {}
        SharedPackageContainerMember(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Windows::Management::Deployment::ISharedPackageContainerMember(ptr, take_ownership_from_abi) {}
        explicit SharedPackageContainerMember(param::hstring const& packageFamilyName);
    };
    struct WINRT_IMPL_EMPTY_BASES StagePackageOptions : winrt::Windows::Management::Deployment::IStagePackageOptions,
        impl::require<StagePackageOptions, winrt::Windows::Management::Deployment::IStagePackageOptions2>
    {
        StagePackageOptions(std::nullptr_t) noexcept {}
        StagePackageOptions(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Windows::Management::Deployment::IStagePackageOptions(ptr, take_ownership_from_abi) {}
        StagePackageOptions();
    };
    struct WINRT_IMPL_EMPTY_BASES UpdateSharedPackageContainerOptions : winrt::Windows::Management::Deployment::IUpdateSharedPackageContainerOptions
    {
        UpdateSharedPackageContainerOptions(std::nullptr_t) noexcept {}
        UpdateSharedPackageContainerOptions(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Windows::Management::Deployment::IUpdateSharedPackageContainerOptions(ptr, take_ownership_from_abi) {}
        UpdateSharedPackageContainerOptions();
    };
    struct WINRT_IMPL_EMPTY_BASES UpdateSharedPackageContainerResult : winrt::Windows::Management::Deployment::IUpdateSharedPackageContainerResult
    {
        UpdateSharedPackageContainerResult(std::nullptr_t) noexcept {}
        UpdateSharedPackageContainerResult(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Windows::Management::Deployment::IUpdateSharedPackageContainerResult(ptr, take_ownership_from_abi) {}
    };
}
#endif
