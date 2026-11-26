#define IC_SDK_INTERNAL 1
#include "Interceptor.h"
namespace Interceptor {Init_Result Restore_Controller_Hid_Filters();}
#include <SetupAPI.h>
#include <newdev.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <devguid.h>
#include <string>
#include <vector>
#include <set>
#include <iostream>
#include <system_error>
#include "SetupBinding.h"
#include "SetupHealth.h"

namespace {
void Check(BOOL ok,const char* operation) { if(!ok)throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),operation); }
struct Devices { HDEVINFO value;~Devices(){if(value!=INVALID_HANDLE_VALUE)SetupDiDestroyDeviceInfoList(value);} };
struct Target { std::wstring instance,hardware,inf,service;bool mouse,filtered; };
bool HasInputFilter(HDEVINFO set,SP_DEVINFO_DATA& device) {
    return SetupHealth::Has(SetupHealth::Strings(set,device,SPDRP_UPPERFILTERS),IC_SDK_SERVICE);
}
bool UsesInputPackage(HDEVINFO set,SP_DEVINFO_DATA& device,bool mouse,const wchar_t* windows) {
    const auto inf=Interceptor::SetupDetail::InstalledMapperProperty(set,device,L"InfPath");
    if(inf.empty() || inf.find_first_of(L"\\/:")!=std::wstring::npos)return false;
    const auto path=std::wstring(windows)+L"\\INF\\"+inf;
    return Interceptor::SetupDetail::IsInputPackageInf(path.c_str(),mouse);
}
bool RemoveInputFilter(HDEVINFO set,SP_DEVINFO_DATA& device) {
    const auto filters=SetupHealth::Strings(set,device,SPDRP_UPPERFILTERS);
    if(!SetupHealth::Has(filters,IC_SDK_SERVICE))return false;
    std::vector<wchar_t> data;
    for(const auto& name:filters)if(_wcsicmp(name.c_str(),IC_SDK_SERVICE)) {
        data.insert(data.end(),name.begin(),name.end());
        data.push_back(L'\0');
    }
    if(!data.empty())data.push_back(L'\0');
    Check(SetupDiSetDeviceRegistryPropertyW(set,&device,SPDRP_UPPERFILTERS,
        data.empty()?nullptr:reinterpret_cast<const BYTE*>(data.data()),
        static_cast<DWORD>(data.size()*sizeof(wchar_t))),"Remove Interceptor input filter");
    return true;
}
bool RecheckMapper(HDEVINFO set,SP_DEVINFO_DATA& device,const Target& target,bool restore,bool onlyNew) {
    const auto& expectedClass=target.mouse?GUID_DEVCLASS_MOUSE:GUID_DEVCLASS_KEYBOARD;
    if(!IsEqualGUID(device.ClassGuid,expectedClass))throw std::system_error(ERROR_RETRY,std::system_category(),"Mapper class changed");
    wchar_t hardware[4096]{},service[256]{};DWORD type=0;
    Check(SetupDiGetDeviceRegistryPropertyW(set,&device,SPDRP_HARDWAREID,&type,reinterpret_cast<BYTE*>(hardware),sizeof(hardware)-2*sizeof(wchar_t),nullptr),"Recheck mapper hardware ID");
    if(type!=REG_MULTI_SZ || _wcsicmp(hardware,target.hardware.c_str()))throw std::system_error(ERROR_RETRY,std::system_category(),"Mapper identity changed");
    Check(SetupDiGetDeviceRegistryPropertyW(set,&device,SPDRP_SERVICE,&type,reinterpret_cast<BYTE*>(service),sizeof(service)-sizeof(wchar_t),nullptr),"Recheck mapper service");
    if(type!=REG_SZ)throw std::system_error(ERROR_INVALID_DATA,std::system_category(),"Invalid mapper service property");
    if(_wcsicmp(service,target.service.c_str()))return false;
    if(target.filtered!=HasInputFilter(set,device))return false;
    const bool ours=!_wcsicmp(service,IC_SDK_SERVICE);
    return restore?(ours || target.filtered):(!onlyNew || !ours);
}
constexpr wchar_t ControlHardware[]=L"ROOT\\InterceptorKeyboardMouse";
constexpr wchar_t ControlInstance[]=L"ROOT\\InterceptorKeyboardMouse\\0000";
bool IsControl(HDEVINFO set,SP_DEVINFO_DATA& d) {
    wchar_t id[MAX_DEVICE_ID_LEN]{},hardware[4096]{};DWORD type=0;
    Check(SetupDiGetDeviceInstanceIdW(set,&d,id,MAX_DEVICE_ID_LEN,nullptr),"Read control instance");
    if(_wcsnicmp(id,L"ROOT\\InterceptorKeyboardMouse\\",_countof(ControlHardware)))return false;
    if(!SetupDiGetDeviceRegistryPropertyW(set,&d,SPDRP_HARDWAREID,&type,reinterpret_cast<BYTE*>(hardware),sizeof(hardware)-2*sizeof(wchar_t),nullptr)) {
        if(GetLastError()==ERROR_INVALID_DATA)return false;Check(FALSE,"Read control hardware ID");
    }
    if(type!=REG_MULTI_SZ)return false;
    for(const auto* p=hardware;*p;p+=wcslen(p)+1)if(!_wcsicmp(p,ControlHardware))return true;
    return false;
}
bool IsUnboundControlStub(HDEVINFO set,SP_DEVINFO_DATA& device,const wchar_t* instance) {
    if(_wcsicmp(instance,ControlInstance) || !IsEqualGUID(device.ClassGuid,GUID_DEVCLASS_SYSTEM))return false;
    for(const auto property:{SPDRP_HARDWAREID,SPDRP_COMPATIBLEIDS,SPDRP_SERVICE,SPDRP_DRIVER,SPDRP_UPPERFILTERS,SPDRP_LOWERFILTERS}) {
        if(!SetupHealth::Strings(set,device,property).empty())return false;
    }
    return true;
}
void SetControlHardwareId(HDEVINFO set,SP_DEVINFO_DATA& device) {
    constexpr wchar_t hardware[]=L"ROOT\\InterceptorKeyboardMouse\0";
    Check(SetupDiSetDeviceRegistryPropertyW(set,&device,SPDRP_HARDWAREID,
        reinterpret_cast<const BYTE*>(hardware),sizeof(hardware)),"Set root control hardware ID");
}
bool RemoveControlDevice(HDEVINFO set,SP_DEVINFO_DATA& d) {
    SP_REMOVEDEVICE_PARAMS remove{};remove.ClassInstallHeader.cbSize=sizeof(SP_CLASSINSTALL_HEADER);
    remove.ClassInstallHeader.InstallFunction=DIF_REMOVE;remove.Scope=DI_REMOVEDEVICE_GLOBAL;
    Check(SetupDiSetClassInstallParamsW(set,&d,&remove.ClassInstallHeader,sizeof(remove)),"Prepare root control removal");
    Check(SetupDiCallClassInstaller(DIF_REMOVE,set,&d),"Remove root control device");
    SP_DEVINSTALL_PARAMS_W params{};params.cbSize=sizeof(params);
    Check(SetupDiGetDeviceInstallParamsW(set,&d,&params),"Query root removal restart");
    return (params.Flags&(DI_NEEDREBOOT|DI_NEEDRESTART))!=0;
}
bool ConfigureControl(const wchar_t* inf,bool remove) {
    Devices scan{SetupDiGetClassDevsW(nullptr,nullptr,nullptr,DIGCF_ALLCLASSES|(remove?0:DIGCF_PRESENT))};
    Check(scan.value!=INVALID_HANDLE_VALUE,"Enumerate root control devices");
    std::vector<std::wstring> ids;
    bool foundStub=false;
    for(DWORD index=0;;++index) {
        SP_DEVINFO_DATA d{};d.cbSize=sizeof(d);
        if(!SetupDiEnumDeviceInfo(scan.value,index,&d)){if(GetLastError()==ERROR_NO_MORE_ITEMS)break;Check(FALSE,"Enumerate root control device");}
        wchar_t id[MAX_DEVICE_ID_LEN]{};
        if(!SetupDiGetDeviceInstanceIdW(scan.value,&d,id,MAX_DEVICE_ID_LEN,nullptr)) {
            if(GetLastError()==ERROR_NO_SUCH_DEVINST)continue;
            Check(FALSE,"Read root control ID");
        }
        if(IsControl(scan.value,d))ids.emplace_back(id);
        else if(!remove && IsUnboundControlStub(scan.value,d,id))foundStub=true;
    }
    Devices set{SetupDiCreateDeviceInfoList(nullptr,nullptr)};
    Check(set.value!=INVALID_HANDLE_VALUE,"Create root control information set");
    bool reboot=false;
    if(remove) {
        for(const auto& id:ids) {
            SP_DEVINFO_DATA d{};d.cbSize=sizeof(d);Check(SetupDiOpenDeviceInfoW(set.value,id.c_str(),nullptr,0,&d),"Open root control device");
            reboot|=RemoveControlDevice(set.value,d);
        }
        std::cout<<ids.size()<<" root control devices removed\n";return reboot;
    }
    SP_DEVINFO_DATA device{};device.cbSize=sizeof(device);
    bool registered=false,repairedStub=false;
    try {
        if(ids.empty()) {
            if(foundStub && SetupDiOpenDeviceInfoW(set.value,ControlInstance,nullptr,0,&device)
                && IsUnboundControlStub(set.value,device,ControlInstance)) {
                SetControlHardwareId(set.value,device);
                repairedStub=true;
                std::cout<<"Recovered the previously unbound root control device.\n";
            }else{
                device={};device.cbSize=sizeof(device);
                if(!SetupDiCreateDeviceInfoW(set.value,ControlInstance,&GUID_DEVCLASS_SYSTEM,L"Interceptor Control",nullptr,0,&device)) {
                    if(GetLastError()!=ERROR_DEVINST_ALREADY_EXISTS)Check(FALSE,"Create root control device");
                    // A non-present or foreign instance occupies the stable ID.
                    // Reuse any present owned instance on subsequent runs.
                    constexpr wchar_t rootName[]=L"InterceptorKeyboardMouse";
                    Check(SetupDiCreateDeviceInfoW(set.value,rootName,&GUID_DEVCLASS_SYSTEM,L"Interceptor Control",
                        nullptr,DICD_GENERATE_ID,&device),"Allocate a free root control instance");
                }
                SetControlHardwareId(set.value,device);
                Check(SetupDiCallClassInstaller(DIF_REGISTERDEVICE,set.value,&device),"Register root control device");
                registered=true;
            }
        }
        BOOL needsRestart=FALSE;
        Check(UpdateDriverForPlugAndPlayDevicesW(nullptr,ControlHardware,inf,INSTALLFLAG_FORCE|INSTALLFLAG_NONINTERACTIVE,&needsRestart),"Bind root control driver");
        reboot=needsRestart!=FALSE;
    } catch(...) {
        if(registered)try {RemoveControlDevice(set.value,device);}catch(const std::exception& e){std::cerr<<"Root rollback failed: "<<e.what()<<'\n';}
        if(repairedStub && !SetupDiSetDeviceRegistryPropertyW(set.value,&device,SPDRP_HARDWAREID,nullptr,0))
            std::cerr<<"Could not restore the original root hardware property; Windows error "<<GetLastError()<<'\n';
        throw;
    }
    std::cout<<"Root control configured; reboot="<<reboot<<'\n';return reboot;
}
}

