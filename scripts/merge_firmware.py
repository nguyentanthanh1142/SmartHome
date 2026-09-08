import os
import platform
Import("env")

def merge_firmware(source, target, env):
    print("--------------------------------------------------")
    print("🔄 Merging ESP32 firmware binaries for Wokwi...")
    
    # Lấy đường dẫn tuyệt đối
    build_dir = env.subst("$BUILD_DIR")
    
    # Dùng thẳng môi trường Python của PlatformIO Core (có sẵn pyserial)
    python_exe = os.path.join(env.subst("$PLATFORMIO_CORE_DIR"), "penv", "Scripts", "python.exe" if platform.system() == "Windows" else "python")
    if not os.path.exists(python_exe):
        python_exe = env.subst("$PYTHONEXE")
        
    esptool = os.path.join(env.subst("$PROJECT_PACKAGES_DIR"), "tool-esptoolpy", "esptool.py")
    
    bootloader = os.path.join(build_dir, "bootloader.bin")
    partitions = os.path.join(build_dir, "partitions.bin")
    firmware = os.path.join(build_dir, "firmware.bin")
    merged = os.path.join(build_dir, "merged-firmware.bin")
    
    # Cố định thông số chuẩn cho ESP32 DevKit trên Wokwi (tránh lỗi định dạng '40000000L')
    flash_mode = "dio"
    flash_freq = "40m"
    flash_size = "4MB"
    
    # Thêm dấu ngoặc kép để xử lý đường dẫn có chứa khoảng trắng (OneDrive)
    cmd = f'"{python_exe}" "{esptool}" --chip esp32 merge_bin -o "{merged}" --flash_mode {flash_mode} --flash_freq {flash_freq} --flash_size {flash_size} 0x1000 "{bootloader}" 0x8000 "{partitions}" 0x10000 "{firmware}"'
    
    print(f"Executing command...")
    result = env.Execute(cmd)
    
    if result == 0:
        print("✅ Merged firmware successfully created!")
        print(f"📁 Path: {merged}")
        print("💡 Ensure wokwi.toml points to: .pio/build/esp32dev/merged-firmware.bin")
    else:
        print("❌ Failed to merge firmware. Check esptool output above.")
    print("--------------------------------------------------")

# Hook vào sự kiện sau khi build xong firmware
env.AddPostAction("$BUILD_DIR/firmware.bin", merge_firmware)