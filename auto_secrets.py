# type: ignore
Import("env")
import os

project_dir = env.get("PROJECT_DIR")
secrets_file = os.path.join(project_dir, "include", "secrets.h")
example_file = os.path.join(project_dir, "include", "secrets.example.h")

if not os.path.exists(secrets_file) and os.path.exists(example_file):
    with open(example_file, "r") as f_ex:
        content = f_ex.read()
    with open(secrets_file, "w") as f_sec:
        f_sec.write(content)
    print("\n[INFO] Đã tự động tạo file include/secrets.h từ file mẫu cho bạn!")