int BindInputMapper(const wchar_t* instance,const wchar_t* fromService,const wchar_t* fromInf,
                    const wchar_t* destination,const wchar_t* desiredService) {
    if(_wcsnicmp(instance,L"HID\\",4))return ERROR_INVALID_PARAMETER;
    Devices set{SetupDiCreateDeviceInfoList(nullptr,nullptr)};
    Check(set.value!=INVALID_HANDLE_VALUE,"Open input setup information set");
    SP_DEVINFO_DATA device{};device.cbSize=sizeof(device);
    Check(SetupDiOpenDeviceInfoW(set.value,instance,nullptr,0,&device),"Open input instance");
    const bool mouse=IsEqualGUID(device.ClassGuid,GUID_DEVCLASS_MOUSE);
    if(!mouse && !IsEqualGUID(device.ClassGuid,GUID_DEVCLASS_KEYBOARD))return ERROR_INVALID_PARAMETER;
    const auto* stock=mouse?L"mouhid":L"kbdhid";
    const bool intercept=!_wcsicmp(desiredService,IC_SDK_SERVICE);
    if((intercept && _wcsicmp(fromService,stock)) ||
       (!intercept && (_wcsicmp(fromService,IC_SDK_SERVICE) || _wcsicmp(desiredService,stock))))return ERROR_INVALID_PARAMETER;
    const auto stillCurrent=[&]{
        const auto service=SetupHealth::Strings(set.value,device,SPDRP_SERVICE);
        const auto hardware=SetupHealth::Strings(set.value,device,SPDRP_HARDWAREID);
        const auto inf=Interceptor::SetupDetail::InstalledMapperProperty(set.value,device,L"InfPath");
        return service.size()==1 && !_wcsicmp(service.front().c_str(),fromService) &&
            !hardware.empty() && !_wcsnicmp(hardware.front().c_str(),L"HID\\",4) &&
            !_wcsicmp(inf.c_str(),fromInf);
    };
    if(!stillCurrent())return ERROR_RETRY;
    if(intercept){
        const auto profile=Interceptor::SetupDetail::ProbeMapperProfile(set.value,device,mouse);
        if(!profile.supported)return ERROR_NOT_SUPPORTED;
        if(!stillCurrent())return ERROR_RETRY;
    }
    return Interceptor::SetupDetail::BindInstance(set.value,device,destination)?ERROR_SUCCESS_REBOOT_REQUIRED:ERROR_SUCCESS;
}

