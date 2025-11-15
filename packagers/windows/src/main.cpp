#include <iostream>
#include <windows.h>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <filesystem>

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
        int updateCount = 0;

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
            updateCount++;
            if (!UpdateResourceA(hUpdate, MAKEINTRESOURCEA(resourceType), MAKEINTRESOURCEA(resourceName), 1033, data.data(), data.size()))
                utils::ShowErrorAndExit("UpdateResource failed.");
        }

    public:
        void finalize()
        {
            if (updateCount == 0)
            {
                std::cout << "No resources were updated." << std::endl;
                return;
            }
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
            std::cout << "Updating asset..." << std::endl;
            std::ifstream file(filePath, std::ios::binary);
            if (!file)
                utils::ShowErrorAndExit("Failed to open asset file.");
            std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            updateResource(10, 1004, data);
        }
        void updateIcon(const std::string &iconPath)
        {
            std::cout << "Updating icon..." << std::endl;
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
            std::cout << "Updating version info..." << std::endl;
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

                if (!info.companyName.empty())
                    appendString(L"CompanyName", info.companyName);
                if (!info.fileDescription.empty())
                    appendString(L"FileDescription", info.fileDescription);
                if (!info.fileVersion.empty())
                    appendString(L"FileVersion", info.fileVersion);
                if (!info.productName.empty())
                    appendString(L"ProductName", info.productName);
                if (!info.productVersion.empty())
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

struct Option
{
    std::string name;
    std::string parameter;
    std::string description;
};

class AgrumentParser
{
private:
    int argc;
    char **argv;
    std::vector<Option> options{
        {"--help", "", "Show this help message"},
        {"--version", "", "Show version information"},
        {"--input", "<path>", "Specify the input executable path"},
        {"--icon", "<path>", "Specify the path to the icon file (.ico)"},
        {"--ezi-asset", "<path>", "Specify the path to the eziapp's asset file"},
        {"--update-version", "true", "Update version information"},
        {"--ver-companyName", "<name>", "Set the company name in version info"},
        {"--ver-fileDescription", "<description>", "Set the file description in version info"},
        {"--ver-fileVersion", "<version>", "Set the file version in version info"},
        {"--ver-productName", "<name>", "Set the product name in version info"},
        {"--ver-productVersion", "<version>", "Set the product version in version info"},
        {"--ver-fileVersionParts", "<x.x.x.x>", "Set the file version parts in version info"},
        {"--ver-productVersionParts", "<x.x.x.x>", "Set the product version parts in version info"},
    };

public:
    AgrumentParser(int argc, char **argv) : argc(argc), argv(argv) {}

public:
    void printHelp()
    {
        printVersion();
        std::cout << "Usage: packager [options]\nOptions:\n";
        size_t maxLen = 0;
        for (auto &opt : options)
        {
            std::string label = opt.parameter.empty() ? opt.name : (opt.name + " " + opt.parameter);
            if (label.size() > maxLen)
                maxLen = label.size();
        }

        for (auto &opt : options)
        {
            std::string label = opt.parameter.empty() ? opt.name : (opt.name + " " + opt.parameter);
            std::cout << "  " << std::left << std::setw(maxLen + 2) << label << opt.description << "\n";
        }
    }

    void printVersion()
    {
        std::cout << "EziApp Builder Packager Version 0.0.0\n";
    }

    std::string getOptionValue(const std::string &optionName)
    {
        for (int i = 1; i < argc; ++i)
        {
            if (std::string(argv[i]) == optionName && i + 1 < argc)
            {
                return std::string(argv[i + 1]);
            }
        }
        return "";
    }
};

int main(int argc, char *argv[])
{
    AgrumentParser parser(argc, argv);

    // 无参数
    if (argc < 2)
    {
        parser.printHelp();
        return 0;
    }

    // 帮助信息
    if (std::string(argv[1]) == "--help")
    {
        parser.printHelp();
        return 0;
    }

    // 版本信息
    if (std::string(argv[1]) == "--version")
    {
        parser.printVersion();
        return 0;
    }

    // 参数小于4个
    if (argc < 4)
    {
        std::cerr << "Insufficient arguments provided. Use --help for usage information." << std::endl;
        return EXIT_FAILURE;
    }

    // 输入程序文件
    std::string inputPath = parser.getOptionValue("--input");
    if (inputPath.empty())
    {
        std::cerr << "Input executable path is required." << std::endl;
        return EXIT_FAILURE;
    }

    if (std::filesystem::exists(inputPath) == false)
    {
        std::cerr << "Input executable file does not exist." << std::endl;
        return EXIT_FAILURE;
    }

    ezi::builder::packager::ResourceUpdater updater(inputPath);

    // 修改icon
    std::string iconPath = parser.getOptionValue("--icon");
    if (!iconPath.empty())
    {
        updater.updateIcon(iconPath);
    }

    // 修改ezi asset
    std::string assetPath = parser.getOptionValue("--ezi-asset");
    if (!assetPath.empty())
    {
        updater.updateAsset(assetPath);
    }

    // 修改版本信息

    auto updateVersionFlag = parser.getOptionValue("--update-version");
    if (updateVersionFlag == "true")
    {
        auto parseVersionParts = [](const std::string &versionStr, WORD parts[4])
        {
            std::istringstream iss(versionStr);
            std::string token;
            int index = 0;
            while (std::getline(iss, token, '.') && index < 4)
            {
                parts[index++] = static_cast<WORD>(std::stoi(token));
            }
            while (index < 4)
            {
                parts[index++] = 0;
            }
        };

        auto gbkToUtf16 = [](const std::string &gbk_str) -> std::wstring
        {
            int len = MultiByteToWideChar(CP_ACP, 0, gbk_str.c_str(), -1, nullptr, 0);
            std::wstring utf16_str(len - 1, L'\0');
            MultiByteToWideChar(CP_ACP, 0, gbk_str.c_str(), -1, &utf16_str[0], len);
            return utf16_str;
        };

        VersionInfo verInfo;
        verInfo.companyName = gbkToUtf16(parser.getOptionValue("--ver-companyName"));
        verInfo.fileDescription = gbkToUtf16(parser.getOptionValue("--ver-fileDescription"));
        verInfo.fileVersion = gbkToUtf16(parser.getOptionValue("--ver-fileVersion"));
        verInfo.productName = gbkToUtf16(parser.getOptionValue("--ver-productName"));
        verInfo.productVersion = gbkToUtf16(parser.getOptionValue("--ver-productVersion"));
        parseVersionParts(parser.getOptionValue("--ver-fileVersionParts"), verInfo.fileVersionParts);
        parseVersionParts(parser.getOptionValue("--ver-productVersionParts"), verInfo.productVersionParts);
        updater.updateVersionInfo(verInfo);
    }

    updater.finalize();
    return 0;
}