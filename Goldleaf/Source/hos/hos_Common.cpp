#include <hos/hos_Common.hpp>
#include <hos/hos_Titles.hpp>
#include <fs/fs_Explorer.hpp>
#include <sstream>

namespace hos
{
    u32 GetBatteryLevel()
    {
        u32 bat = 0;
        psmGetBatteryChargePercentage(&bat);
        return bat;
    }

    bool IsCharging()
    {
        ChargerType charger = ChargerType_None;
        psmGetChargerType(&charger);
        return (charger > ChargerType_None);
    }

    std::string GetCurrentTime()
    {
        time_t timet = time(NULL);
        struct tm *times = localtime((const time_t*)&timet);
        int h = times->tm_hour;
        int min = times->tm_min;
        int s = times->tm_sec;
        char timestr[0x10];
        sprintf(timestr, "%02d:%02d:%02d", h, min, s);
        return std::string(timestr);
    }

    FwVersion GetFwVersion()
    {
        FwVersion pfw = { 0, 0, 0, "" };
        SetSysFirmwareVersion fw;
        Result rc = setsysGetFirmwareVersion(&fw);
        if(rc != 0) return pfw;
        pfw.Major = fw.major;
        pfw.Minor = fw.minor;
        pfw.Micro = fw.micro;
        pfw.DisplayName = std::string(fw.display_title);
        return pfw;
    }

    std::string FwVersion::ToString()
    {
        return (std::to_string(this->Major) + "." + std::to_string(this->Minor) + "." + std::to_string(this->Micro));
    }

    std::string FormatHex128(u128 Number)
    {
        u8 *ptr = (u8*)&Number;
        std::stringstream strm;
        for(u32 i = 0; i < 16; i++) strm << std::hex << std::uppercase << (int)ptr[i];
        return strm.str();
    }

    std::string DoubleToString(double Number)
    {
        std::stringstream strm;
        strm << Number;
        return strm.str();
    }

    std::string FormatTime(u64 Seconds)
    {
        u64 secs = Seconds;
        std::string base = std::to_string(secs) + "s";
        if(Seconds > 60)
        {
            auto divt = div(Seconds, 60);
            u64 mins = divt.quot;
            secs = divt.rem;
            base = std::to_string(mins) + "min";
            if(secs > 0) base += (" " + std::to_string(secs) + "s");
            if(mins >= 60)
            {
                auto divt2 = div(mins, 60);
                u64 hrs = divt2.quot;
                mins = divt2.rem;
                base = std::to_string(hrs) + "h";
                if(mins > 0) base += (" " + std::to_string(mins) + "min");
                if(secs > 0) base += (" " + std::to_string(secs) + "s");
                if(hrs >= 24)
                {
                    auto divt3 = div(hrs, 24);
                    u64 days = divt3.quot;
                    hrs = divt3.rem;
                    base = std::to_string(days) + "d";
                    if(hrs > 0) base += (" " + std::to_string(hrs) + "h");
                    if(mins > 0) base += (" " + std::to_string(mins) + "min");
                    if(secs > 0) base += (" " + std::to_string(secs) + "s");
                    if(days >= 7)
                    {
                        auto divt4 = div(days, 7);
                        u64 weeks = divt4.quot;
                        days = divt4.rem;
                        base = std::to_string(weeks) + "w";
                        if(days > 0) base += (" " + std::to_string(days) + "d");
                        if(hrs > 0) base += (" " + std::to_string(hrs) + "h");
                        if(mins > 0) base += (" " + std::to_string(mins) + "min");
                        if(secs > 0) base += (" " + std::to_string(secs) + "s");
                        if(weeks >= 52)
                        {
                            auto divt5 = div(weeks, 52);
                            u64 years = divt5.quot;
                            weeks = divt5.rem;
                            base = std::to_string(years) + "y";
                            if(days > 0) base += (" " + std::to_string(weeks) + "w");
                            if(days > 0) base += (" " + std::to_string(days) + "d");
                            if(hrs > 0) base += (" " + std::to_string(hrs) + "h");
                            if(mins > 0) base += (" " + std::to_string(mins) + "min");
                            if(secs > 0) base += (" " + std::to_string(secs) + "s");
                        }
                    }
                }
            }
        }
        return base;
    }

    u64 GetSdCardFreeSpaceForInstalls()
    {
        return fs::GetFreeSpaceForPartition(fs::Partition::SdCard);
    }

    u64 GetNANDFreeSpaceForInstalls()
    {
        return fs::GetFreeSpaceForPartition(fs::Partition::NANDUser);
    }

    void IRAMWrite(void *Data, uintptr_t IRAMAddress, size_t Size)
    {
        u8 *block = new (std::align_val_t(0x1000)) u8[0x1000]();
        memcpy(block, Data, Size);
        SecmonArgs args = {};
        args.X[0] = 0xF0000201;
        args.X[1] = (uintptr_t)block;
        args.X[2] = IRAMAddress;
        args.X[3] = Size;
        args.X[4] = 1;
        svcCallSecureMonitor(&args);
        memcpy(Data, block, Size);
        operator delete[](block, std::align_val_t(0x1000));
    }

    void IRAMClear()
    {
        u8 *block = new (std::align_val_t(0x1000)) u8[0x1000]();
        memset(block, 0xFF, 0x1000);
        for(u32 i = 0; i < MaxPayloadSize; i += 0x1000) IRAMWrite(block, (IRAMPayloadBaseAddress + i), 0x1000);
        operator delete[](block, std::align_val_t(0x1000));
    }

    void PayloadProcess(pu::String Path)
    {
        u8 *block = new (std::align_val_t(0x1000)) u8[MaxPayloadSize]();
        auto fexp = fs::GetExplorerForMountName(fs::GetPathRoot(Path));
        auto size = fexp->GetFileSize(Path);
        if((size == 0) || (size > MaxPayloadSize)) return;
        fexp->ReadFileBlock(Path, 0, size, block);

        IRAMClear();
        for(u32 i = 0; i < MaxPayloadSize; i += 0x1000) IRAMWrite(&block[i], (IRAMPayloadBaseAddress + i), 0x1000);
        operator delete[](block, std::align_val_t(0x1000));

        splSetConfig((SplConfigItem)65001, 2);
    }

    void LockAutoSleep()
    {
        if(GetLaunchMode() == LaunchMode::Application) appletBeginBlockingHomeButton(0);
        appletSetMediaPlaybackState(true);
    }

    void UnlockAutoSleep()
    {
        appletSetMediaPlaybackState(false);
        if(GetLaunchMode() == LaunchMode::Application) appletEndBlockingHomeButton();
    }
}