int ConfigureControlDriver(const wchar_t* inf,bool remove) {return ConfigureControl(inf,remove)?ERROR_SUCCESS_REBOOT_REQUIRED:0;}
int InstallDriver(const wchar_t* directory,bool restore,bool onlyNew,const wchar_t* mouseInf,const wchar_t* controlInf) {
    const GUID* classes[]={&GUID_DEVCLASS_KEYBOARD,&GUID_DEVCLASS_MOUSE};
    std::vector<Target> targets;std::set<std::wstring> seen;
    wchar_t windows[MAX_PATH]{};Check(GetWindowsDirectoryW(windows,MAX_PATH)!=0,"GetWindowsDirectory");
    // Snapshot all devices before changes: updating one device may invalidate enumeration.
    for(unsigned kind=0;kind<2;++kind) {
        Devices devices{SetupDiGetClassDevsW(classes[kind],nullptr,nullptr,restore?0:DIGCF_PRESENT)};
        Check(devices.value!=INVALID_HANDLE_VALUE,"Enumerate devices");
        for(DWORD i=0;;++i) {
            SP_DEVINFO_DATA data{};data.cbSize=sizeof(data);
            if(!SetupDiEnumDeviceInfo(devices.value,i,&data)) { if(GetLastError()==ERROR_NO_MORE_ITEMS)break;Check(FALSE,"Enumerate device"); }
            wchar_t hardware[4096]{},service[256]{};DWORD type=0;
            if(!SetupDiGetDeviceRegistryPropertyW(devices.value,&data,SPDRP_HARDWAREID,&type,reinterpret_cast<BYTE*>(hardware),sizeof(hardware)-2*sizeof(wchar_t),nullptr))continue;
            if(_wcsnicmp(hardware,L"HID\\",4))continue;
            if(!SetupDiGetDeviceRegistryPropertyW(devices.value,&data,SPDRP_SERVICE,&type,reinterpret_cast<BYTE*>(service),sizeof(service)-sizeof(wchar_t),nullptr))continue;
            if(!restore && !_wcsicmp(service,L"Interceptor"))throw std::system_error(ERROR_ALREADY_EXISTS,std::system_category(),"Original Interceptor owns this device; uninstall it before installing this split product");
            const bool legacy=!_wcsicmp(service,IC_SDK_SERVICE);
            const bool filtered=HasInputFilter(devices.value,data);
            if(onlyNew && legacy)continue;
            if(restore) { if(!legacy && !filtered)continue; }
            else if(_wcsicmp(service,kind?L"mouhid":L"kbdhid") && !legacy) {
                std::wcerr<<L"Skipped custom HID driver: "<<hardware<<L" ("<<service<<L")\n";continue;
            }
            wchar_t instance[MAX_DEVICE_ID_LEN]{};
            Check(SetupDiGetDeviceInstanceIdW(devices.value,&data,instance,MAX_DEVICE_ID_LEN,nullptr),"Read mapper instance ID");
            if(!restore && Interceptor::SetupDetail::IsMapperPaused(instance,IC_SDK_SERVICE))continue;
            if(!seen.insert(instance).second)continue;
            std::wstring inf;
            if(onlyNew)inf=kind?mouseInf:directory;
            else {
                inf=restore?std::wstring(windows)+L"\\INF":directory;
                inf+=restore?(kind?L"\\msmouse.inf":L"\\keyboard.inf"):(kind?L"\\InterceptorKeyboardMouseMouse.inf":L"\\InterceptorKeyboardMouseKeyboard.inf");
            }
            targets.push_back({instance,hardware,inf,service,kind!=0,filtered});
        }
    }
    bool needsReboot=false;
    if(!restore && (!onlyNew || controlInf)) {
        const auto path=onlyNew?std::wstring(controlInf):std::wstring(directory)+L"\\InterceptorKeyboardMouseControl.inf";
        needsReboot=ConfigureControl(path.c_str(),false);
    }
    for(const auto& target:targets) {
        Devices devices{SetupDiCreateDeviceInfoList(nullptr,nullptr)};
        Check(devices.value!=INVALID_HANDLE_VALUE,"Open mapper information set");
        SP_DEVINFO_DATA data{};data.cbSize=sizeof(data);
        if(!SetupDiOpenDeviceInfoW(devices.value,target.instance.c_str(),nullptr,0,&data)) {
            const DWORD error=GetLastError();
            if(restore && error==ERROR_NO_SUCH_DEVINST)continue;
            throw std::system_error(static_cast<int>(error),std::system_category(),"Open mapper instance");
        }
        // Recheck ownership after the snapshot: another identical device with
        // a custom driver must not be changed by a hardware-ID-wide update.
        if(!RecheckMapper(devices.value,data,target,restore,onlyNew)) {
            std::wcout<<L"Skipped changed mapper instance: "<<target.instance<<L"\n";continue;
        }
        // The profile is inspected through the Windows HID mapper before a
        // new association. An existing Interceptor mapper does not expose that
        // metadata route; an unchanged binding can still update its INF. The
        // service snapshot above prevents skipping this probe after a race back
        // to kbdhid/mouhid, or adopting a binding changed by another installer.
        if(!restore && _wcsicmp(target.service.c_str(),IC_SDK_SERVICE) &&
            !Interceptor::SetupDetail::UsesInboxInputMapper(devices.value,data,target.mouse) &&
            !(target.filtered && UsesInputPackage(devices.value,data,target.mouse,windows))) {
            std::wcout<<L"Manufacturer HID package requires a recorded original binding: "<<target.instance<<L"\n";continue;
        }
        if(!restore && !target.filtered && _wcsicmp(target.service.c_str(),IC_SDK_SERVICE)) {
            const auto profile=Interceptor::SetupDetail::ProbeMapperProfile(devices.value,data,target.mouse);
            if(!profile.supported) {
                std::wcout<<L"Skipped mapper association; current driver unchanged: "<<target.instance
                    <<L"; "<<profile.reason<<L"; error="<<profile.error<<L"\n";continue;
            }
            if(!RecheckMapper(devices.value,data,target,restore,onlyNew) ||
                !Interceptor::SetupDetail::UsesInboxInputMapper(devices.value,data,target.mouse)) {
                std::wcout<<L"Skipped changed mapper instance after profile inspection: "<<target.instance<<L"\n";continue;
            }
        }
        const auto displayName=restore?Interceptor::SetupDetail::MapperDisplayName{}:
            Interceptor::SetupDetail::ReadMapperDisplayName(devices.value,data,
                _wcsicmp(target.service.c_str(),L"InterceptorKeyboardMouseRelease")!=0);
        bool reboot=false;
        try {
            if(restore || target.filtered) {
                const bool removed=RemoveInputFilter(devices.value,data);
                ULONG flags=0,problem=0;
                reboot=removed && CM_Get_DevNode_Status(&flags,&problem,data.DevInst,0)==CR_SUCCESS;
            }
            // An intervening OEM package can retain an UpperFilters entry.
            // Removing our entry suffices on a device with a custom service.
            if(!restore || !_wcsicmp(target.service.c_str(),IC_SDK_SERVICE) ||
                UsesInputPackage(devices.value,data,target.mouse,windows))
                reboot=Interceptor::SetupDetail::BindInstance(devices.value,data,target.inf.c_str()) || reboot;
        }
        catch(const std::system_error& e) {
            // Package removal also restores nonpresent instances.
            if(restore && e.code().value()==ERROR_NO_SUCH_DEVINST)continue;
            throw;
        }
        const auto nameError=restore?Interceptor::SetupDetail::ClearMapperDisplayName(devices.value,data):
            Interceptor::SetupDetail::KeepMapperDisplayName(devices.value,data,displayName);
        if(nameError!=ERROR_SUCCESS)
            std::wcerr<<L"Could not update the original HID display name for "<<target.instance<<L"; error="<<nameError<<L"\n";
        needsReboot|=reboot;
        std::wcout<<L"Configured "<<target.instance<<L"; reboot="<<reboot<<L"\n";
    }
    std::cout<<targets.size()<<" device instances processed\n";
    if(onlyNew && targets.empty())std::cout<<"No additional supported HID devices to associate.\n";
    if(restore)needsReboot|=ConfigureControl(nullptr,true);
    return needsReboot?3010:0;
}
