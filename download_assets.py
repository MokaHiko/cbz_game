import subprocess

# Install gdown if not present
subprocess.run(["pip", "install", "--quiet", "gdown"], check=True)

# Google Drive folder URL
url = "https://drive.google.com/drive/folders/1QPo3M0BOXFWkbgwmJBewhlqU2dHpxjHC?usp=drive_link"

# Output directory
output_dir = "assets"

import gdown

# Download the entire folder
gdown.download_folder(url, output=output_dir, quiet=False, use_cookies=False)

print("✅ Folder downloaded to:", output_dir)
