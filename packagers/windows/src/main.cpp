#include <iostream>
#include <windows.h>
#include <fstream>
#include <vector>
#include <string>

#pragma pack(push, 1)
struct ICONDIR
{
    WORD idReserved;
    WORD idType;
    WORD idCount;
};

struct ICONDIRENTRY
{
    BYTE bWidth;
    BYTE bHeight;
    BYTE bColorCount;
    BYTE bReserved;
    WORD wPlanes;
    WORD wBitCount;
    DWORD dwBytesInRes;
    DWORD dwImageOffset;
};

struct VersionInfo
{
    std::wstring companyName;
    std::wstring fileDescription;
    std::wstring fileVersion;
    std::wstring productName;
    std::wstring productVersion;
    WORD fileVersionParts[4];
    WORD productVersionParts[4];
};

#pragma pack(pop)

namespace ezi::builder::packager
{
    namespace utils
    {
        void ShowErrorAndExit(const std::string &message = "")
        {
            std::cerr << message << " Error code: " << GetLastError() << std::endl;
            exit(EXIT_FAILURE);
        }
        void PadToDword(std::vector<BYTE> &data)
        {
            while (data.size() % 4)
                data.push_back(0);
        }
    }

    class ResourceUpdater
    {
    private:
        HANDLE hUpdate;

    public:
        ResourceUpdater(const std::string &exePath)
            : hUpdate(BeginUpdateResourceA(exePath.c_str(), FALSE))
        {
            if (!hUpdate)
                utils::ShowErrorAndExit("BeginUpdateResource failed.");
        }

        ~ResourceUpdater()
        {
            if (hUpdate)
            {
                EndUpdateResourceA(hUpdate, TRUE);
            }
        }

    private:
        void updateResource(WORD resourceType, WORD resourceName, std::vector<char> &data)
        {
            if (!UpdateResourceA(hUpdate, MAKEINTRESOURCEA(resourceType), MAKEINTRESOURCEA(resourceName), 1033, data.data(), data.size()))
                utils::ShowErrorAndExit("UpdateResource failed.");
        }

