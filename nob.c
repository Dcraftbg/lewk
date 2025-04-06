#define NOB_IMPLEMENTATION
#include "nob.h"


#define SRCDIR "./kernel/src/"
#define OBJDIR "./int/kernel/"
const char* lew_sources[] = {
    SRCDIR "main.lew"
};
const char* nasm_sources[] = {
    SRCDIR "io.nasm"
};

#define lew_compiler(cmd) nob_cmd_append(cmd, "lewc")
#define lew_flags(cmd)
#define lew_output(cmd, output) nob_cmd_append(cmd, "-o", output)
bool lewc_compile_to_s(Nob_Cmd* cmd, const char* path, const char* opath) {
    lew_compiler(cmd);
    lew_flags(cmd);
    nob_cmd_append(cmd, path);
    lew_output(cmd, opath);
    return nob_cmd_run_sync_and_reset(cmd);
}
bool lewc_compile(Nob_Cmd* cmd, const char* path, const char* opath) {
    size_t temp = nob_temp_save();
    const char* s_path = nob_temp_sprintf("%s.s", opath);
    if(!lewc_compile_to_s(cmd, path, s_path)) {
        nob_temp_rewind(temp);
        return false;
    }
    nob_cmd_append(cmd, "as");
    nob_cmd_append(cmd, s_path);
    nob_cmd_append(cmd, "-o");
    nob_cmd_append(cmd, opath);
    bool res = nob_cmd_run_sync_and_reset(cmd);
    nob_temp_rewind(temp);
    return res;
}
bool nasm_assemble(Nob_Cmd* cmd, const char* path, const char* opath) {
    nob_cmd_append(cmd, "nasm", "-f", "elf64", path, "-o", opath);
    return nob_cmd_run_sync_and_reset(cmd);
}
bool link_kernel(Nob_Cmd* cmd, const char **objs, size_t objs_count) {
    nob_cmd_append(cmd, "ld");
    nob_cmd_append(cmd, "-g");
    nob_cmd_append(cmd, "-T");
    nob_cmd_append(cmd, "./kernel/link.ld");
    nob_da_append_many(cmd, objs, objs_count);
    nob_cmd_append(cmd, "-o");
    nob_cmd_append(cmd, "./bin/iso/kernel");
    return nob_cmd_run_sync_and_reset(cmd);
}
bool build_kernel(Nob_Cmd* cmd, Nob_File_Paths* objs) {
    if(!nob_mkdir_if_not_exists("int/kernel")) return false;
    for(size_t i = 0; i < NOB_ARRAY_LEN(lew_sources); ++i) {
        assert(strncmp(lew_sources[i], SRCDIR, strlen(SRCDIR)) == 0);
        const char* src = lew_sources[i] + strlen(SRCDIR);
        const char* obj = nob_temp_sprintf("%s%s.o", OBJDIR, src);
        if(!lewc_compile(cmd, lew_sources[i], obj)) return false;
        nob_da_append(objs, obj);
    }
    for(size_t i = 0; i < NOB_ARRAY_LEN(nasm_sources); ++i) {
        assert(strncmp(nasm_sources[i], SRCDIR, strlen(SRCDIR)) == 0);
        const char* src = nasm_sources[i] + strlen(SRCDIR);
        const char* obj = nob_temp_sprintf("%s%s.o", OBJDIR, src);
        if(!nasm_assemble(cmd, nasm_sources[i], obj)) return false;
        nob_da_append(objs, obj);
    }
    return true;
}
bool _copy_all_to(const char* to, const char** paths, size_t paths_count) {
    for(size_t i = 0; i < paths_count; ++i) {
        size_t temp = nob_temp_save();
        const char* path = nob_temp_sprintf("%s/%s",to, nob_path_name(paths[i]));
        if(!nob_file_exists(path) || nob_needs_rebuild1(path,paths[i])) {
            if(!nob_copy_file(paths[i],path)) {
                nob_temp_rewind(temp);
                return false;
            }
        }
        nob_temp_rewind(temp);
    }
    return true;
}
#define copy_all_to(to, ...) \
    _copy_all_to((to), \
                 ((const char*[]){__VA_ARGS__}), \
                 (sizeof((const char*[]){__VA_ARGS__})/sizeof(const char*)))


bool make_iso(Nob_Cmd* cmd) {
    nob_cmd_append(
        cmd,
        "xorriso",
        "-as", "mkisofs",
        "-b", "limine-bios-cd.bin",
        "-no-emul-boot",
        "-boot-load-size", "4",
        "-boot-info-table",
        "--efi-boot", "limine-uefi-cd.bin",
        "-efi-boot-part",
        "--efi-boot-image",
        "./bin/iso",
        "-o",
        "./bin/OS.iso"
    );
    return nob_cmd_run_sync_and_reset(cmd);
}
bool make_limine(void) {
    if(!copy_all_to(
        "./bin/iso",
        "./kernel/vendor/limine/limine-bios.sys", "./kernel/vendor/limine/limine-bios-cd.bin",
        "./kernel/vendor/limine/limine-uefi-cd.bin",
        "./kernel/limine.conf"
    )) return false;
    nob_log(NOB_INFO, "Copied limine");
    return true;
}
bool run_qemu(Nob_Cmd* cmd) {
    nob_cmd_append(
        cmd,
        "qemu-system-x86_64",
        "-device", "nec-usb-xhci,id=xhci",
        "--no-reboot",
        "--no-shutdown",
        "-d", "int",
        "-D", "qemu.log",
        "-smp", "2",
        "-m", "128",
        "-cdrom", "./bin/OS.iso",
        "-serial", "stdio",
    );
    return nob_cmd_run_sync_and_reset(cmd);
}
int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    Nob_Cmd cmd = { 0 };
    if(!nob_mkdir_if_not_exists("bin")) return 1;
    if(!nob_mkdir_if_not_exists("bin/iso")) return 1;
    if(!nob_mkdir_if_not_exists("int")) return 1;
    Nob_File_Paths objs = { 0 };
    if(!build_kernel(&cmd, &objs)) return 1;
    if(!link_kernel(&cmd, objs.items, objs.count)) return 1;
    if(!make_limine()) return 1;
    if(!make_iso(&cmd)) return 1;
    if(!run_qemu(&cmd)) return 1;
    return 0;
}
