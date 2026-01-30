"""
PlatformIO pre-script to copy environment-specific data files
"""
Import("env")
import shutil
import os

# Get the project directory
project_dir = env.get("PROJECT_DIR")

# Get the current environment name
env_name = env.get("PIOENV")

# Define source data directories
data_sources = {
    "display_test": "data_display",
    "host_test": "data_host"
}

# Target data directory
data_dir = os.path.join(project_dir, "data")

# Clear and recreate data directory
if os.path.exists(data_dir):
    shutil.rmtree(data_dir)
os.makedirs(data_dir)

# Copy files from the appropriate source
if env_name in data_sources:
    source_dir = os.path.join(project_dir, data_sources[env_name])
    if os.path.exists(source_dir):
        print(f"Copying {data_sources[env_name]} -> data/ for {env_name}")
        for item in os.listdir(source_dir):
            src = os.path.join(source_dir, item)
            dst = os.path.join(data_dir, item)
            if os.path.isfile(src):
                shutil.copy2(src, dst)
                print(f"  Copied: {item}")
    else:
        print(f"Warning: Source directory {source_dir} not found")
else:
    print(f"No data directory configured for environment: {env_name}")