    public:
        void finalize()
        {
            if (hUpdate)
            {
                if (!EndUpdateResourceA(hUpdate, FALSE))
                    utils::ShowErrorAndExit("EndUpdateResource failed.");
                else
                    std::cout << "Resources updated successfully." << std::endl;
                hUpdate = nullptr;
            }
        }
        void updateAsset(std::string filePath)
        {
            std::ifstream file(filePath, std::ios::binary);
            if (!file)
                utils::ShowErrorAndExit("Failed to open asset file.");
            std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            updateResource(10, 1004, data);
        }
        void updateIcon(const std::string &iconPath)
        {
            std::ifstream file(iconPath, std::ios::binary);
            if (!file)
                utils::ShowErrorAndExit("Failed to open icon file.");

            std::vector<char> icoData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            if (icoData.size() < sizeof(ICONDIR))
                utils::ShowErrorAndExit("Invalid .ico file.");

            const ICONDIR *iconDir = reinterpret_cast<const ICONDIR *>(icoData.data());
            if (iconDir->idType != 1 || iconDir->idCount == 0)
                utils::ShowErrorAndExit("Not a valid icon file.");

            struct GRPICONDIR
            {
                WORD idReserved;
                WORD idType;
                WORD idCount;
            } grpDir = {0, 1, iconDir->idCount};

            std::vector<char> groupData;
            groupData.insert(groupData.end(), reinterpret_cast<char *>(&grpDir), reinterpret_cast<char *>(&grpDir) + sizeof(GRPICONDIR));

            const ICONDIRENTRY *entries = reinterpret_cast<const ICONDIRENTRY *>(icoData.data() + sizeof(ICONDIR));
            WORD iconBaseID = 1;

            for (int i = 0; i < iconDir->idCount; ++i)
            {
                const ICONDIRENTRY &entry = entries[i];
                const char *imageData = icoData.data() + entry.dwImageOffset;

                std::vector<char> iconImage(entry.dwBytesInRes);
                std::memcpy(iconImage.data(), imageData, entry.dwBytesInRes);
                updateResource(3, iconBaseID + i, iconImage);

                struct GRPICONDIRENTRY
                {
                    BYTE bWidth;
                    BYTE bHeight;
                    BYTE bColorCount;
                    BYTE bReserved;
                    WORD wPlanes;
                    WORD wBitCount;
                    DWORD dwBytesInRes;
                    WORD nID;
                } grpEntry;

                std::memcpy(&grpEntry, &entry, sizeof(GRPICONDIRENTRY) - sizeof(WORD));
                grpEntry.nID = iconBaseID + i;

                groupData.insert(groupData.end(), reinterpret_cast<char *>(&grpEntry), reinterpret_cast<char *>(&grpEntry) + sizeof(GRPICONDIRENTRY));
            }

            updateResource(14, 1, groupData);
        }
        void updateVersionInfo(const VersionInfo info)
        {
            std::vector<BYTE> stringTableChildren;
            {
                auto makeStringBlock = [](const std::wstring &key, const std::wstring &value) -> std::vector<BYTE>
                {
                    std::vector<BYTE> block;
                    WORD wType = 1; // text

                    size_t keyChars = key.size() + 1;     // include null
                    size_t valueChars = value.size() + 1; // include null

                    // header: wLength, wValueLength, wType
                    // compute header bytes then pad to dword before value
                    size_t headerBytes = sizeof(WORD) * 3 + keyChars * sizeof(WCHAR);
                    size_t paddedHeader = ((headerBytes + 3) / 4) * 4;
                    size_t valueBytes = valueChars * sizeof(WCHAR);

                    WORD wLength = static_cast<WORD>(paddedHeader + valueBytes);
                    WORD wValueLength = static_cast<WORD>(valueChars - 1); // number of characters excluding terminating null

                    block.insert(block.end(), reinterpret_cast<BYTE *>(&wLength), reinterpret_cast<BYTE *>(&wLength) + sizeof(WORD));
                    block.insert(block.end(), reinterpret_cast<BYTE *>(&wValueLength), reinterpret_cast<BYTE *>(&wValueLength) + sizeof(WORD));
                    block.insert(block.end(), reinterpret_cast<BYTE *>(&wType), reinterpret_cast<BYTE *>(&wType) + sizeof(WORD));

                    // key (WCHARs including null)
                    block.insert(block.end(), reinterpret_cast<const BYTE *>(key.c_str()), reinterpret_cast<const BYTE *>(key.c_str() + keyChars));
                    utils::PadToDword(block);

                    // value (WCHARs including null)
                    block.insert(block.end(), reinterpret_cast<const BYTE *>(value.c_str()), reinterpret_cast<const BYTE *>(value.c_str() + valueChars));
                    utils::PadToDword(block);

                    return block;
                };
                auto appendString = [&](const std::wstring &k, const std::wstring &v)
                {
                    auto b = makeStringBlock(k, v);
                    stringTableChildren.insert(stringTableChildren.end(), b.begin(), b.end());
                };

                appendString(L"CompanyName", info.companyName);
                appendString(L"FileDescription", info.fileDescription);
                appendString(L"FileVersion", info.fileVersion);
                appendString(L"ProductName", info.productName);
                appendString(L"ProductVersion", info.productVersion);
            }

            std::vector<BYTE> stringTable;
            {
                WORD wType = 1;
                std::wstring langCode = L"040904B0";
                size_t keyChars = langCode.size() + 1;
                size_t headerBytes = sizeof(WORD) * 3 + keyChars * sizeof(WCHAR);
                size_t paddedHeader = ((headerBytes + 3) / 4) * 4;
                size_t totalBytes = paddedHeader + stringTableChildren.size();

                WORD wLength = static_cast<WORD>(totalBytes);
                WORD wValueLength = 0;

                stringTable.insert(stringTable.end(), reinterpret_cast<BYTE *>(&wLength), reinterpret_cast<BYTE *>(&wLength) + sizeof(WORD));
                stringTable.insert(stringTable.end(), reinterpret_cast<BYTE *>(&wValueLength), reinterpret_cast<BYTE *>(&wValueLength) + sizeof(WORD));
                stringTable.insert(stringTable.end(), reinterpret_cast<BYTE *>(&wType), reinterpret_cast<BYTE *>(&wType) + sizeof(WORD));
                stringTable.insert(stringTable.end(), reinterpret_cast<const BYTE *>(langCode.c_str()), reinterpret_cast<const BYTE *>(langCode.c_str() + keyChars));
                utils::PadToDword(stringTable);
                stringTable.insert(stringTable.end(), stringTableChildren.begin(), stringTableChildren.end());
            }

            std::vector<BYTE> stringFileInfo;
            {
                WORD wType = 1;
                std::wstring key = L"StringFileInfo";
                size_t keyChars = key.size() + 1;
                size_t headerBytes = sizeof(WORD) * 3 + keyChars * sizeof(WCHAR);
                size_t paddedHeader = ((headerBytes + 3) / 4) * 4;
                size_t totalBytes = paddedHeader + stringTable.size();

                WORD wLength = static_cast<WORD>(totalBytes);
                WORD wValueLength = 0;

                stringFileInfo.insert(stringFileInfo.end(), reinterpret_cast<BYTE *>(&wLength), reinterpret_cast<BYTE *>(&wLength) + sizeof(WORD));
                stringFileInfo.insert(stringFileInfo.end(), reinterpret_cast<BYTE *>(&wValueLength), reinterpret_cast<BYTE *>(&wValueLength) + sizeof(WORD));
                stringFileInfo.insert(stringFileInfo.end(), reinterpret_cast<BYTE *>(&wType), reinterpret_cast<BYTE *>(&wType) + sizeof(WORD));
                stringFileInfo.insert(stringFileInfo.end(), reinterpret_cast<const BYTE *>(key.c_str()), reinterpret_cast<const BYTE *>(key.c_str() + keyChars));
                utils::PadToDword(stringFileInfo);
                stringFileInfo.insert(stringFileInfo.end(), stringTable.begin(), stringTable.end());
            }

            std::vector<BYTE> varChildren;
            {
                // Var structure: header + key "Translation" + padding + value (DWORD array)
                std::wstring varKey = L"Translation";
                WORD wType = 0; // binary
                size_t keyChars = varKey.size() + 1;
                size_t headerBytes = sizeof(WORD) * 3 + keyChars * sizeof(WCHAR);
                size_t paddedHeader = ((headerBytes + 3) / 4) * 4;
                size_t valueBytes = sizeof(DWORD);
                WORD wLength = static_cast<WORD>(paddedHeader + valueBytes);
                WORD wValueLength = static_cast<WORD>(valueBytes / sizeof(WORD));

                varChildren.insert(varChildren.end(), reinterpret_cast<BYTE *>(&wLength), reinterpret_cast<BYTE *>(&wLength) + sizeof(WORD));
                varChildren.insert(varChildren.end(), reinterpret_cast<BYTE *>(&wValueLength), reinterpret_cast<BYTE *>(&wValueLength) + sizeof(WORD));
                varChildren.insert(varChildren.end(), reinterpret_cast<BYTE *>(&wType), reinterpret_cast<BYTE *>(&wType) + sizeof(WORD));
                varChildren.insert(varChildren.end(), reinterpret_cast<const BYTE *>(varKey.c_str()), reinterpret_cast<const BYTE *>(varKey.c_str() + keyChars));
                utils::PadToDword(varChildren);
                DWORD translation = 0x040904B0;
                varChildren.insert(varChildren.end(), reinterpret_cast<BYTE *>(&translation), reinterpret_cast<BYTE *>(&translation) + sizeof(DWORD));
                utils::PadToDword(varChildren);
            }

            std::vector<BYTE> varFileInfo;
            {
                WORD wType = 0;
                std::wstring key = L"VarFileInfo";
                size_t keyChars = key.size() + 1;
                size_t headerBytes = sizeof(WORD) * 3 + keyChars * sizeof(WCHAR);
                size_t paddedHeader = ((headerBytes + 3) / 4) * 4;
                size_t totalBytes = paddedHeader + varChildren.size();

                WORD wLength = static_cast<WORD>(totalBytes);
                WORD wValueLength = 0;

                varFileInfo.insert(varFileInfo.end(), reinterpret_cast<BYTE *>(&wLength), reinterpret_cast<BYTE *>(&wLength) + sizeof(WORD));
                varFileInfo.insert(varFileInfo.end(), reinterpret_cast<BYTE *>(&wValueLength), reinterpret_cast<BYTE *>(&wValueLength) + sizeof(WORD));
                varFileInfo.insert(varFileInfo.end(), reinterpret_cast<BYTE *>(&wType), reinterpret_cast<BYTE *>(&wType) + sizeof(WORD));
                varFileInfo.insert(varFileInfo.end(), reinterpret_cast<const BYTE *>(key.c_str()), reinterpret_cast<const BYTE *>(key.c_str() + keyChars));
                utils::PadToDword(varFileInfo);
                varFileInfo.insert(varFileInfo.end(), varChildren.begin(), varChildren.end());
            }

            VS_FIXEDFILEINFO fixed = {};
            fixed.dwSignature = 0xFEEF04BD;
            fixed.dwStrucVersion = 0x00010000;
            fixed.dwFileVersionMS = (info.fileVersionParts[0] << 16) | info.fileVersionParts[1];
            fixed.dwFileVersionLS = (info.fileVersionParts[2] << 16) | info.fileVersionParts[3];
            fixed.dwProductVersionMS = (info.productVersionParts[0] << 16) | info.productVersionParts[1];
            fixed.dwProductVersionLS = (info.productVersionParts[2] << 16) | info.productVersionParts[3];
            fixed.dwFileFlagsMask = 0x3F;
            fixed.dwFileFlags = 0;
            fixed.dwFileOS = 0x40004; // VOS_NT_WINDOWS32
            fixed.dwFileType = 0x1;   // VFT_APP
            fixed.dwFileSubtype = 0;
            fixed.dwFileDateMS = 0;
            fixed.dwFileDateLS = 0;

            std::vector<BYTE> versionInfo;
            std::wstring rootKey = L"VS_VERSION_INFO";

            // compute root length
            size_t rootHeaderBytes = sizeof(WORD) * 3 + (rootKey.size() + 1) * sizeof(WCHAR);
            size_t rootPaddedHeader = ((rootHeaderBytes + 3) / 4) * 4;
            size_t totalRoot = rootPaddedHeader + sizeof(VS_FIXEDFILEINFO) + stringFileInfo.size() + varFileInfo.size();

            WORD rootLen = static_cast<WORD>(totalRoot);
            WORD rootValLen = static_cast<WORD>(sizeof(VS_FIXEDFILEINFO));
            WORD rootType = 0;

            versionInfo.insert(versionInfo.end(), reinterpret_cast<BYTE *>(&rootLen), reinterpret_cast<BYTE *>(&rootLen) + sizeof(WORD));
            versionInfo.insert(versionInfo.end(), reinterpret_cast<BYTE *>(&rootValLen), reinterpret_cast<BYTE *>(&rootValLen) + sizeof(WORD));
            versionInfo.insert(versionInfo.end(), reinterpret_cast<BYTE *>(&rootType), reinterpret_cast<BYTE *>(&rootType) + sizeof(WORD));
            versionInfo.insert(versionInfo.end(), reinterpret_cast<const BYTE *>(rootKey.c_str()), reinterpret_cast<const BYTE *>(rootKey.c_str() + rootKey.size() + 1));
            utils::PadToDword(versionInfo);
            versionInfo.insert(versionInfo.end(), reinterpret_cast<BYTE *>(&fixed), reinterpret_cast<BYTE *>(&fixed) + sizeof(fixed));
            versionInfo.insert(versionInfo.end(), stringFileInfo.begin(), stringFileInfo.end());
            versionInfo.insert(versionInfo.end(), varFileInfo.begin(), varFileInfo.end());

            std::vector<char> buffer(versionInfo.begin(), versionInfo.end());

            updateResource(16, 1, buffer);
        }
    };
}

int main()
{
    using ezi::builder::packager::ResourceUpdater;

    ResourceUpdater updater("ezi.exe");
    updater.updateVersionInfo({.companyName = L"EziApp Org.",
                               .fileDescription = L"Ezi Application",
                               .fileVersion = L"0.0.0",
                               .productName = L"EziApp",
                               .productVersion = L"0.0.0",
                               .fileVersionParts = {0, 0, 0, 0},
                               .productVersionParts = {0, 0, 0, 0}});
    updater.finalize();
    return 0;
}